#include "TopologyOracle.hpp"
#include "Scheduler.hpp"
#include "boost/random/discrete_distribution.hpp"
#include "boost/random/uniform_int.hpp"
#include <boost/lexical_cast.hpp>
#include <initializer_list>
#include <boost/random/mersenne_twister.hpp>
#include <cmath>
#define ILOUSESTL
#define IL_STD
#include "ilcplex/ilocplex.h"

// random generator
extern boost::mt19937 gen;

TopologyOracle::TopologyOracle(Topology* topo, po::variables_map vm, uint roundDuration) {
  this->roundDuration = roundDuration;
  this->cachingOpt = vm["optimize-caching"].as<bool>();
  this->ponCardinality = vm["pon-cardinality"].as<uint>();
  this->policy = (CachePolicy) vm["cache-policy"].as<uint>();
  this->maxCacheSize = vm["ucache-size"].as<uint>() * 8000; // input is in GB, variable in Mb
  this->maxLocCacheSize = vm["lcache-size"].as<uint>()  * 8000; // input is in GB, variable in Mb;
  this->reducedCaching = vm["reduced-caching"].as<bool>();
  this->topo = topo;
  this->mode = (SimMode) vm["sim-mode"].as<uint>();
  this->avgHoursPerUser = vm["avg-hours-per-user"].as<double>();
  this->avgContentLength = vm["content-length"].as<double>(); // in minutes
  this->devContentLength = vm["content-dev"].as<double>();
  this->peakReqRatio = vm["peak-req-ratio"].as<uint>();
  this->bitrate = vm["bitrate"].as<uint>();
  this->preCaching = vm["pre-caching"].as<bool>();
  //FIXME: chunkSize is integer here but we compare it with sizeDownloaded which is double
  this->chunkSize = vm["chunk-size"].as<uint>();
  this->bufferSize = vm["buffer-size"].as<uint>();
  //FIXME: this assumes constant bitrate for upload and a 10GPON
  this->maxUploads = std::floor(10240 / (ponCardinality * bitrate));
  this->avgReqLength = (avgContentLength * 60 *  // in seconds
          std::accumulate(sessionLength.begin(), sessionLength.end(), 0)/sessionLength.size());
  uint rounds = vm["rounds"].as<uint>();
  flowStats.avgFlowDuration.assign(rounds, 0);
  flowStats.avgPeerFlowDuration.assign(rounds, 0);
  flowStats.avgCacheFlowDuration.assign(rounds, 0);
  flowStats.avgUserCacheOccupancy.assign(rounds, 0);
  flowStats.avgASCacheOccupancy.assign(rounds, 0);
  flowStats.completedRequests.assign(rounds, 0);
  flowStats.localRequests.assign(rounds, 0);
  flowStats.servedRequests.assign(rounds, 0);
  flowStats.fromASCache.assign(rounds, 0);
  flowStats.fromPeers.assign(rounds, 0);
  flowStats.fromCentralServer.assign(rounds, 0);
  flowStats.congestionBlocked.assign(rounds, 0);
  flowStats.cacheOptimized.assign(rounds, 0);
  this->userCacheMap = new UserCacheMap;
  this->asidContentMap = new AsidContentMap;
  for (uint i = 0; i < topo->getNumASes(); i++) {
    ChunkMap map;
    asidContentMap->insert(std::make_pair(i,map));
  }
  
  // Initialize user cache map
    VertexVec ponNodes = topo->getPonNodes();
    BOOST_FOREACH(Vertex v, ponNodes) {
    for(uint i = 0; i < topo->getPonCustomers(v); i++) {
        PonUser user = std::make_pair(v, i);
        ChunkCache cache(maxCacheSize, policy);
        userCacheMap->insert(std::make_pair(user, cache));
      }
    }
  // Initialize local cache nodes
  this->localCacheMap = new LocalCacheMap;
  if (!reducedCaching) {    
    VertexVec cacheNodes = topo->getLocalCacheNodes();
    BOOST_FOREACH(Vertex v, cacheNodes) {
      ChunkCache cache(maxLocCacheSize, policy);
      localCacheMap->insert(std::make_pair(v, cache));
    }
  } else {
    Vertex v = topo->getCentralServer();
    ChunkCache cache(maxLocCacheSize, policy);
    localCacheMap->insert(std::make_pair(v, cache));
  }
  
  //Initialize dailyRanking
  RankingTable<ContentElement*> ranking;
  dailyRanking.resize(7, ranking);
  
}

TopologyOracle::~TopologyOracle() {
  UserCacheMap::iterator ucit;
  for (ucit = userCacheMap->begin(); ucit != userCacheMap->end(); ucit++) {
    ucit->second.clearCache();
  }
  userCacheMap->clear();
  delete this->userCacheMap;
  asidContentMap->clear();
  delete this->asidContentMap;
  localCacheMap->clear();
  delete this->localCacheMap;
  dailyRanking.clear();
}

void TopologyOracle::addToCache(PonUser user, ChunkPtr chunk, SimTime time) {
  if (!chunk) {
    BOOST_LOG_TRIVIAL(error) << "TopologyOracle::addToCache() - invalid ChunkPtr";
    abort();
  }
  ContentElement* content = chunk->getContent();
  uint chunkIndex = chunk->getIndex();
  // update the chunkMap for content retrieval
  uint asid = topo->getAsid(user);
  ChunkMap::iterator cIt = asidContentMap->at(asid).find(chunk);
  std::pair<bool, std::set<ChunkPtr> > addResult;
  if (cIt == asidContentMap->at(asid).end()) {
    // This is the first time the oracle sees this chunk, add it to the map
    // Note: this should not happen now, throw an error
    BOOST_LOG_TRIVIAL(error) << time << ":ERROR: TopologyOracle::addToCache() - "
            "unknown chunk for content " << content->getName() << 
            ", chunkId " << chunkIndex;
    abort();
    // std::set<PonUser> sourceNodes;
    // cIt = asidContentMap->at(asid).insert(std::make_pair(content, sourceNodes)).first;
  }
  // update userCacheMap according to the specified cache policy
  BOOST_LOG_TRIVIAL(trace) << time << ": caching chunk " << 
          chunk->getIndex() << " of content " << content->getName() << 
          " at User " << user.first << "," << user.second;
  addResult = userCacheMap->at(user).addToCache(chunk, chunk->getSize(), time);
  if (!addResult.first) {
    if (userCacheMap->at(user).getMaxSize() >= chunk->getSize())
      BOOST_LOG_TRIVIAL(warning) << time << ": WARNING: failed to cache content " 
            << content->getName() 
            << " at User "  << user.first << "," << user.second;
  } else {
   /* if caching at the user was successful, add the user to the asidCacheMap
    * for its AS (cIt is the iterator to the right position in the map, see above)
    * note that the user might be already there if he had a copy of the content
    * with a different size
    */
    if (cIt->second.find(user) == cIt->second.end()) {
      bool insert = cIt->second.insert(user).second;
      if (!insert) {
        BOOST_LOG_TRIVIAL(warning) << time << ": WARNING: failed to insert User " 
                << user.first << "," << user.second << " as a source for chunk "
                << chunk->getIndex() << " of content "
                << content->getName() << " in AS " << asid;
      }
    }
  }
  BOOST_FOREACH (ChunkPtr e, addResult.second) {
    // erase the content from the ContentMap entry of the user
    this->removeFromCMap(e, user);
    // update the asidContentMap too
    cIt = asidContentMap->at(asid).find(e);
    if (cIt != asidContentMap->at(asid).end())
      cIt->second.erase(user);
  }
 
  if (!reducedCaching && !preCaching) {
    // also update the local Cache associated to this user ASID
    // (the flow is not simulated as it is assumed it was cached as it transited
    // towards the user)
    Vertex lCache = topo->getLocalCache(user.first);
    if (!localCacheMap->at(lCache).isCached(chunk)) {
      // debug info
      BOOST_LOG_TRIVIAL(trace) << time << ": caching chunk " << 
          chunk->getIndex() << " of content " << content->getName() <<
          " at local cache " << lCache;
      addResult = localCacheMap->at(lCache).addToCache(chunk, chunk->getSize(), time);
      if (!addResult.first) {
        BOOST_LOG_TRIVIAL(warning) << time << ": WARNING: failed to cache caching chunk " << 
                chunk->getIndex() << " of content " << content->getName() 
                << " at AS cache "  << lCache;
      }
    }
  }
}

void TopologyOracle::clearUserCache() {
  // clear the actual caches
  UserCacheMap::iterator uIt;
  for (uIt = userCacheMap->begin(); uIt != userCacheMap->end(); uIt++) {
    uIt->second.clearCache();
  }
  // clear the chunkMap to be consistent with the user caches
  AsidContentMap::iterator aIt;
  for (aIt = asidContentMap->begin(); aIt != asidContentMap->end(); aIt++) {
    ChunkMap::iterator cIt;
    for (cIt = aIt->second.begin(); cIt != aIt->second.end(); cIt++) {
      cIt->second.clear();
    }
  }
}

void TopologyOracle::clearLocalCache() {
  LocalCacheMap::iterator lIt;
  for (lIt = localCacheMap->begin(); lIt != localCacheMap->end(); lIt++) {
    lIt->second.clearCache();
  }
}

/* Attempts to find a source for the requested content. The priority is:
 * user's own cache -> other local peers -> local AS cache -> non local peers -> central server
 */
bool TopologyOracle::serveRequest(Flow* flow, Scheduler* scheduler) {
  PonUser destination = flow->getDestination();
  PonUser closestSource = UNKNOWN;
  SimTime time = scheduler->getSimTime();
  int currentDay = scheduler->getCurrentRound();
  ContentElement* content = flow->getContent();
  std::string contentName = content->getName();
  uint chunkId = flow->getChunkId();
  ChunkPtr chunk = content->getChunkById(chunkId);
  BOOST_LOG_TRIVIAL(trace) << time << ": fetching source for chunk " << chunkId
            << " of content " << contentName
            << " to user " << destination.first << "," << destination.second;
  // update the number of requests for this content if it's the first chunk
  if (chunk->getIndex() == 0)
    dailyRanking.at(currentDay-content->getReleaseDay()).hit(content);
  // also increase the number of hits of the chunk (currently not used)
  chunk->increaseViewsThisRound();
  // Set the flow as a transfer, since we are now assigning the source
  flow->setFlowType(FlowType::TRANSFER);
  // First check if the content is already in the user cache: if it is, there's
  // no need to simulate the data exchange and we only need to update the 
  // cache stats.
  if (checkIfCached(destination, chunk)) {
    // signal to the scheduler that this flow is "virtual"
    flow->setSource(destination);
    flow->setEta(time);
    // update this user's cacheMap entry (for LRU/LFU)
    bool result = userCacheMap->at(destination).getFromCache(chunk,
            (scheduler->getCurrentRound()+1)*time);
    assert(result);
    flowStats.servedRequests.at(scheduler->getCurrentRound())++;
    flowStats.completedRequests.at(scheduler->getCurrentRound())++;
    flowStats.localRequests.at(scheduler->getCurrentRound())++;
    flowStats.fromPeers.at(scheduler->getCurrentRound())++;
    // debug info
    BOOST_LOG_TRIVIAL(debug) << time << ": user " << destination.first
            << "," << destination.second 
            << " had a local copy of chunk " << chunkId << " from content " 
            << contentName;
    return true;
  }
  // try to find a local peer first
  uint asid = topo->getAsid(destination);
  std::set<PonUser> localSources = asidContentMap->at(asid).at(chunk);
  // copy the set to a vector which can then be randomized to reduce the chance
  // the same peer will be selected over and over until congestion is reached
  std::vector<PonUser> randSources(localSources.begin(), localSources.end());
  std::random_shuffle(randSources.begin(), randSources.end());
  localSources.clear();  
  bool foundSource = false;
  // Attempts to find the closest source whose route to the destination is not congested
  while (!randSources.empty() && !foundSource) {
    closestSource = randSources.back();
    // check if the identified source has enough content to serve this request
    if (this->checkIfCached(closestSource, chunk) 
            && !topo->isCongested(closestSource, destination))
      foundSource = true;
    else {
      // erase closestSource from randSources
      randSources.pop_back();
      closestSource = UNKNOWN;
    }
  }
  if (!foundSource)  
  {
    // Couldn't find a local source for the required content, check for the 
    // local AS cache (unless we're in reducedCaching mode)
    
    if (!reducedCaching) 
    {
      Vertex lCache = topo->getLocalCache(flow->getDestination().first);
      if (checkIfCached(lCache, chunk)) {
        // NOTE: We should check here if the downlink is congested, but with our
        // current hypothesis it should never be the case
        assert(topo->isCongested(std::make_pair(lCache, 0), destination) == false);
        // debug info
        BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
                << "," << destination.second
                << " downloading chunk " << chunkId 
                << " of content " << contentName
                << " from AS cache node " << lCache
                << " (closestSource=" << closestSource.first
                << "," << closestSource.second << ")";
        this->getFromLocalCache(lCache, chunk, 
                (scheduler->getCurrentRound()+1)*time);
        flow->setSource(std::make_pair(lCache, 0));
        flowStats.servedRequests.at(scheduler->getCurrentRound())++;
        flowStats.localRequests.at(scheduler->getCurrentRound())++;
        flow->setP2PFlow(false);
        scheduler->schedule(flow);
        topo->updateCapacity(flow, scheduler, true);
        return true;
      }
    }
    // no local peer and no micro cache, look for non local peer
    BOOST_LOG_TRIVIAL(trace) << "No local source found for chunk " << chunkId
            << " of content "+
            contentName+" , searching for non-local P2P sources";
    uint minDistance, distance, asIndex;
    ChunkMap::iterator cIt;
    // vector of ASes which have already explored for viable sources
    std::vector<bool> exploredASes(topo->getNumASes(), false);
    exploredASes[asid] = true;
    foundSource = false;
    while (std::find(exploredASes.begin(), exploredASes.end(), false) != exploredASes.end()
            && !foundSource) {
      // identify an AS with minimum distance from destination
      minDistance = INF_TIME;
      for (uint i = 0; i < topo->getNumASes(); i++) {
        if (exploredASes[i])
          continue;
        cIt = asidContentMap->at(i).find(chunk);
        if (cIt == asidContentMap->at(i).end() || cIt->second.empty()) {
          // this AS has no source with the required chunk, mark it as explored
          // to save time at a future iteration
          BOOST_LOG_TRIVIAL(trace) << "No viable source for chunk " << chunkId
            << " of content " + contentName + " in asid " << i;
          exploredASes[i] = true;
        } else {
          distance = topo->getRoute(*(cIt->second.begin()), destination).size();
          if (distance < minDistance) {
            minDistance = distance;
            asIndex = i;
          }
        }
      }
      // scan available sources in the identified closer AS
      if (minDistance != INF_TIME) {
        // Attempts to find the closest source whose route to the destination is not congested
        localSources.clear();
        localSources = asidContentMap->at(asIndex).at(chunk);
        randSources.clear();
        randSources.assign(localSources.begin(), localSources.end());
        std::random_shuffle(randSources.begin(), randSources.end());
        localSources.clear();  
        while (!randSources.empty() && !foundSource) {
          closestSource = randSources.back();
          // check if the identified source has enough content to serve this request
          if (this->checkIfCached(closestSource, chunk) 
                  && !topo->isCongested(closestSource, destination))
              foundSource = true;
          else {
            // erase closestSource from randSources
            randSources.pop_back();
            closestSource = UNKNOWN;
          }
        }
        exploredASes[asIndex] = true;
      }
    }    
  }
  // if the closestSource is still unknown, we have to go to the central server
  if (closestSource == UNKNOWN) {
    // Ensure the central server was specified in the topology file
    Vertex centralServer = topo->getCentralServer();
    assert(centralServer != std::numeric_limits<unsigned int>::max());
    // The 1 in the second PonUser field is to differentiate it from an AS Cache
    closestSource = std::make_pair(centralServer, 1);
    if (topo->isCongested(closestSource, destination)) {
      // This was our last hope: there is no uncongested route to any source
      flowStats.congestionBlocked.at(scheduler->getCurrentRound())++;
     BOOST_LOG_TRIVIAL(info) << time << ": user " << destination.first
            << "," << destination.second 
            << " could not find an un-congested route to chunk " << chunkId
            << " of content " << contentName;
      return false;
    }
    // If we're in reducedCaching mode, check if the content is stored on the 
    // central cache, otherwise fetch it off-network
    if (reducedCaching) {
      if (!checkIfCached(centralServer, chunk)) {
        localCacheMap->at(centralServer).addToCache(chunk, 
                chunk->getSize(), 
                (scheduler->getCurrentRound()*roundDuration)+time);
      }
      // update LFU/LRU stats
      this->getFromLocalCache(centralServer, chunk, 
              (scheduler->getCurrentRound()+1)*time);
    }
    flow->setP2PFlow(false);
    // debug info
    BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
            << "," << destination.second 
            << " downloading chunk " << chunkId
            << " of content " << contentName
            << " from central server";
  } else {
    // p2p flow, update user cache statistics (for LFU/LRU purposes)
    flow->setP2PFlow(true);
    bool result = userCacheMap->at(closestSource).getFromCache(chunk,
            (scheduler->getCurrentRound()+1)*time);
    assert(result);
    // check for locality is done here to avoid central server to be mistakenly
    // identified as local
    if (topo->isLocal(destination.first, closestSource.first))
      flowStats.localRequests.at(scheduler->getCurrentRound())++;
    // debug info
    BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
            << "," << destination.second << " (asid " << topo->getAsid(destination) 
            << ") downloading chunk " << chunkId
            << " of content " << contentName
            << " from peer " << closestSource.first << ","
            << closestSource.second << " (asid " << topo->getAsid(destination) 
            << ")";
  }
  // Assign the closest source to the flow
  flow->setSource(closestSource);
  // Update flow statistics
  flowStats.servedRequests.at(scheduler->getCurrentRound())++;
  // schedule flow with INF_TIME ETA so that it's in the queue for future updating
  scheduler->schedule(flow);
  // update network load and estimate ETA based on available bandwidth
  topo->updateCapacity(flow, scheduler, true); 
  return true;
}

void TopologyOracle::notifyCompletedFlow(Flow* flow, Scheduler* scheduler) {
  PonUser dest = flow->getDestination();
  SimTime time = scheduler->getSimTime();
  uint round = scheduler->getCurrentRound();
  ChunkPtr chunk = flow->getContent()->getChunkById(flow->getChunkId());
  switch(flow->getFlowType()) {
    case FlowType::TRANSFER:
    {
      if (flow->getSource() != dest) {
      BOOST_LOG_TRIVIAL(debug) << "At time " << time << " user " << dest.first
              << "," << dest.second << " completed download of chunk "
              << chunk->getIndex() << " of content " << chunk->getContent()->getName();
      flow->updateSizeDownloaded(time);
      if (flow->getSizeDownloaded() < chunkSize) {
        // ensure that this is a result of a discrete time scale (approximation to previous second)
        assert(chunk->getSize() - flow->getSizeDownloaded() <= flow->getBandwidth());
        BOOST_LOG_TRIVIAL(trace) << time << ": completed flow has sizeDownloaded (" <<
                flow->getSizeDownloaded() << ") < Chunk Size (" <<
                chunkSize << ") due to time approximation, fixing this";
        flow->setSizeDownloaded(chunkSize);
      }
      // update flow statistics
      SimTime flowDuration = time - flow->getStart();
      // Update average flow duration stats, both generic and p2p / cache ones
      flowStats.avgFlowDuration.at(round) = (flowStats.avgFlowDuration.at(round)
              * flowStats.completedRequests.at(round)
              + flowDuration) / (flowStats.completedRequests.at(round) + 1);
      flowStats.completedRequests.at(round)++;

      if (flow->isP2PFlow()) {
        flowStats.avgPeerFlowDuration.at(round) = (flowStats.avgPeerFlowDuration.at(round)
                * flowStats.fromPeers.at(round)
                + flowDuration) / (flowStats.fromPeers.at(round) + 1);
        flowStats.fromPeers.at(round)++;
      } else {
        flowStats.avgCacheFlowDuration.at(round) = (flowStats.avgCacheFlowDuration.at(round) *
                (flowStats.fromASCache.at(round) + flowStats.fromCentralServer.at(round)) + flowDuration)
                / (flowStats.fromASCache.at(round) + flowStats.fromCentralServer.at(round) + 1);
        if (flow->getSource().first == topo->getCentralServer() &&
                flow->getSource().second == 1)
          flowStats.fromCentralServer.at(round)++;
        else
          flowStats.fromASCache.at(round)++;
      }
      topo->updateLoadMap(flow);
      // notify the source cache that it has completed this upload
      PonUser source = flow->getSource();
      if (flow->isP2PFlow())
        userCacheMap->at(source).uploadCompleted(chunk);
      else
        localCacheMap->at(source.first).uploadCompleted(chunk);
      // update cache info (unless the content has expired, e.g. a flow carried over
      // from the previous round)
      /* FIXME: checking for a nullptr here does not make any sense, as we have 
       already accessed it earlier in the method - check the code to see if we need
       to do it earlier or it's not needed at all*/
      if (flow->getContent() != nullptr) {
        /* here we need to ask the oracle to decide whether we should cache the new
         * chunk. Because the optimization will also tell us what to delete, we
         * have to remove those chunks manually before invoking addToCache
         * Note that in the popularity estimation branch, we should try to optimize
         * only when we know enough about the content - e.g. a round.
         */
        if (round == 0 || !cachingOpt) {
          this->addToCache(dest, chunk, round * roundDuration + time);
        } else {
          std::pair<bool, bool> optResult = this->optimizeCaching(dest, chunk,
                  time, round);
          /* we need to add the requested element if the optimization failed OR if
           * it succeeded and it determined that we need to. The difference is that
           * in the second case elements that need to be removed have already been erased
           */
          if (!optResult.first || (optResult.first && optResult.second)) {
            this->addToCache(dest, chunk, round * roundDuration + time);
          }
          // also record if the cache optimization was successful 
          if (optResult.first == true)
            flowStats.cacheOptimized.at(round)++;
        }
      }
      // free resources in the topology
      topo->updateCapacity(flow, scheduler, false);
      }      
      // if the flow is carried over from the previous round, the rest should be skipped
      if (flow->getContent() == userWatchMap.at(dest).content) {
        // add this chunk to the streaming buffer of the user
        // FIXME: we are not checking if the buffer is full, although we do check when we fetch
        auto insResult = userWatchMap.at(dest).buffer.insert(chunk);
        assert(insResult.second == true);
        /* here we previously generated a new request, however this is no longer related to 
         * downloads, but rather to watching. the only thing we need to do is starting
         * a watch event if the user was waiting for the chunk we just downloaded
         */      
        if (userWatchMap.at(dest).waiting
                && userWatchMap.at(dest).currentChunk == chunk->getIndex()) {
          // if we are waiting for a chunk we can't be done, just make sure
          assert(chunk->getIndex() < userWatchMap.at(dest).chunksToBeWatched);
          // start a new watching flow for the chunk we just got
          BOOST_LOG_TRIVIAL(debug) << "User was waiting for this chunk, starting a WATCH flow";
          // if it was not the first chunk, display a debugging message to track rebuffering events
          if (chunk->getIndex() > 0) {
            BOOST_LOG_TRIVIAL(info) << "At time " << scheduler->getSimTime()
                    << " user " << dest.first << "," << dest.second
                    << " stopped waiting for chunk " << chunk->getIndex() << " of content "
                    << chunk->getContent()->getName();
          }
          SimTime eta = scheduler->getSimTime() +
                  std::ceil(chunk->getSize() / this->bitrate);
          Flow* watchEvent = new Flow(flow->getContent(), dest, eta, chunk->getIndex(),
                  FlowType::WATCH);
          scheduler->schedule(watchEvent);
          userWatchMap.at(dest).waiting = false;          
        }
      } else {
        BOOST_LOG_TRIVIAL(debug) << "Carried over transfer of chunk "
              << chunk->getIndex() << " of content " << chunk->getContent()->getName()
              << " to user " << dest.first << "," << dest.second
              << " from previous round";
      }
    } 
    break;
    
    case FlowType::WATCH:
    {
      BOOST_LOG_TRIVIAL(debug) << "At time " << time << " user " << dest.first
              << "," << dest.second << " finished watching chunk "
              << chunk->getIndex() << " of content " << chunk->getContent()->getName();
      // is it a carried over flow?
      if (flow->getContent() != userWatchMap.at(dest).content) {
        /* the userWatchMap for this watching session has been overwritten now,
         * and a new session has been programmed, so there is no reason to 
         * keep going with this but to complete the flows that were still hanging
         */
        BOOST_LOG_TRIVIAL(info) << "carried over watch flow for content " <<
                flow->getContent()->getName() << ", currently watching " <<
                userWatchMap.at(dest).content->getName();
        return;
      }
      // are we done with this content?
      uint completedChunk = userWatchMap.at(dest).currentChunk;
      if (time >= userWatchMap.at(dest).dailySessionInterval.getEnd() ||
              completedChunk >= userWatchMap.at(dest).chunksToBeWatched-1) {
        // yes, we are; free buffer and reset watching info
        BOOST_LOG_TRIVIAL(debug) << "At time " << time << " user " << dest.first
                << "," << dest.second << " finished watching content " 
                << flow->getContent()->getName() << " with chunk " 
                << completedChunk << "/" << flow->getContent()->getTotalChunks()-1;
        userWatchMap.at(dest).reset();
        // generate a new request (if the daily session is over, it's going to be checked there)
        this->generateNewRequest(dest, time, scheduler);
        // and that's all
        return;
      }
      // still more watching to do for this content
      // free space in the buffer now that the chunk has been watched
      userWatchMap.at(dest).buffer.erase(flow->getContent()->getChunkById(completedChunk));
      // pre-fetch as many new chunks as we can, since there's space in the buffer now
      uint bufferSlots = this->bufferSize - userWatchMap.at(dest).buffer.size();
      // there should be at least one free slot if nothing is wrong!
      assert (bufferSlots > 0);
      while (bufferSlots > 0 && 
              userWatchMap.at(dest).highestChunkFetched < flow->getContent()->getTotalChunks()-1) {    
        BOOST_LOG_TRIVIAL(debug) << "There's " << bufferSlots << " slots in the buffer, "
                "pre-fetching chunk " << userWatchMap.at(dest).highestChunkFetched+1;                
        Flow* requestChunk = new Flow(flow->getContent(), dest, time, 
                userWatchMap.at(dest).highestChunkFetched+1);
        scheduler->schedule(requestChunk);
        userWatchMap.at(dest).highestChunkFetched++;
        bufferSlots--;
      }      
           
      // first make sure that we are not attempting to fetch a chunk that does not exist
      assert(completedChunk != flow->getContent()->getTotalChunks()-1);
      // update the chunk we are currently interested into
      userWatchMap.at(dest).currentChunk++;
      // check if we've got the next chunk so we can start watching straight away
      if (userWatchMap.at(dest).buffer.find(flow->getContent()->getChunkById(completedChunk+1)) 
              != userWatchMap.at(dest).buffer.end()) {
        // start a new watching flow
        BOOST_LOG_TRIVIAL(debug) << "We had the next chunk (" << completedChunk+1
                << ") in the buffer, starting a new WATCH flow";
        SimTime eta = scheduler->getSimTime() + 
                std::ceil(flow->getContent()->getChunkById(completedChunk+1)->getSize() / this->bitrate);
        Flow* watchEvent = new Flow(flow->getContent(), dest, eta, completedChunk+1, 
                FlowType::WATCH);
        scheduler->schedule(watchEvent);
        // and remove the waiting status in case it was set
        userWatchMap.at(dest).waiting = false;
      } else {
        // there are chunks we want to watch and we do not have them. ALARM!
        BOOST_LOG_TRIVIAL(info) << "At time " << scheduler->getSimTime() 
                << " user " << dest.first << "," << dest.second
                << " started waiting for chunk " << completedChunk+1 << " of content "
                << flow->getContent()->getName();
        BOOST_LOG_TRIVIAL(info) << "Highest chunk fetched so far: "
                << userWatchMap.at(dest).highestChunkFetched;
        userWatchMap.at(dest).waiting = true;
      }      
    }
    break;
    default:
      BOOST_LOG_TRIVIAL(error) << "notifyCompletedFlow for non TRANSFER, non WATCH event.";
      abort();
  }
  
}

// Implemented for future use (e.g. distributed cache updates between rounds)
void TopologyOracle::notifyEndRound(uint endingRound) {
  // reset load on each link of the topology to prepare for next round's collection
  this->topo->resetLoadMap();
  // update the contentRateVec with the views observed on the current round
  double old = contentRateVec.at(0).at(0);
  for (uint day = 0; day < 7; day++) {
    for (auto i = 0; i < dailyRanking.at(day).size(); i++) {      
      contentRateVec.at(day).at(i) = ((contentRateVec.at(day).at(i) * 
              endingRound) + dailyRanking.at(day).getRoundHitsByRank(i))/(endingRound+1);
    }
  }
  BOOST_LOG_TRIVIAL(trace) << "old rate for day 0 rank 0: " << old << ", new: "
          << contentRateVec.at(0).at(0) << ", daily hits: " << dailyRanking.at(0).getHitsByRank(0);
  return;
}

std::set<PonUser> TopologyOracle::getSources(ChunkPtr chunk,
        uint asid) {
  return asidContentMap->at(asid).at(chunk);
}

void TopologyOracle::printStats(uint currentRound) {
  double localPctg, ASPctg, P2PPctg, CSPctg, blockPctg;
  localPctg = (flowStats.localRequests.at(currentRound)  / 
          (double) flowStats.completedRequests.at(currentRound)) * 100;
  ASPctg = (flowStats.fromASCache.at(currentRound) / 
          (double) flowStats.completedRequests.at(currentRound)) * 100;
  P2PPctg = (flowStats.fromPeers.at(currentRound) / 
          (double) flowStats.completedRequests.at(currentRound)) * 100;
  CSPctg = (flowStats.fromCentralServer.at(currentRound) / 
          (double) flowStats.completedRequests.at(currentRound)) * 100;
  blockPctg = (flowStats.congestionBlocked.at(currentRound) * 100) /
          (double) (flowStats.servedRequests.at(currentRound) +
          flowStats.congestionBlocked.at(currentRound));
  // check how much caching space is used on average in user caches
  double avgUserCacheOccupancy(0.0), avgMetroCacheOccupancy(0.0), temp(0.0);
  uint numCaches = 0;
  SimTime extTime = (currentRound + 1 ) * roundDuration;
  for (auto uIt = userCacheMap->begin(); uIt != userCacheMap->end() &&
          this->maxCacheSize > 0; uIt++) {
    temp = uIt->second.getAvgOccupancy(extTime);
    BOOST_LOG_TRIVIAL(trace) << "User " << uIt->first.first << "," <<  uIt->first.second
              << " has a cache occupancy of " << temp << "% (currentSize: " 
              << uIt->second.getCurrentSize() << ", maxSize: "
              << uIt->second.getMaxSize() << ") with "
              << uIt->second.getNumElementsCached() << " elements";
    avgUserCacheOccupancy += temp;
    numCaches++;
    uIt->second.resetOccupancy(extTime);
  }
  if (numCaches != 0)
    avgUserCacheOccupancy = avgUserCacheOccupancy / numCaches;
  else
    avgUserCacheOccupancy = 0.0;
  this->flowStats.avgUserCacheOccupancy.at(currentRound) = avgUserCacheOccupancy;
  // check how much caching space is used on average in metro/core caches
  numCaches = 0;
  for (auto uIt = localCacheMap->begin(); uIt != localCacheMap->end() && 
          this->maxLocCacheSize > 0; uIt++) {
    // central server cache should not be included in the average computation
    if (uIt->first != topo->getCentralServer()) {
      temp = uIt->second.getAvgOccupancy(extTime);
      BOOST_LOG_TRIVIAL(trace) << "AS cache " << uIt->first 
              << " has a cache occupancy of " << temp << "% (currentSize: " 
              << uIt->second.getCurrentSize() << ", maxSize: "
              << uIt->second.getMaxSize() << ") with "
              << uIt->second.getNumElementsCached() << " elements";
      avgMetroCacheOccupancy += temp;
      numCaches++;
    }
  uIt->second.resetOccupancy(extTime);  
  }
  if (numCaches != 0)
    avgMetroCacheOccupancy = avgMetroCacheOccupancy / numCaches;
  else
    avgMetroCacheOccupancy = 0.0;
  this->flowStats.avgASCacheOccupancy.at(currentRound) = avgMetroCacheOccupancy;
  
  // print stats to screen for human visualization
  std::cout << "Completed " << flowStats.completedRequests.at(currentRound)
          << " out of " << flowStats.servedRequests.at(currentRound)
          << " requests, of which " << flowStats.localRequests.at(currentRound) 
          << " locally ("
          << localPctg << "%). " << std::endl 
          << "Successful cache optimizations: " << flowStats.cacheOptimized.at(currentRound)
          << " (" << 100 * flowStats.cacheOptimized.at(currentRound) / (double) flowStats.completedRequests.at(currentRound)
          << "% of all completed requests)"
          << std::endl << "P2P flows: " 
          << flowStats.fromPeers.at(currentRound) << " (" << P2PPctg 
          << "%), AS Cache Flows: "
          << flowStats.fromASCache.at(currentRound) << " (" 
          << ASPctg << "%), Central Server Flows: "
          << flowStats.fromCentralServer.at(currentRound) << " (" << CSPctg
          << "%); blocked due to congestion: " 
          << flowStats.congestionBlocked.at(currentRound) << " ("
          << blockPctg << "%)" << std::endl;
  std::cout << "Average flow duration: " << flowStats.avgFlowDuration.at(currentRound)
            << "; Average P2P flow duration: " << flowStats.avgPeerFlowDuration.at(currentRound)
            << "; Average Cache flow duration: " << flowStats.avgCacheFlowDuration.at(currentRound)
            << std::endl;
  std::cout << "Average User Cache Occupancy: " << flowStats.avgUserCacheOccupancy.at(currentRound)
            << "%; Average AS Cache Occupancy: " <<flowStats.avgASCacheOccupancy.at(currentRound)
            << "%" << std::endl << std::endl;  
}

/* addContent performs the maintenance steps required when adding a new element
 * to the catalog, i.e., creating entries in the AsidContentMap for each chunk
 * and adding it to the central repository if we are in reducedCaching mode.
 */
void TopologyOracle::addContent(ContentElement* content, uint elapsedRounds) {
  SimTime time = elapsedRounds * roundDuration;
  AsidContentMap::iterator aIt;
  std::vector<ChunkPtr>::iterator cIt;
  std::vector<ChunkPtr> chunks = content->getChunks();
  for (aIt = asidContentMap->begin(); aIt != asidContentMap->end(); aIt++) {
    std::set<PonUser> users;
    for (cIt = chunks.begin(); cIt != chunks.end(); cIt++) {
      aIt->second.insert(std::make_pair(*cIt, users));
    }
  }
  if (this->reducedCaching) {
    for (LocalCacheMap::iterator lit = localCacheMap->begin();
            lit != localCacheMap->end(); lit++) {
      for (cIt = chunks.begin(); cIt != chunks.end(); cIt++) {
        lit->second.addToCache(*cIt, (*cIt)->getSize(), time);
      }
    }
  }
}

/* removeContent does the reverse of addContent; it should be called when an 
 * element is being removed from the catalog, as it deletes all the copies
 * of its chunks stored in caches across the system and removes the related
 * entries in the chunkMaps.
 */
void TopologyOracle::removeContent(ContentElement* content, uint roundsElapsed) {
  // erase the expiring content from all user and local caches
  SimTime time = roundsElapsed * roundDuration;
  AsidContentMap::iterator aIt;
  std::vector<ChunkPtr>::iterator cIt;
  std::vector<ChunkPtr> chunks = content->getChunks();
  for (aIt = asidContentMap->begin(); aIt != asidContentMap->end(); aIt++) {
    for (cIt = chunks.begin(); cIt != chunks.end(); cIt++) {
      BOOST_FOREACH(PonUser user, aIt->second.at(*cIt)) {
        userCacheMap->at(user).removeFromCache(*cIt, time);
      }
    aIt->second.erase(*cIt);
    }
  }
  for (LocalCacheMap::iterator lit = localCacheMap->begin(); 
          lit != localCacheMap->end(); lit++) {
    for (cIt = chunks.begin(); cIt != chunks.end(); cIt++) {
      lit->second.removeFromCache(*cIt, time);
    }
  }
}

bool TopologyOracle::checkIfCached(PonUser user, ChunkPtr chunk) {
  return userCacheMap->at(user).isCached(chunk);
}

bool TopologyOracle::checkIfCached(Vertex lCache, ChunkPtr chunk) {
  return localCacheMap->at(lCache).isCached(chunk);
}

void TopologyOracle::getFromLocalCache(Vertex lCache, ChunkPtr chunk, 
        SimTime time) {  
  // used to update LFU/LRU stats when content is grabbed directly from the
  // local cache
  if (localCacheMap->at(lCache).getMaxSize() >= chunk->getSize()) {
    bool result = localCacheMap->at(lCache).getFromCache(chunk, time);
    // the central server should have inifinite capacity but this is not implemented
    // so we should not get worried if the result is false
    if (lCache != topo->getCentralServer())
      assert (result);
  }
}

void TopologyOracle::removeFromCMap(ChunkPtr chunk, PonUser user) {
  uint asid = topo->getAsid(user);  
  std::set<PonUser>::iterator uIt = asidContentMap->at(asid).at(chunk).find(user);
  if (uIt == asidContentMap->at(asid).at(chunk).end()) {
    BOOST_LOG_TRIVIAL(trace) << "Attempted to remove missing chunk " << chunk->getIndex()
              << " from the cache of user " << user.first << ","
              << user.second;
    return;
  } else
    asidContentMap->at(asid).at(chunk).erase(uIt);
}

std::pair<bool, bool> TopologyOracle::optimizeCaching(PonUser reqUser, 
        ChunkPtr chunk, SimTime time, uint currentRound) {
  SimTime absTime = time + (currentRound*roundDuration);
  /* This should no longer apply as chunks are either cached entirely or not cached
   * 
   * there is an inconsistence in our assumptions here, in that we add 
   * an optimization variable for the requested content in addition to the 
   * variables we have for the elements cached, but there is a chance that the 
   * requested content was already cached (albeit with a different size). I need
   * to either make sure that this case is covered or that it never happens (e.g.,
   * by deleting it). In the next few lines I attempt to do so.
    
  if (checkIfCached(reqUser, content) == true) {
    userCacheMap->at(reqUser).removeFromCache(content, absTime);
    removeFromCMap(content, reqUser);
  }
  */
  int hour = std::floor(time / 3600);
  /* attempting to fix a potential bug in case we enter this function when it's
  /* midnight of the following day
   */
  if (hour >= usrPctgByHour.size())
    hour = usrPctgByHour.size() - 1;
  IloEnv env;
  int asid = topo->getAsid(reqUser);
  try {    
    IloModel model(env);
    IloNumVarArray c(env);
    auto cachedVec = userCacheMap->at(reqUser).getCacheMap();
    // add a boolean variable for each of the elements cached
    // char varName[24];
    for (auto it = cachedVec.begin(); it != cachedVec.end(); it++) {
      // varName = ((string)("c_" + it->first->getName())).c_str();
      c.add(IloNumVar(env, 0, 1, ILOINT));
    }
    // add a boolean variable for the element requested
    // varName = ((string)("c_" + content->getName())).c_str();
    c.add(IloNumVar(env, 0, 1, ILOINT));
    // Define the constraints
    IloRangeArray constraints(env);
    // Initialize the objective function expression
    IloNumExpr exp(env);
    // add the size of the elements cached, if kept in the cache
    int i = 0;
    for (auto it = cachedVec.begin(); it != cachedVec.end(); it++) {
      ChunkPtr chunkIt = it->first;
      ContentElement* contIt = chunkIt->getContent();
      exp += c[i] * it->second.size;
      // also make sure that we do not erase a content if we are uploading it
      if  (it->second.uploads > 0) {
        constraints.add(c[i] == 1);
        i++;
        continue;
      }
      // build the constraint on rates if required
      /* the contentRateVec holds the number of requests expected for each content
       * per day. To get the number of requests per hour per AS we need
       * to get the % of requests in this particular hour via usrPctgByHour and
       * multiply it by the fraction of users in the AS of the requester. The avg
       * concurrent request number is calculated as reqPerHour * avgReqLength. To
       * get the peak of concurrent users, which is what we need, we multiply
       * that value by a pakReqRatio, which is taken as an integer input parameter 
       * (-k).
       */
      uint dayIndex = currentRound - contIt->getReleaseDay();
      assert(0 <= dayIndex && dayIndex < 7);
      /* if it's a young content (released today and with only a few hours of life)
       * make sure it is not erased, as ranking is still too volatile to be accurate
       */
      if (dayIndex == 0 && hour < 6) {
        constraints.add(c[i] == 1);
        i++;
        continue;
      }
      uint rank = dailyRanking.at(dayIndex).getRankOf(contIt);
      double rate = contentRateVec.at(dayIndex).at(rank);
      double avgReqPerHour = (rate * usrPctgByHour.at(hour) / 100) *
          (topo->getASCustomers(asid) / topo->getNumCustomers());      
      BOOST_LOG_TRIVIAL(trace) << "avgReqPerHour(" << contIt->getName() << 
              ":" << chunkIt->getIndex() << "," << hour
              << ") = (" << rate << " * " << usrPctgByHour.at(hour)
              << " / 100) * (" << topo->getASCustomers(asid) << " / " << topo->getNumCustomers()
              << ") = " << avgReqPerHour;
      if (avgReqPerHour < 1) {
        BOOST_LOG_TRIVIAL(trace) << "avgReqPerHour for chunk " << chunkIt->getIndex()
              << " of content " << contIt->getName()
              << " is less than 1 (" << avgReqPerHour << "), setting it to 1";
        avgReqPerHour = 1;
      }
      int chunkRate = std::floor((peakReqRatio * avgReqPerHour * (avgReqLength / 3600)) + 0.5);
      // ensure that the peakReqRatio does not inflate the requests to more than the users we have
      chunkRate = std::min(chunkRate, static_cast<int>(topo->getASCustomers(asid)));
      // ensure that we keep at least one copy of each content in each AS
      chunkRate = std::max(1, chunkRate);
      BOOST_LOG_TRIVIAL(trace) << "chunkRate = std::floor((" << peakReqRatio 
              << " * " << avgReqPerHour
              << " * " << avgReqLength << " / 3600) + 0.5) = " << chunkRate;
      IloNumExpr cExp(env, 0);
      for (auto uit = asidContentMap->at(asid).at(chunkIt).begin();
              uit != asidContentMap->at(asid).at(chunkIt).end(); uit++) {
        if (reqUser != *uit) {
          cExp += maxUploads - userCacheMap->at(*uit).getTotalUploads()
                  + userCacheMap->at(*uit).getCurrentUploads(chunkIt);
        } else {
          // multiply the term by the caching variable so that we lose it if
          // we decide to erase the element from the cache
          cExp += c[i]* (IloInt) (maxUploads - userCacheMap->at(reqUser).getTotalUploads()
                  + userCacheMap->at(reqUser).getCurrentUploads(chunkIt));
        }
      }
      constraints.add(cExp >= chunkRate);      
      i++;
    }
    // same for the chunk we just downloaded
    exp += c[cachedVec.size()] * chunk->getSize();
    // add a chunkRate constraint for the downloaded chunk if needed
    uint dayIndex = currentRound - chunk->getContent()->getReleaseDay();
    assert(0 <= dayIndex && dayIndex < 7);
    if (dayIndex == 0 && hour < 6) {
      constraints.add(c[cachedVec.size()] == 1);
    } else {
      uint rank = dailyRanking.at(dayIndex).getRankOf(chunk->getContent());
      double rate = contentRateVec.at(dayIndex).at(rank);
      double avgReqPerHour = (rate * usrPctgByHour.at(hour) / 100) *
              (topo->getASCustomers(asid) / topo->getNumCustomers());
      BOOST_LOG_TRIVIAL(trace) << "avgReqPerHour(" << chunk->getContent()->getName() 
              << ":" << chunk->getIndex() << "," << hour
              << ") = (" << rate << " * " << usrPctgByHour.at(hour)
              << " / 100) * (" << topo->getASCustomers(asid) << " / " << topo->getNumCustomers()
              << ") = " << avgReqPerHour;
      if (avgReqPerHour < 1) {
        BOOST_LOG_TRIVIAL(trace) << "avgReqPerHour for chunk " << chunk->getIndex()
              << " of content " << chunk->getContent()->getName()
              << " is less than 1 (" << avgReqPerHour << "), setting it to 1";
        avgReqPerHour = 1;
      }
      int chunkRate = std::floor((peakReqRatio * avgReqPerHour * (avgReqLength / 3600)) + 0.5);
      // ensure that the peakReqRatio does not inflate the requests to more than the users we have
      chunkRate = std::min(chunkRate, static_cast<int>(topo->getASCustomers(asid)));
      // ensure that we keep at least one copy of each content in each AS
      chunkRate = std::max(1, chunkRate);
      BOOST_LOG_TRIVIAL(trace) << "chunkRate = std::floor((" << peakReqRatio
              << " * " << avgReqPerHour
              << " * " << avgReqLength << " / 3600) + 0.5) = " << chunkRate;
      IloNumExpr cExp(env, 0);
      for (auto uit = asidContentMap->at(asid).at(chunk).begin();
              uit != asidContentMap->at(asid).at(chunk).end(); uit++) {
        if (reqUser != *uit) {
          cExp += maxUploads - userCacheMap->at(*uit).getTotalUploads()
                  + userCacheMap->at(*uit).getCurrentUploads(chunk);
        } else {
          cExp += c[cachedVec.size()]* (IloInt) (maxUploads - userCacheMap->at(reqUser).getTotalUploads()
                  + userCacheMap->at(reqUser).getCurrentUploads(chunk));
        }
      }
      // if the requesting user was not already caching the content, we must add 
      // its term to the sum in case we decide to cache the requested content
      if (userCacheMap->at(reqUser).isCached(chunk) == false) {
        cExp += c[cachedVec.size()]* (IloInt) (maxUploads - userCacheMap->at(reqUser).getTotalUploads()
                + userCacheMap->at(reqUser).getCurrentUploads(chunk));
      }
      constraints.add(cExp >= chunkRate);
    }
    //finally add a constraint on the maximum size of the cache
    constraints.add(exp <= userCacheMap->at(reqUser).getMaxSize());

    // specify that we are interested in minimizing the objective
    IloObjective obj = IloMinimize(env, exp);
    // Add the objectives and the constraints to the model
    model.add(c);
    model.add(obj);
    model.add(constraints);

    // ask CPLEX to solve the problem
    IloCplex cplex(model);
    cplex.setOut(env.getNullStream());
    if (!cplex.solve()) {
      BOOST_LOG_TRIVIAL(trace) << "Failed to optimize caching for chunk " <<
              chunk->getIndex() << " of content " << 
              chunk->getContent()->getName() << " at user " << reqUser.first << "," <<
              reqUser.second << "; reverting to standard cache policies";
      env.end();
      return std::make_pair(false, false); 
    }
    
    IloNumArray vals(env);
    BOOST_LOG_TRIVIAL(trace) << "Solution status = " << cplex.getStatus();
    BOOST_LOG_TRIVIAL(trace) << "Solution value  = " << cplex.getObjValue();
    cplex.getValues(vals, c);
    BOOST_LOG_TRIVIAL(trace) << "Values        = " << vals;
    
    // check if any element has to be deleted from the cache
    i = 0;
    for (auto it = cachedVec.begin(); it != cachedVec.end(); it++) {
      if (vals[i] == 0) {
        userCacheMap->at(reqUser).removeFromCache(it->first, absTime);
        // also delete the user from the content map
        removeFromCMap(it->first, reqUser);
      }
      i++;
    }
    // check if reqContent has to be added to the cache
    bool shouldCache = (vals[cachedVec.size()] == 1);
    env.end();
    return std::make_pair(true, shouldCache);
    /*
    // print the new content of the cache for debugging purposes
    cachedVec = caches[reqUser].getCachedVec();
    cout << "Cache content after optimization: " ;
    for (auto it = cachedVec.begin(); it != cachedVec.end(); it++)
      cout << *it << " ";
    cout << endl << "Total size: " << caches[reqUser].getCurrentSize() << endl;
     * */
  } catch (IloException& e) {
    BOOST_LOG_TRIVIAL(warning) << "Concert exception caught: " << e;
  } 
  env.end();
  return std::make_pair(false, false); 
}
