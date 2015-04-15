#include "IPTVTopologyOracle.hpp"
#include "Scheduler.hpp"
#include "boost/random/discrete_distribution.hpp"
#include "boost/random/uniform_int.hpp"
#include "boost/random/normal_distribution.hpp"
#include "zipf_distribution.hpp"
#include <boost/lexical_cast.hpp>
#include <initializer_list>
#include <boost/random/mersenne_twister.hpp>

// random generator
extern boost::mt19937 gen;

IPTVTopologyOracle::IPTVTopologyOracle(Topology* topo, po::variables_map vm,
        uint roundDuration) : TopologyOracle(topo, vm, roundDuration) {
  this->contentNum = vm["contents"].as<uint>() * vm["channels"].as<uint>();
  double zmExp = vm["zm-exponent"].as<double>();
  this->shiftDist = new boost::random::uniform_int_distribution<>(0,50);
  this->expDist = new boost::random::uniform_real_distribution<>(0.4, zmExp);
  this->rankDist = new boost::random::zipf_distribution<>(contentNum, 
          (*shiftDist)(gen), (*expDist)(gen));
  this->relDayDist = new boost::random::zipf_distribution<>(7, 0, (*expDist)(gen));
  // allocate memory for the dailyCatalog and initialize contentRateVec
  std::vector<double> dailyRank(contentNum, 0.0);
  contentRateVec.resize(7, dailyRank);
  for (uint i = 0; i < 7; i++) {
    std::vector<ContentElement*> vec(contentNum, nullptr);
    dailyCatalog.push_back(vec);
  }  
}

IPTVTopologyOracle::~IPTVTopologyOracle() {
  BOOST_FOREACH(std::vector<ContentElement*> list, dailyCatalog) {
    BOOST_FOREACH(ContentElement* content, list) {
      delete content;
    }
    list.clear();
  }
  dailyCatalog.clear();
  userWatchMap.clear();
  delete this->relDayDist;
  delete this->rankDist;
  delete this->expDist;
  delete this->shiftDist;
}

void IPTVTopologyOracle::generateUserViewMap(Scheduler* scheduler) {
  BOOST_LOG_TRIVIAL(debug) << "entering generateUserViewMap";
  /* fetching video chunks takes time, so we get delays with respect to the 
   * session defined here; hence I'm introducing a guardThreshold so that we 
   * have enough time to complete all the flows before midnight (hopefully)
  
  uint guardThreshold = 60;
  uint roundDuration = scheduler->getRoundDuration() - guardThreshold;
   *  */
  uint roundDuration = scheduler->getRoundDuration();
  double totalHours = 0, randomHours;
  boost::random::normal_distribution<> userSessionDist(avgHoursPerUser);
  boost::random::discrete_distribution<> hourDist(usrPctgByHour);
  boost::random::uniform_int_distribution<> minSecDist(0, 3599);
  // Generate a session start time and session length for each user
  SimTime sessionStart, sessionLength;

  BOOST_FOREACH(Vertex v, topo->getPonNodes()) {
    for (uint i = 0; i < topo->getPonCustomers(v); i++) {
      PonUser user = std::make_pair(v,i);
      // populate userWatchMap for the first time if it's round 0
      if (scheduler->getCurrentRound() == 0) {
        std::pair<PonUser, UserWatchingInfo> userWatchMapEntry = std::make_pair(user, UserWatchingInfo()); 
        userWatchMap.insert(userWatchMapEntry);
      }
      else {
        userWatchMap.at(user).reset();
      }
      randomHours = userSessionDist(gen);
      // daily sessions can't be longer than a day, duh
      if (randomHours > 24)
        randomHours = 24;
      if (randomHours > 0)
        totalHours += randomHours;
      sessionLength = std::floor((randomHours * 3600) + 0.5);
      if (sessionLength <= 0) {
        BOOST_LOG_TRIVIAL(info) << "TopologyOracle::generateUserViewMap() - "
                  "non-positive sessionLength for user " << v << "," << i
                  << " - skipping it";        
        continue;
      }
      /* the time we generate is assumed to be the middle point of the viewing
       * session, so we shift it of half sessionLength, but we need to ensure it
       * still fits inside the round
       */
      sessionStart = (hourDist(gen) * 3600 + minSecDist(gen)) -
              std::floor((sessionLength / 2) + 0.5);
      if (sessionStart < 0)
        sessionStart = 0;
      else if (sessionStart + sessionLength >= roundDuration)
        sessionStart = roundDuration - sessionLength;
      SimTimeInterval interval(sessionStart, sessionStart + sessionLength);
      // store the new interval
      userWatchMap.at(user).dailySessionInterval = interval;
      // generate first request and schedule it
      this->generateNewRequest(user, interval.getStart(), scheduler);
    }
  }
  BOOST_LOG_TRIVIAL(info) << "Total hours of scheduled viewing for the current day: "
          << totalHours << "; avg. hours per user: " 
          << totalHours/(topo->getNumCustomers()) 
          << " (requested: " << avgHoursPerUser << ")";
}

void IPTVTopologyOracle::populateCatalog() {
  // normal distribution to generate content length in minutes
  boost::random::normal_distribution<> normDist(this->avgContentLength, 
          this->devContentLength);
  Capacity size = 0;
  // generate content for the first day and the previous 6
  for (int day = -6; day < 1; day++) {
    for (uint i = 0; i < contentNum; i++) {
      size = std::ceil(normDist(gen) * 60 * this->bitrate);
      ContentElement* content = new ContentElement(
        boost::lexical_cast<string > ((day + 6) * contentNum + i), day, size, 
              chunkSize);
      dailyCatalog[std::abs(day)].at(i) = content;
      // insert the content (NOTE: not the chunks) in the daily ranking
      dailyRanking.at(std::abs(day)).insert(content);
      this->addContent(content, 0);
      BOOST_LOG_TRIVIAL(debug) << "generated content " << content->getName();
    }
  }
}

void IPTVTopologyOracle::updateCatalog(uint currentRound) {
  // Update catalogue 
  dailyRanking.at(6).clear();
  BOOST_FOREACH(ContentElement* content, dailyCatalog[6]) {
    this->removeContent(content, currentRound+1);
    delete content;
  }
  for (uint day = 6; day > 0; day--) {
    dailyCatalog[day] = dailyCatalog[day - 1];
    dailyRanking.at(day).resetRoundHits();
    dailyRanking.at(day) = dailyRanking.at(day-1);
  }
  dailyRanking.at(0).clear();
  // normal distribution to generate content length in minutes
  boost::random::normal_distribution<> normDist(this->avgContentLength, 
          this->devContentLength);
  Capacity size = 0;
  for (uint i = 0; i < contentNum; i++) {
    size = std::ceil(normDist(gen) * 60 * this->bitrate);
    ContentElement* content = new ContentElement(
            boost::lexical_cast<string > ((currentRound + 7) * contentNum + i),
            currentRound + 1, size, chunkSize);
    dailyCatalog[0].at(i) = content;
    dailyRanking.at(0).insert(content);
    this->addContent(content, currentRound+1);   
  }  
}

/* selects a movie to be requested according to the popularity distributions,
 * determines how long the user is going to be watching it (because of zapping)
 * and stores the required information in the userWatchMap, then creates a new
 * Flow and schedules it.
 */
void IPTVTopologyOracle::generateNewRequest(PonUser user, SimTime time, 
        Scheduler* scheduler) {
  SimTime sessionEnd = userWatchMap.at(user).dailySessionInterval.getEnd();
  if (time < sessionEnd) {
    boost::random::uniform_int_distribution<> indexDist(0, 17);
    uint i = (*relDayDist)(gen);
    ContentElement* content = dailyCatalog[i].at((*rankDist)(gen));
    double watchingPortion = sessionLength[indexDist(gen)];
    Capacity reqLength =  watchingPortion * content->getSize();
    /* calculate the time at which the user will be done with this content */
    SimTime endTime = time + std::ceil(reqLength / this->bitrate);
    // check that the new request would not go past the desired session
    // length; this will make all requests past midnight end at midnight :(
    if (endTime > sessionEnd) {
      endTime = sessionEnd;
    }
    // determine the number of chunks to be watched for this content
    uint watchingChunks = std::ceil(watchingPortion * content->getTotalChunks());
    if (watchingChunks > content->getTotalChunks())
      watchingChunks = content->getTotalChunks();
    UserWatchingMap::iterator wIt = userWatchMap.find(user);
    assert (wIt != userWatchMap.end());
    wIt->second.content = content;
    wIt->second.chunksToBeWatched = watchingChunks;
    // we are waiting for the first chunk to be downloaded
    wIt->second.waiting = true;
    /* Request enough chunks to fill the buffer. 
     */
    for (uint i = 0; i < bufferSize; i++) {
      // stop if we fetched all the chunks of this content
      if (i >= content->getTotalChunks())
        continue;
      Flow* request = new Flow(content, user, time, i);
      scheduler->schedule(request);
      wIt->second.highestChunkFetched = i;
    }
    BOOST_LOG_TRIVIAL(debug) << "generated request from user " << user.first
            << "," << user.second << " for content " << content->getName();
  }
}

void IPTVTopologyOracle::notifyEndRound(uint endingRound) {
  delete this->rankDist;
  this->rankDist = new boost::random::zipf_distribution<>(contentNum, 
          (*shiftDist)(gen), (*expDist)(gen));
  delete this->relDayDist;
  this->relDayDist = new boost::random::zipf_distribution<>(7, 0, (*expDist)(gen));
}

void IPTVTopologyOracle::preCache() {
  bool isFull = false;
  uint i(0), day(0), index(0);
  ContentElement* content = nullptr;
  while (!isFull) {
    day = std::floor(i / dailyCatalog.at(0).size()); // all the same size
    if (day >= dailyCatalog.size())
      break;
    index = i % dailyCatalog.at(day).size();
    content = dailyCatalog.at(day).at(index);
    for (uint as = 0; as < topo->getNumASes(); as++) {
      if (localCacheMap->at(as).fitsInCache(content->getSize())) {
        auto chunks = content->getChunks();
        std::pair<bool, std::set<ChunkPtr> > result;
        for (auto j = chunks.begin(); j != chunks.end(); j++) {
          result = localCacheMap->at(as).addToCache(*j, (*j)->getSize(), 0);
          assert(result.first == true);
          assert(result.second.empty() == true);
          assert(localCacheMap->at(as).isCached(*j));
        }
      }
      else {
        isFull = true;
        break;
      }        
    }
    i++;
  }
  BOOST_LOG_TRIVIAL(trace) << "cached " << i << " contents in AS caches";
}