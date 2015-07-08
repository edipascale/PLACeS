/* 
 * File:   TopologyOracle.hpp
 * Author: manuhalo
 *
 * Created on 25 giugno 2012, 10.36
 */

#ifndef TOPOLOGYORACLE_HPP
#define	TOPOLOGYORACLE_HPP

#include "Topology.hpp"
#include "SimTimeInterval.hpp"
#include "zipf_distribution.hpp"
#include <fstream>
#include "Cache.hpp"
#include "RankingTable.hpp"
#include <unordered_set>


/** Percentage of requests taking places at a given hour, starting from midnight. Used to model realistic usage patterns. */
const std::vector<double> usrPctgByHour = {
  4, 2, 1.5, 1.5, 1, 0.5, 0.5, 1, 1, 1.5, 2, 2.5, 3, 4, 4.5, 4.5, 4, 4, 4, 6, 12, 15, 12, 8
//0  1   2    3   4   5    6   7  8   9   10  11 12 13  14   15  16 17 18 19  20  21  22  23
};
/** Weight of the days of the week when determining request scheduling (Monday to Sunday).
 * Only used for the VoDTopologyOracle.
 * @deprecated */
const double dayWeights[] = {0.8, 0.9, 1, 0.8, 1.2, 1.3, 1.2};
//                            M    T   W   T    F    S    S

/** Length of a watching session for an individual video element; currently 50% of the time the user requests the whole item, and the other 50% a decimal fraction of the content is requested (i.e., one-tenth, two-tenths etc.). */
const std::vector<double> sessionLength = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1,
        1, 1, 1, 1, 1, 1, 1, 1}; // 50% linear zapping, 50% entire content 

typedef Cache<ChunkPtr, Capacity, SimTime> ChunkCache;
typedef std::map<ChunkPtr, std::set<PonUser> > ChunkMap;
typedef std::map<uint, ChunkMap> AsidContentMap;
typedef std::map<PonUser, ChunkCache> UserCacheMap;
typedef std::map<Vertex, ChunkCache> LocalCacheMap;

/**
 * This struct condenses statistical measures of a number of metrics related
 * to the current simulation. Each vector has as many elements as the number
 * of rounds of the simulation; at the end of each round the appropriate 
 * statistics for the current round are measured and recorded, so that they can 
 * be printed to file at the very end of the simulation.
 */
struct FlowStats {
  std::vector<double> avgFlowDuration; /**< Average length of a data flow in seconds. */
  std::vector<double> avgPeerFlowDuration; /**< Average length of a P2P data flow in seconds. */
  std::vector<double> avgCacheFlowDuration; /**< Average length of a data flow from a CDN cache in seconds. */
  std::vector<float> avgUserCacheOccupancy; /**< Average time-weighted occupancy of a user cache. @see Cache */
  std::vector<float> avgASCacheOccupancy; /**< Average time-weighted occupancy of a CDN cache. @see Cache */
  std::vector<uint> servedRequests; /**< Number of content requests that were served. */
  std::vector<uint> localRequests; /**< Number of content requests that were served locally, i.e., without leaving the Access Section (AS) of the requesting user. */
  std::vector<uint> completedRequests; /**< Number of content requests that were completed (i.e., not blocked or suspended due to the expiration of the content that was being accessed). */ 
  std::vector<uint> fromASCache; /**< Number of content requests that were served by CDN caches. */
  std::vector<uint> fromPeers; /**< Number of content requests that were served by a P2P source. */
  std::vector<uint> fromCentralServer; /**< Number of content requests that were served by the central repository. */
  std::vector<uint> congestionBlocked; /**< Number of content requests that were blocked due to link congestion. */
  std::vector<uint> cacheOptimized; /**< Number of content requests for which the storage optimization algorithm succeeded. @see TopologyOracle::optimizeCaching() */
};

/**
 * UserWatchingInfo is the one-stop shop for everything related to the current
 * watching session of a customer, from the total viewing session interval to
 * the chunk currently being watched. It also manages the buffer where the pre-
 * fetched chunks are stored waiting to be consumed.
 */
class UserWatchingInfo {
public:
  SimTimeInterval dailySessionInterval; /**< Continuous video watching session for the current round. */
  ContentElement* content; /**< ContentElement currently being watched. */
  uint currentChunk; /**< ID of the chunk currently being watched (or of the one we are waiting for). */
  uint highestChunkFetched; /**< ID of the highest sequential chunk that has been fetched so far. */
  /** time at which the user will change content (either due to zapping, because
   * the content has been completed, or because the watching session ended) */
  uint chunksToBeWatched; 
  bool waiting; /**< If true, the user cannot progress with its watching as it is waiting for a chunk to be downloaded (i.e., due to a rebuffering event). */
  std::unordered_set<ChunkPtr> buffer; /**< Represents the buffer in which the chunks downloaded from a source are kept before they are consumed by the user. @see TopologyOracle::bufferSize */
  
  UserWatchingInfo() : dailySessionInterval(0,1) {
    content = nullptr;
    currentChunk = 0;
    highestChunkFetched = 0;
    chunksToBeWatched = 0;
    waiting = false;
  }
  
  UserWatchingInfo(const SimTimeInterval interval) : dailySessionInterval(interval) {
    content = nullptr;
    currentChunk = 0;
    highestChunkFetched = 0;
    chunksToBeWatched = 0;
    waiting = false;
  }
  
  void reset() {
    //dailySessionInterval = SimTimeInterval(0,0);
    content = nullptr;
    currentChunk = 0;
    highestChunkFetched = 0;
    chunksToBeWatched = 0;
    buffer.clear();
    waiting = false;
  }
};
typedef std::map<PonUser, UserWatchingInfo> UserWatchingMap;

/**
 * Locality Oracle for the current Topology.
 * 
 * The TopologyOracle keeps track of what everyone is caching and uses that
 * information to match requesters to sources with the appropriate content.
 * Essentially it serves as a global tracker (in the BitTorrent sense).
 * We keep both a map of all the user caching a particular content (to fetch
 * potential sources quickly) and a map of what each individual user is caching
 * (to be able to simulate finite-size caching).
 * 
 * This is an abstract class, which acts as the aggregator for the common
 * functionalities of the two derived classes IPTVTopologyOracle and 
 * VodTopologyOracle. The appropriate methods are overridden in the derived
 * classes to specialize their behaviour. 
 */
class TopologyOracle{
protected:
  SimMode mode;   /**< Specifies which kind of popularity model is being used, i.e., VoD or IPTV */
  uint contentNum; /**< Number of contents in the catalog. */
  double avgContentLength; /**< Average length in minutes of a ContentElement. */
  double devContentLength; /**< Standard deviation of the length of a ContentElement. */
  double avgHoursPerUser; /**< Average hours of viewing per user per round. */
  double avgReqLength; /**<  Average length of a ContentElement request. Derivative information, but useful for the popularity estimation algorithm. */
  uint peakReqRatio; /**< Multiplicative factor used to determine peak requests from average requests. */
  uint bitrate; /**< Bitrate of the encoded content in Mbps. */
  Topology* topo; /**< Pointer to the Topology being used for this simulation. */
  AsidContentMap* asidContentMap; /**< A map keeping track of the items available in each Access Section (AS). */
  UserCacheMap* userCacheMap; /**< A map keeping track of the items available at each user. */
  LocalCacheMap* localCacheMap; /**< A map keeping track of the items available in each CDN cache. */
  uint ponCardinality; /**< Number of users per PON. */
  CachePolicy policy; /**< Cache content replacement policy. @see CachePolicy */
  uint maxCacheSize; /**< Maximum size of user caches. */
  uint maxLocCacheSize; /**< Maximum size of CDN caches. */
  FlowStats flowStats; /**< Struct grouping miscellaneous statistics on the current simulation results. @see FlowStats */
  bool reducedCaching; /**< If true, use only a single CDN in the core; otherwise, place a CDN cache in each Access Section (AS). */
  bool preCaching;  /**< @deprecated If true, CDN caches are pre-filled with popular content and never updated. */
  uint maxUploads;  /**< Maximum number of concurrent uploads per PON tree. Used for the cache optimization problem. @see TopologyOracle::optimizeCaching() */
  std::vector< std::vector<double> > contentRateVec; /**< A vector which associates to each release day and popularity rank the number of requests that the oracle expects to observe per user per day. */
  std::vector<RankingTable<ContentElement*> > dailyRanking; /**< A bimap-based container to keep track of the dynamic evolution of content popularity. */
  uint roundDuration; /**< Length of a simulation round in seconds. */
  bool cachingOpt; /**< If true, attempts to optimize the storage space utilization of the user caches. @see TopologyOracle::optimizeCaching() */
  
  uint chunkSize; /**< size of a Chunk in Megabits. Note that the last chunk of a ContentElement can be smaller than this. */
  uint bufferSize; /**< Number of chunks that can be prefetched in the user buffer for streaming purposes, once a content has been requested. */
  
  /** 
   * This map stores, for each user in the network, the corresponding 
   * information on its streaming session, including the beginning and end
   * instants of his consecutive video watching session for the current
   * round (as extracted from the user behaviour distributions), the content
   * it is currently streaming, its fetching buffer, the id of the chunk it is
   * watching, the id of the highest chunk it has fetched so far. 
   * @see UserWatchingMap
   */
  UserWatchingMap userWatchMap;
  
public:
  TopologyOracle(Topology* topo, po::variables_map vm, uint roundDuration);
  ~TopologyOracle();
  
  /**
   * Serves a content request by finding an appropriate source for it.
   * 
   * This method attempts to serve a request by identifying a viable source with
   * the desired content. In order of preference, the TopologyOracle will attempt
   * to redirect the request to:
   *   1. the requester's cache, if the item had already been cached previously;
   *   2. a local PonUser, i.e., a user belonging to the same Access Section (AS) of the requester;
   *   3. the local CDN cache, if present;
   *   4. a non-local PonUser, i.e., a user belonging to a different AS than the
   *      one of the requester. Note that sources at a lower distance from the
   *      requester (in terms of core hop count) are prioritized over sources 
   *      further away;
   *   5. the central repository.
   * In each case, the selected source is only picked if there is enough capacity
   * in the route between itself and the destination to at least stream the content
   * at its encoding bitrate.
   * 
   * @param flow The Flow related to the request which is to be served.
   * @param scheduler the Scheduler, which is needed to queue the Flow once an appropriate source has been selected.
   * @return True if a viable source could be found, false otherwise.
   */
  bool serveRequest(Flow* flow, Scheduler* scheduler);
  
  /**
   * Adds a chunk to a user cache.
   * 
   * This method updates the content of a user's cache, and updates the relevant
   * maps in the TopologyOracle to mirror this change.
   * 
   * @param user The user that owns the cache to be updated.
   * @param chunk The chunk to be cached at the user.
   * @param time The current simulation time.
   */
  void addToCache(PonUser user, ChunkPtr chunk, SimTime time);
  
  /**
   * Resets all user caches, emptying them.
   */
  void clearUserCache();
  /**
   * Resets all CDN caches, emptying them.
   */
  void clearLocalCache();
  
  /**
   * Retrieves the set of local sources for a given chunk.
   * 
   * This utility method returns a std::set of PonUsers residing in Access Section
   * (AS) asid and who are currently caching a copy of chunk.
   * @param chunk The chunk we are looking for sources of.
   * @param asid The id of the AS in which the sources should reside.
   * @return A std::set of PonUsers local to AS asid and with a copy of chunk.
   */
  std::set<PonUser> getSources(const ChunkPtr& chunk, uint asid);
  
  /**
   * Performs all the required post-completion operations on a Flow.
   * 
   * This method should only be called on flows of FlowType::TRANSFER or 
   * FlowType::WATCH. If the completed Flow was a transfer, the method will update
   * the relevant statistics, add the downloaded chunk to the user streaming buffer,
   * and, if the destination was waiting on this particular chunk, schedule
   * a new watching Flow. It will also add the chunk to the destination cache,
   * conditionally to the result of TopologyOracle::optimizeCaching if the
   * simulation has the caching optimization enabled.
   * If the completed Flow was a watching one, the chunk which has just been
   * consumed is deleted from the buffer, and if the viewing session has not
   * terminated a number of new chunks are pre-fetched to fill the buffer. 
   * Finally, if the next chunk is available in the buffer, a new watching Flow
   * for that chunk is scheduled, otherwise the user marks itself as waiting.
   * 
   * @param flow The Flow that has just been completed.
   * @param scheduler A pointer to the Scheduler, which is needed to schedule new Flows.
   */
  void notifyCompletedFlow(Flow* flow, Scheduler* scheduler);
  
  /**
   * Maintenance method called at the end of a round.
   * 
   * Performs basic inter-round maintenance operations such as resetting the
   * load statistics on topology links and updating the observed rate of requests
   * for content in the catalog.
   * @param endingRound The round that is just about to finish.
   */
  void notifyEndRound(uint endingRound);
  
  /**
   * A deprecated method to retrieve the Topology from the TopologyOracle.
   * 
   * This method is currently used in the Scheduler to perform some cleanup
   * operations when dealing with Flows whose ContentElement has expired
   * (i.e., after midnight of the 7th day since their release). It's a hack
   * and it should not be used, but it's a handy workaround for the time being.
   * @deprecated
   * @return A pointer to the Topology used in this simulation.
   */
  Topology* getTopology() {return topo;}
  
  /**
   * Prints to screen various statistics on the simulation round that just ended.
   * @param currentRound The simulation round that just ended.
   */
  void printStats(uint currentRound);
  
  /**
   * Adds a new ContentElement to the catalog.
   * 
   * This method is called when a new ContentElement is created, so that it can
   * be added to the catalog and that the relative entries in the content tracking
   * maps can be generated.
   * @param content A pointer to the ContentElement which was just created.
   * @param elapsedRounds The number of round elapsed so far in this simulation.
   */
  void addContent(ContentElement* content, uint elapsedRounds);
  
  /**
   * Removes a ContentElement from the catalog.
   * 
   * This method is called after a ContentElement has expired, i.e., the set
   * amount of days (7) has passed since its day of release. All the cached
   * copies of this content item are erased and the entries in the cache maps
   * are deleted.
   * 
   * @param content The ContentElement to be removed from the catalog.
   * @param roundsElapsed The number of simulation rounds elapsed since the beginning.
   */
  void removeContent(ContentElement* content, uint roundsElapsed);
  
  /**
   * Checks whether chunk is cached at the specified user cache.
   * @param user The PonUser whose cache content we are checking.
   * @param chunk The chunk we are looking for.
   * @return True if chunk is cached by user, false otherwise.
   */
  bool checkIfCached(PonUser user, const ChunkPtr& chunk);
  
  /**
   * Checks whether chunk is cached at the specified CDN cache.
   * @param lCache The CDN cache that we are checking.
   * @param chunk The chunk we are looking for.
   * @return True if chunk is cached by lCache, false otherwise.
   */
  bool checkIfCached(Vertex lCache, const ChunkPtr& chunk);
  
  /**
   * Updates LFU/LRU stats when content is grabbed directly from the local CDN cache.
   * @param lCache The CDN cache we are fetching content from.
   * @param chunk The chunk that is being fetched.
   * @param time The current SimTime, required to update the LRU metadata.
   */
  void getFromLocalCache(Vertex lCache, const ChunkPtr& chunk, SimTime time);
  
  /**
   * Removes user from the content map for chunk.
   * 
   * This method should be called after chunk has been deleted from user's cache,
   * to update the map that matches chunks with local sources.
   * @param chunk The chunk that has just been deleted.
   * @param user The user that should be removed from the content map.
   */
  void removeFromCMap(const ChunkPtr& chunk, PonUser user);
  
  /**
   * Method to optimize the storage utilization of user caches.
   * 
   * This method implements the optimal caching algorithm to minimize storage
   * space while keeping high level of locality, by ensuring that an optimal
   * number of replicas are always available, based on  popularity estimation. 
   * the function should be called before adding elements to the cache, as it 
   * determines whether or not the new element is worth storing and which 
   * elements should be erased to make space for it. 
   * 
   * @param user The user whose cache we are trying to optimize.
   * @param chunk The chunk that user has just finished downloading.
   * @param time The current simulation time, used to calculate the hour of the day.
   * @param currentRound The current simulation round, used to compute the age of the cached content (i.e., the days elapsed since their first release).
   * 
   * @return A std::pair<bool,bool>, the first of which tells the caller whether
   * the optimization was successful (i.e., a valid solution was found), and the
   * second telling whether the requested element should be cached. The second
   * boolean value should only be taken into consideration if the first is true.
   */
  std::pair<bool, bool> optimizeCaching(PonUser user, const ChunkPtr& chunk, 
      SimTime time, uint currentRound);
  
  /**
   * Print a graphml snapshot of the current status of the network.
   * 
   * This method is invoked by the Scheduler when a FlowType::SNAPSHOT is encountered.
   * The real work is done inside the Topology class.
   * @param time Current simulation time.
   * @param round Current simulation round.
   */
  void takeSnapshot(SimTime time, uint round) const {
    this->topo->printTopology(time, round);
  }
  
  FlowStats getFlowStats() const {
    return this->flowStats;
  }
  // The next methods are virtualized so that VoD and IPTV can re-implement them
  
  /** 
   * Generates content requests for all the elements in the catalog for the current round, 
   * then schedules those requests as flows in the Scheduler. Overridden in the 
   * specialized subclasses.
   * 
   * @param scheduler A pointer to the Scheduler, required to queue new Flows.
   */
  virtual void generateUserViewMap(Scheduler* scheduler) = 0;
  
  /**
   * Instantiates the ContentElements that compose the catalog (plus their relative chunks)
   * and stores them in an appropriate data structure.
   */
  virtual void populateCatalog() = 0;

  /** 
   * performs general maintenance on the catalog at the end of
   * a round (e.g., removing expired content items, generating new ones etc.)
   * @param currentRound The current simulation round.
   */
  virtual void updateCatalog(uint currentRound) = 0;
  
  /** 
   * Generates a single content request for a specific user. It's only used in 
   * the IPTV model, as in the VoD one requests are pre-determined at the 
   * beginning of the simulation round.   
   * @param user The user on behalf of which the new request has to be generated.
   * @param time The current simulation time.
   * @param scheduler A pointer to the Scheduler so that the new request Flows can be queued.
   */
  virtual void generateNewRequest(PonUser user, SimTime time, 
                                      Scheduler* scheduler) = 0;
 
  /**
   * Pushes the most popular content in the AS caches. It must be
   * ovveridden by the specialized classes as determining the most popular 
   * elements of the catalog depends on the simulation mode.
   * @deprecated
   */
  virtual void preCache() = 0;
};
       
#endif	/* TOPOLOGYORACLE_HPP */

