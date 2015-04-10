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


// % of requests taking places at a give hour, starting from midnight
const std::vector<double> usrPctgByHour = {
  4, 2, 1.5, 1.5, 1, 0.5, 0.5, 1, 1, 1.5, 2, 2.5, 3, 4, 4.5, 4.5, 4, 4, 4, 6, 12, 15, 12, 8
//0  1   2    3   4   5    6   7  8   9   10  11 12 13  14   15  16 17 18 19  20  21  22  23
};
// weight of the days of the week when determining request scheduling (M->S)
const double dayWeights[] = {0.8, 0.9, 1, 0.8, 1.2, 1.3, 1.2};
//                            M    T   W   T    F    S    S

const std::vector<double> sessionLength = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1,
        1, 1, 1, 1, 1, 1, 1, 1}; // 50% linear zapping, 50% entire content 

typedef Cache<ChunkPtr, Capacity, SimTime> ChunkCache;
typedef std::map<ChunkPtr, std::set<PonUser> > ChunkMap;
typedef std::map<uint, ChunkMap> AsidContentMap;
typedef std::map<PonUser, ChunkCache> UserCacheMap;
typedef std::map<Vertex, ChunkCache> LocalCacheMap;


struct FlowStats {
  std::vector<double> avgFlowDuration;
  std::vector<double> avgPeerFlowDuration;
  std::vector<double> avgCacheFlowDuration;
  std::vector<float> avgUserCacheOccupancy;
  std::vector<float> avgASCacheOccupancy;
  std::vector<uint> servedRequests;
  std::vector<uint> localRequests;
  std::vector<uint> completedRequests;
  std::vector<uint> fromASCache;
  std::vector<uint> fromPeers;
  std::vector<uint> fromCentralServer;
  std::vector<uint> congestionBlocked;
  std::vector<uint> cacheOptimized;
};

/* UserWatchingInfo is the one-stop shop for everything related to the current
 * watching session of a customer, from the total viewing session interval to
 * the chunk currently being watched. It also manages the buffer where the pre-
 * fetched chunks are stored waiting to be consumed.
 */
class UserWatchingInfo {
public:
  SimTimeInterval dailySessionInterval; // continuous watching session for the round
  ContentElement* content; // content being watched
  uint currentChunk; // chunk currently being watched
  uint highestChunkFetched; // ID of the highest sequential chunk to have been fetched
  /* time at which the user will change content (either due to zapping, because
   * the content has been completed. or because the watching session ended)  
   */
  SimTime watchingEndTime; 
  bool waiting; // is the user waiting for a chunk to be downloaded (i.e. rebuffering?)
  std::unordered_set<ChunkPtr> buffer; // stores chunks for streaming
  
  UserWatchingInfo(const SimTimeInterval interval) : dailySessionInterval(interval) {
    content = nullptr;
    currentChunk = 0;
    highestChunkFetched = 0;
    watchingEndTime = 0;
    waiting = false;
  }
  
  void reset() {
    //dailySessionInterval = SimTimeInterval(0,0);
    content = nullptr;
    currentChunk = 0;
    highestChunkFetched = 0;
    watchingEndTime = 0;
    buffer.clear();
    waiting = false;
  }
};
typedef std::map<PonUser, UserWatchingInfo> UserWatchingMap;

/* The TopologyOracle keeps track of what everyone is caching and uses that
 * information to match requesters to sources with the appropriate content.
 * Essentially it serves as a global tracker (in the BitTorrent sense).
 * We keep both a map of all the user caching a particular content (to fetch
 * potential sources quickly) and a map of what each individual user is caching
 * (to be able to simulate finite-size caching).
 */
class TopologyOracle{
protected:
  SimMode mode;   // VoD or IPTV
  uint contentNum; // # of contents in the catalog
  double avgContentLength; //average content length in minutes
  double devContentLength; // std deviation of content length
  double avgHoursPerUser; // hours of viewing per user per round
  double avgReqLength; //  derivative, but useful for the popularity estimation
  uint peakReqRatio; // multiplicative factor to determine peak requests from avg requests
  uint bitrate; // bitrate of the encoded content in Mbps
  Topology* topo;
  AsidContentMap* asidContentMap; // keeps track of content available in each AS
  UserCacheMap* userCacheMap; // keeps track of content available at each user
  LocalCacheMap* localCacheMap; // keeps track of content available in each CDN cache
  uint ponCardinality; // users per PON
  CachePolicy policy; // cache content replacement policy
  uint maxCacheSize;
  uint maxLocCacheSize;
  FlowStats flowStats;
  bool reducedCaching; // if true, use only a single CDN in the core;
                       // if false, one CDN micro cache in each AS
  bool preCaching;  // if true, AS caches are pre-filled with popular content and never updated
  uint maxUploads;  // max number of concurrent uploads for the optimization problem
  /* the following vector associates to each day of release and rank the expected 
   * number of requests that the oracle expects per user per day.
   */
  std::vector< std::vector<double> > contentRateVec;
  // bimap-based container to keep track of content popularity
  std::vector<RankingTable<ChunkPtr> > dailyRanking;
  uint roundDuration;
  bool cachingOpt;
  
  /* new members that are required for the chunking mechanisms:
   * chunkSize is the size of each chunk in Megabits
   * bufferSize is the number of chunks that can be stored in the streaming 
   * buffer for each user
   */
  uint chunkSize; // in Mbits
  uint bufferSize; // in # of chunks
  
  /* userWatchMap stores, for each user in the network, the corresponding 
   * information on its streaming session, including the beginning and end
   * instants of his consecutive video watching session for the current
   * round (as extracted from the user behaviour distributions), the content
   * it is currently streaming, its fetching buffer, the id of the chunk it is
   * watching, the id of the highest chunk it has fetched so far.
   */
  UserWatchingMap userWatchMap;
  
public:
  TopologyOracle(Topology* topo, po::variables_map vm, uint roundDuration);
  ~TopologyOracle();
  bool serveRequest(Flow* flow, Scheduler* scheduler);
  void addToCache(PonUser user, ChunkPtr chunk, SimTime time);
  void clearUserCache();
  void clearLocalCache();
  std::set<PonUser> getSources(ChunkPtr chunk, uint asid);
  void notifyCompletedFlow(Flow* flow, Scheduler* scheduler);
  void notifyEndRound(uint endingRound);
  // this is a hack and should be removed
  Topology* getTopology() {return topo;}
  void printStats(uint currentRound);
  void addContent(ContentElement* content, uint elapsedRounds);
  void removeContent(ContentElement* content, uint roundsElapsed);
  bool checkIfCached(PonUser user, ChunkPtr chunk);
  bool checkIfCached(Vertex lCache, ChunkPtr chunk);
  void getFromLocalCache(Vertex lCache, ChunkPtr chunk, SimTime time);
  void removeFromCMap(ChunkPtr chunk, PonUser user);
  /* optimizeCaching implements the optimal caching algorithm to minimize storage
   * space while keeping high level of locality, by ensuring that some minimal
   * number of replicas are always available. Replicas are determined based on 
   * popularity estimation. the function should be called 
   * before adding elements to the cache, as it determines whether or not the 
   * new element is worth storing and which elements should be erased to make
   * space for it. It returns a pair of bool, the first of which tells whether
   * the optimization was successful (i.e., a valid solution was found), and the
   * second telling whether the requested element should be cached. The second
   * boolean value should only be taken into consideration if the first is true.
   */
  std::pair<bool, bool> optimizeCaching(PonUser user, ChunkPtr chunk, 
      SimTime time, uint currentRound);
  
  void takeSnapshot(SimTime time, uint round) const {
    this->topo->printTopology(time, round);
  }
  
  FlowStats getFlowStats() const {
    return this->flowStats;
  }
  // The next methods are virtualized so that VoD and IPTV can reimplement them
  
  /* generateUserViewMap() generates content requests for all the elements
   * of the catalog for the current round, then schedules those requests as flows
   * in the scheduler. 
   */
  virtual void generateUserViewMap(Scheduler* scheduler) = 0;
  /* populateCatalog() instantiates the ContentElement that compose the catalog
   * and stores them in an appropriate data structure.
   */
  virtual void populateCatalog() = 0;
  /* updateCatalog() performs general maintenance on the catalog at the end of
   * a round (e.g. removing expired content, generating new one etc.)
   */
  virtual void updateCatalog(uint currentRound) = 0;
  /* generateNewRequest() is a legacy method that generates a single request for
   * a specific user. It's only used in the IPTV model and it's being kept
   * for "backward compatibility". It should be removed in the future.
   */
  virtual void generateNewRequest(PonUser user, SimTime time, 
                                      Scheduler* scheduler) = 0;
  /* preCache() pushes the most popular content in the AS caches. It must be
   * ovveridden by the specialized classes as determining the most popular 
   * elements of the catalog depends on the simulation mode
   */
  virtual void preCache() = 0;
};
       
#endif	/* TOPOLOGYORACLE_HPP */

