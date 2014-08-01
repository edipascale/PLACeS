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
#include <ilcplex/ilocplex.h>

// random generator
extern boost::mt19937 gen;

TopologyOracle::TopologyOracle(Topology* topo, po::variables_map vm) {
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
  this->userCacheMap = new UserCacheMap;
  this->asidContentMap = new AsidContentMap;
  for (uint i = 0; i < topo->getNumASes(); i++) {
    ContentMap map;
    asidContentMap->insert(std::make_pair(i,map));
  }
  
  // Initialize user cache map
    VertexVec ponNodes = topo->getPonNodes();
    BOOST_FOREACH(Vertex v, ponNodes) {
    for(uint i = 0; i < topo->getPonCustomers(v); i++) {
        PonUser user = std::make_pair(v, i);
        ContentCache cache(maxCacheSize, policy);
        userCacheMap->insert(std::make_pair(user, cache));
      }
    }
  // Initialize local cache nodes
  this->localCacheMap = new LocalCacheMap;
  if (!reducedCaching) {    
    VertexVec cacheNodes = topo->getLocalCacheNodes();
    BOOST_FOREACH(Vertex v, cacheNodes) {
      ContentCache cache(maxLocCacheSize, policy);
      localCacheMap->insert(std::make_pair(v, cache));
    }
  } else {
    Vertex v = topo->getCentralServer();
    ContentCache cache(maxLocCacheSize, policy);
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

void TopologyOracle::addToCache(PonUser user, ContentElement* content, 
        Capacity sizeDownloaded, SimTime time) {
  // update the contentMap for content retrieval
  uint asid = topo->getAsid(user);
  ContentMap::iterator cIt = asidContentMap->at(asid).find(content);
  std::pair<bool, std::set<ContentElement*> > addResult;
  if (cIt == asidContentMap->at(asid).end()) {
    // This is the first time the oracle sees this content, add it to the map
    // Note: this should not happen now, throw a warning
    BOOST_LOG_TRIVIAL(warning) << time << ":WARNING: TopologyOracle::addToCache() - unknown content "
            << content->getName();
    std::set<PonUser> sourceNodes;
    cIt = asidContentMap->at(asid).insert(std::make_pair(content, sourceNodes)).first;
  }
  // update userCacheMap according to the specified cache policy
  BOOST_LOG_TRIVIAL(trace) << time << ": caching content " << content->getName() << " at User " 
          << user.first << "," << user.second << 
          "; sizeDownloaded: " << sizeDownloaded;
  addResult = userCacheMap->at(user).addToCache(content, sizeDownloaded, time);
  if (!addResult.first) {
    if (userCacheMap->at(user).getMaxSize() >= sizeDownloaded)
      BOOST_LOG_TRIVIAL(warning) << time << ": WARNING: failed to cache content " << content->getName() 
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
        BOOST_LOG_TRIVIAL(warning) << time << ": WARNING: failed to insert User " << user.first
                << "," << user.second << " as a source for content "
                << content->getName() << " in AS " << asid;
      }
    }
  }
  BOOST_FOREACH (ContentElement* e, addResult.second) {
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
    if (!localCacheMap->at(lCache).isCached(content, sizeDownloaded)) {
      // debug info
      BOOST_LOG_TRIVIAL(trace) << time << ": caching content " << content->getName()
              << " at local cache " << lCache;
      addResult = localCacheMap->at(lCache).addToCache(content, sizeDownloaded, time);
      if (!addResult.first) {
        BOOST_LOG_TRIVIAL(warning) << time << ": WARNING: failed to cache content " 
                << content->getName() << " at AS cache "  << lCache;
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
  // clear the contentMap to be consistent with the user caches
  AsidContentMap::iterator aIt;
  for (aIt = asidContentMap->begin(); aIt != asidContentMap->end(); aIt++) {
    ContentMap::iterator cIt;
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
  BOOST_LOG_TRIVIAL(trace) << time << ": fetching source for content " << contentName
            << " to user " << destination.first << "," << destination.second;
  // update the number of requests for this content
  dailyRanking.at(currentDay-content->getReleaseDay()).hit(content);
  // First check if the content is already in the user cache: if it is, there's
  // no need to simulate the data exchange and we only need to update the 
  // cache stats.
  if (checkIfCached(destination, content, flow->getSizeRequested())) {
    // signal to the scheduler that this flow is "virtual"
    flow->setSource(destination);
    flow->setEta(time);
    // update this user's cacheMap entry (for LRU/LFU)
    bool result = userCacheMap->at(destination).getFromCache(content,
            (scheduler->getCurrentRound()+1)*time);
    assert(result);
    flowStats.servedRequests.at(scheduler->getCurrentRound())++;
    flowStats.completedRequests.at(scheduler->getCurrentRound())++;
    flowStats.localRequests.at(scheduler->getCurrentRound())++;
    flowStats.fromPeers.at(scheduler->getCurrentRound())++;
    // debug info
    BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
            << "," << destination.second 
            << " had a local copy of content " << contentName;
    return true;
  }
  // try to find a local peer first
  uint asid = topo->getAsid(destination);
  std::set<PonUser> localSources = asidContentMap->at(asid).at(content);
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
    if (this->checkIfCached(closestSource, content, 
            flow->getSizeRequested()) 
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
      if (checkIfCached(lCache, content, flow->getSizeRequested())) {
        // NOTE: We should check here if the downlink is congested, but with our
        // current hypothesis it should never be the case
        assert(topo->isCongested(std::make_pair(lCache, 0), destination) == false);
        // debug info
        BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
                << "," << destination.second
                << " downloading content " << contentName
                << " from AS cache node " << lCache
                << " (closestSource=" << closestSource.first
                << "," << closestSource.second << ")";
        this->getFromLocalCache(lCache, content, 
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
    BOOST_LOG_TRIVIAL(trace) << "No local source found for content "+
            contentName+" , searching for non-local P2P sources";
    uint minDistance, distance, asIndex;
    ContentMap::iterator cIt;
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
        cIt = asidContentMap->at(i).find(content);
        if (cIt == asidContentMap->at(i).end() || cIt->second.empty()) {
          // this AS has no source with the required content, mark it as explored
          // to save time at a future iteration
          BOOST_LOG_TRIVIAL(trace) << "No viable source for " + contentName + 
                  " in asid " << i;
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
        localSources = asidContentMap->at(asIndex).at(content);
        randSources.clear();
        randSources.assign(localSources.begin(), localSources.end());
        std::random_shuffle(randSources.begin(), randSources.end());
        localSources.clear();  
        while (!randSources.empty() && !foundSource) {
          closestSource = randSources.back();
          // check if the identified source has enough content to serve this request
          if (this->checkIfCached(closestSource, content, 
                  flow->getSizeRequested()) 
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
     BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
            << "," << destination.second 
            << " could not find an uncongested route to content "
            << contentName;
      return false;
    }
    // If we're in reducedCaching mode, check if the content is stored on the 
    // central cache, otherwise fetch it off-network
    if (reducedCaching) {
      if (!checkIfCached(centralServer, content, 
              flow->getSizeRequested())) {
        localCacheMap->at(centralServer).addToCache(content, 
                content->getSize(), (scheduler->getCurrentRound()+1)*time);
      }
      // update LFU/LRU stats
      this->getFromLocalCache(centralServer, content, 
              (scheduler->getCurrentRound()+1)*time);
    }
    flow->setP2PFlow(false);
    // debug info
    BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
            << "," << destination.second 
            << " downloading content " << contentName
            << " from central server";
  } else {
    // p2p flow, update user cache statistics (for LFU/LRU purposes)
    flow->setP2PFlow(true);
    bool result = userCacheMap->at(closestSource).getFromCache(content,
            (scheduler->getCurrentRound()+1)*time);
    assert(result);
    // check for locality is done here to avoid central server to be mistakenly
    // identified as local
    if (topo->isLocal(destination.first, closestSource.first))
      flowStats.localRequests.at(scheduler->getCurrentRound())++;
    // debug info
    BOOST_LOG_TRIVIAL(trace) << time << ": user " << destination.first
            << "," << destination.second << " (asid " << topo->getAsid(destination) 
            << ") downloading content " << contentName
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

/* Override in IPTVTopologyOracle to add a generateNewRequest() at the end*/
void TopologyOracle::notifyCompletedFlow(Flow* flow, Scheduler* scheduler) {
  SimTime time = scheduler->getSimTime();
  uint round = scheduler->getCurrentRound();
  flow->updateSizeDownloaded(time);
  if (flow->getSizeDownloaded() < flow->getSizeRequested()) {
    // ensure that this is a result of a discrete time scale (approximation to previous second)
    assert(flow->getSizeRequested() - flow->getSizeDownloaded() <= flow->getBandwidth());
    BOOST_LOG_TRIVIAL(trace) << time << ": completed flow has sizeDownloaded (" << 
              flow->getSizeDownloaded() << ") < sizeRequested (" <<
              flow->getSizeRequested() << ") due to time approximation, fixing this";
    flow->setSizeDownloaded(flow->getSizeRequested());
  }
  // update flow statistics
  SimTime flowDuration = time - flow->getStart();
  // Update average flow duration stats, both generic and p2p / cache ones
  flowStats.avgFlowDuration.at(round) = (flowStats.avgFlowDuration.at(round) 
          * flowStats.completedRequests.at(round) 
          + flowDuration) / (flowStats.completedRequests.at(round) +1);
  flowStats.completedRequests.at(round)++;
  
  if (flow->isP2PFlow()) {
    flowStats.avgPeerFlowDuration.at(round) = (flowStats.avgPeerFlowDuration.at(round) 
            * flowStats.fromPeers.at(round) 
          + flowDuration) / (flowStats.fromPeers.at(round) +1);
    flowStats.fromPeers.at(round)++;
  } else {
    flowStats.avgCacheFlowDuration.at(round) = (flowStats.avgCacheFlowDuration.at(round) * 
            (flowStats.fromASCache.at(round) + flowStats.fromCentralServer.at(round)) + flowDuration) 
            / (flowStats.fromASCache.at(round) +flowStats.fromCentralServer.at(round) +1);
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
    userCacheMap->at(source).uploadCompleted(flow->getContent());
  else
    localCacheMap->at(source.first).uploadCompleted(flow->getContent());
  // update cache info (unless the content has expired, e.g. a flow carried over
  // from the previous round)
  PonUser dest = flow->getDestination();
  if (flow->getContent() != nullptr) {
    /* here we need to ask the oracle to decide whether we should cache the new
     * content. Because the optimization will also tell us what to delete, we
     * have to remove those contents manually before invoking addToCache
     * Note that in the popularity estimation branch, we should try to optimize
     * only when we know enough about the content - e.g. a couple of hours.
     * we should also prevent those young contents to be eliminated by the 
     * optimizer!
     */
    std::pair<bool, bool> optResult = this->optimizeCaching(dest, 
            flow->getContent(), flow->getSizeRequested(), time, round);
    /* we need to add the requested element if the optimization failed OR if
     * it succeeded and it determined that we need to. The difference is that
     * in the second case elements that need to be removed have already been erased
     */
    if (!optResult.first || (optResult.first && optResult.second)) {
      this->addToCache(dest, flow->getContent(), flow->getSizeDownloaded(), 
            (round+1)*time);
    }
  }
  // free resources in the topology
  topo->updateCapacity(flow, scheduler, false);
  // generate new request if this user's session is not over
  // NOTE: the start time for the new request is not necessarily the current time  
  // (the moment the flow was completed), but the time at which the user changes
  // channel! sizeRequested / bitrate = view duration
  time = flow->getStart() + 
          std::floor((flow->getSizeRequested() / this->bitrate) + 0.5);
  assert(time >= scheduler->getSimTime());
  if (flow->getStart() >= 0) // else it's a leftover flow from an expired session
    this->generateNewRequest(dest, time, scheduler);
}

// Implemented for future use (e.g. distributed cache updates between rounds)
void TopologyOracle::notifyEndRound(uint endingRound) {
  // reset load on each link of the topology to prepare for next round's collection
  this->topo->resetLoadMap();
  return;
}

std::set<PonUser> TopologyOracle::getSources(ContentElement* content,
        uint asid) {
  return asidContentMap->at(asid).at(content);
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
  float avgUserCacheOccupancy(0.0), avgMetroCacheOccupancy(0.0), temp(0.0);
  uint numCaches = 0;
  for (auto uIt = userCacheMap->begin(); uIt != userCacheMap->end() &&
          this->maxCacheSize > 0; uIt++) {
    avgUserCacheOccupancy += (100* uIt->second.getCurrentSize() 
            / uIt->second.getMaxSize());
    numCaches++;
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
      temp = (100 * uIt->second.getCurrentSize() 
            / uIt->second.getMaxSize());
      BOOST_LOG_TRIVIAL(trace) << "AS cache " << uIt->first 
              << " has a cache occupancy of " << temp << "% (currentSize: " 
              << uIt->second.getCurrentSize() << ", maxSize: "
              << uIt->second.getMaxSize() << ") with "
              << uIt->second.getNumElementsCached() << " elements";
      avgMetroCacheOccupancy += temp;
      numCaches++;
    }
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
          << localPctg << "%). " << std::endl << "P2P flows: " 
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

void TopologyOracle::addContent(ContentElement* content) {
  AsidContentMap::iterator aIt;
  for (aIt = asidContentMap->begin(); aIt != asidContentMap->end(); aIt++) {
    std::set<PonUser> users;
    aIt->second.insert(std::make_pair(content, users));
  }
  if (this->reducedCaching) {
    for (LocalCacheMap::iterator lit = localCacheMap->begin();
            lit != localCacheMap->end(); lit++) {
      // the time of addition shouldn't matter as the CDN sever always has
      // enough space for all contents in our simulations
      //FIXME: this might matter after all, especially in IPTV simulations
      lit->second.addToCache(content, content->getSize(), 0);
    }
  }
}

void TopologyOracle::removeContent(ContentElement* content) {
  // erase the expiring content from all user and local caches
  AsidContentMap::iterator aIt;
  for (aIt = asidContentMap->begin(); aIt != asidContentMap->end(); aIt++) {
    BOOST_FOREACH(PonUser user, aIt->second.at(content)) {
      userCacheMap->at(user).removeFromCache(content);
    }
    aIt->second.erase(content);
  }
  for (LocalCacheMap::iterator lit = localCacheMap->begin(); 
          lit != localCacheMap->end(); lit++) {
    lit->second.removeFromCache(content);
  }
}

bool TopologyOracle::checkIfCached(PonUser user, ContentElement* content,
        Capacity sizeRequested) {
  return userCacheMap->at(user).isCached(content, sizeRequested);
}

bool TopologyOracle::checkIfCached(Vertex lCache, ContentElement* content,
        Capacity sizeRequested) {
  return localCacheMap->at(lCache).isCached(content, sizeRequested);
}

void TopologyOracle::getFromLocalCache(Vertex lCache, ContentElement* content, 
        SimTime time) {  
  // used to update LFU/LRU stats when content is grabbed directly from the
  // local cache
  if (localCacheMap->at(lCache).getMaxSize() >= content->getSize()) {
    bool result = localCacheMap->at(lCache).getFromCache(content, time);
    // the central server should have inifinite capacity but this is not implemented
    // so we should not get worried if the result is false
    if (lCache != topo->getCentralServer())
      assert (result);
  }
}

void TopologyOracle::removeFromCMap(ContentElement* content, PonUser user) {
  uint asid = topo->getAsid(user);  
  std::set<PonUser>::iterator uIt = asidContentMap->at(asid).at(content).find(user);
  if (uIt == asidContentMap->at(asid).at(content).end()) {
    BOOST_LOG_TRIVIAL(trace) << "Attempted to remove missing content " << content->getName()
              << " from the cache of user " << user.first << ","
              << user.second;
    return;
  } else
    asidContentMap->at(asid).at(content).erase(uIt);
}

std::pair<bool, bool> TopologyOracle::optimizeCaching(PonUser reqUser, 
        ContentElement* content, Capacity sizeRequested, SimTime time, 
        uint currentRound) {
  /* FIXME: there is an inconsistence in our assumptions here, in that we add 
   * an optimization variable for the requested content in addition to the 
   * variables we have for the elements cached, but there is a chance that the 
   * requested content was already cached (albeit with a different size). I need
   * to either make sure that this case is covered or that it never happens (e.g.,
   * by deleting it before entering the method).
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
    for (int i = 0; i < cachedVec.size(); i++) {
      c.add(IloNumVar(env, 0, 1, ILOINT));
    }
    // add a boolean variable for the element requested
    c.add(IloNumVar(env, 0, 1, ILOINT));
    // Define the constraints
    IloRangeArray constraints(env);
    // Initialize the objective function expression
    IloNumExpr exp(env);
    // add the size of the elements cached, if kept in the cache
    int i = 0;
    for (auto it = cachedVec.begin(); it != cachedVec.end(); it++) {
      ContentElement* contIt = it->first;
      exp += c[i] * it->second.size;
      // also make sure that we do not erase a content if we are uploading it
      if (it->second.uploads > 0)
        constraints.add(c[i] == 1);
      // build the constraint on rates if required
      /* the contentRateVec holds the number of requests expected for each content
       * per user per day. To get the number of requests per hour per AS we need
       * to get the % of requests in this particular hour via usrPctgByHour and
       * multiply it by the number of users in the AS of the requester. The avg
       * concurrent request number is calculated as reqPerHour * avgReqLength. To
       * get the peak of concurrent users, which is what we need, we multiply
       * that value by a pakReqRatio, which is taken as an integer input parameter 
       * (-k).
       */
      uint dayIndex = currentRound - contIt->getReleaseDay();
      assert(0 <= dayIndex && dayIndex < 7);
      uint rank = dailyRanking.at(dayIndex).getRankOf(contIt);
      double rate = contentRateVec.at(dayIndex).at(rank);
      double avgReqPerHour = (rate * usrPctgByHour.at(hour) / 100) *
          topo->getASCustomers(asid);      
      BOOST_LOG_TRIVIAL(trace) << "avgReqPerHour(" << contIt->getName() << "," << hour
              << ") = (" << rate << " * " << usrPctgByHour.at(hour)
              << " / 100) * " << topo->getASCustomers(asid)
              << " = " << avgReqPerHour;
      int contentRate = std::floor((peakReqRatio * avgReqPerHour * avgReqLength / 3600) + 0.5);
      // ensure that we keep at least one copy of each content in each AS
      contentRate = std::max(1, contentRate);
      BOOST_LOG_TRIVIAL(trace) << "contentRate = std::floor((" << peakReqRatio 
              << " * " << avgReqPerHour
              << " * " << avgReqLength << " / 3600) + 0.5) = " << contentRate;
      IloNumExpr cExp(env, 0);
      for (auto uit = asidContentMap->at(asid).at(contIt).begin();
              uit != asidContentMap->at(asid).at(contIt).end(); uit++) {
        if (reqUser != *uit) {
          cExp += maxUploads - userCacheMap->at(*uit).getTotalUploads()
                  + userCacheMap->at(*uit).getCurrentUploads(contIt);
        } else {
          // multiply the term by the caching variable so that we lose it if
          // we decide to erase the element from the cache
          cExp += c[i]* (IloInt) (maxUploads - userCacheMap->at(reqUser).getTotalUploads()
                  + userCacheMap->at(reqUser).getCurrentUploads(contIt));
        }
      }
      constraints.add(cExp >= contentRate);      
      i++;
    }
    // same for the requested content
    exp += c[cachedVec.size()] * sizeRequested;
    IloNumExpr cExp(env, 0);
    for (auto uit = asidContentMap->at(asid).at(content).begin(); 
              uit != asidContentMap->at(asid).at(content).end(); uit++) {
      if (reqUser != *uit) {
        cExp += maxUploads - userCacheMap->at(*uit).getTotalUploads() 
                  + userCacheMap->at(*uit).getCurrentUploads(content);
      } else {
       cExp += c[cachedVec.size()]* (IloInt)(maxUploads - userCacheMap->at(reqUser).getTotalUploads() 
                    + userCacheMap->at(reqUser).getCurrentUploads(content));
      }
    }
    // if the requesting user was not already caching the content, we must add 
    // its term to the sum in case we decide to cache the requested content
    if (userCacheMap->at(reqUser).isCached(content, content->getSize()) == false) {
      cExp += c[cachedVec.size()]* (IloInt)(maxUploads - userCacheMap->at(reqUser).getTotalUploads() 
                    + userCacheMap->at(reqUser).getCurrentUploads(content));
    }
    // add a contentRate constraint for the requested content if needed
    uint dayIndex = currentRound - content->getReleaseDay();
    assert(0 <= dayIndex && dayIndex < 7);
    uint rank = dailyRanking.at(dayIndex).getRankOf(content);
    double rate = contentRateVec.at(dayIndex).at(rank);
    double avgReqPerHour = (rate * usrPctgByHour.at(hour) / 100) *
          topo->getASCustomers(asid);
    BOOST_LOG_TRIVIAL(trace) << "avgReqPerHour(" << content->getName() << "," << hour
              << ") = (" << rate << " * " << usrPctgByHour.at(hour)
              << " / 100) * " << topo->getASCustomers(asid)
              << " = " << avgReqPerHour;
    int contentRate = std::floor((peakReqRatio * avgReqPerHour * avgReqLength / 3600) + 0.5);
    // ensure that we keep at least one copy of each content in each AS
    contentRate = std::max(1, contentRate);
    BOOST_LOG_TRIVIAL(trace) << "contentRate = std::floor((" << peakReqRatio 
              << " * " << avgReqPerHour
              << " * " << avgReqLength << " / 3600) + 0.5) = " << contentRate;
    constraints.add(cExp >= contentRate);    
    
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
      BOOST_LOG_TRIVIAL(info) << "Failed to optimize caching for content " << 
              content->getName() << " at user " << reqUser.first << "," <<
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
        userCacheMap->at(reqUser).removeFromCache(it->first);
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
