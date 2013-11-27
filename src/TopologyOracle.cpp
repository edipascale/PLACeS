#include "TopologyOracle.hpp"
#include "Scheduler.hpp"
#include "boost/random/discrete_distribution.hpp"
#include "boost/random/uniform_int.hpp"
#include <boost/lexical_cast.hpp>
#include <initializer_list>
#include <boost/random/mersenne_twister.hpp>
#include <cmath>

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
  this->avgContentLength = vm["content-length"].as<double>();
  this->devContentLength = vm["content-dev"].as<double>();
  this->bitrate = vm["bitrate"].as<uint>();
  this->preCaching = vm["pre-caching"].as<bool>();
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
    std::cerr << "WARNING: TopologyOracle::addToCache() - unknown content "
            << content->getName() << std::endl;
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
    this->removeFromCMap(e, user);
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
  std::string contentName = flow->getContent()->getName();
  BOOST_LOG_TRIVIAL(trace) << time << ": fetching source for content " << contentName
            << " to user " << destination.first << "," << destination.second;
  // First check if the content is already in the user cache: if it is, there's
  // no need to simulate the data exchange and we only need to update the 
  // cache stats.
  if (checkIfCached(destination, flow->getContent(), flow->getSizeRequested())) {
    // signal to the scheduler that this flow is "virtual"
    flow->setSource(destination);
    flow->setEta(time);
    // update this user's cacheMap entry (for LRU/LFU)
    bool result = userCacheMap->at(destination).getFromCache(flow->getContent(),
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
  std::set<PonUser> localSources = asidContentMap->at(asid).at(flow->getContent());
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
    if (this->checkIfCached(closestSource, flow->getContent(), 
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
      if (checkIfCached(lCache, flow->getContent(), flow->getSizeRequested())) {
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
        this->getFromLocalCache(lCache, flow->getContent(), 
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
        cIt = asidContentMap->at(i).find(flow->getContent());
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
        localSources = asidContentMap->at(asIndex).at(flow->getContent());
        randSources.clear();
        randSources.assign(localSources.begin(), localSources.end());
        std::random_shuffle(randSources.begin(), randSources.end());
        localSources.clear();  
        while (!randSources.empty() && !foundSource) {
          closestSource = randSources.back();
          // check if the identified source has enough content to serve this request
          if (this->checkIfCached(closestSource, flow->getContent(), 
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
      if (!checkIfCached(centralServer, flow->getContent(), 
              flow->getSizeRequested())) {
        localCacheMap->at(centralServer).addToCache(flow->getContent(), 
                flow->getContent()->getSize(),
                (scheduler->getCurrentRound()+1)*time);
      }
      // update LFU/LRU stats
      this->getFromLocalCache(centralServer, flow->getContent(), 
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
    bool result = userCacheMap->at(closestSource).getFromCache(flow->getContent(),
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
  // update cache info (unless the content has expired, e.g. a flow carried over
  // from the previous round)
  PonUser dest = flow->getDestination();
  if (flow->getContent() != nullptr)
    this->addToCache(dest, flow->getContent(), flow->getSizeDownloaded(), 
            (round+1)*time);
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
