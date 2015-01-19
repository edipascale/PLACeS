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
  for (UserViewMap::iterator it = userViewMap.begin(); it != userViewMap.end(); it++) {
    delete (it->second);
  }
  userViewMap.clear();
  delete this->relDayDist;
  delete this->rankDist;
  delete this->expDist;
  delete this->shiftDist;
}

void IPTVTopologyOracle::generateUserViewMap(Scheduler* scheduler) {
  uint roundDuration = scheduler->getRoundDuration();
  double totalHours = 0, randomHours;
  userViewMap.clear();
  boost::random::normal_distribution<> userSessionDist(avgHoursPerUser);
  boost::random::discrete_distribution<> hourDist(usrPctgByHour);
  boost::random::uniform_int_distribution<> minSecDist(0, 3599);
  // Generate a session start time and session length for each user
  SimTime sessionStart, sessionLength;

  BOOST_FOREACH(Vertex v, topo->getPonNodes()) {
    for (uint i = 0; i < topo->getPonCustomers(v); i++) {
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
      std::pair<PonUser, SimTimeInterval*> userViewMapEntry; 
      userViewMapEntry.first = std::make_pair(v,i);
      userViewMapEntry.second = new SimTimeInterval(sessionStart, 
              sessionStart + sessionLength);
      userViewMap.insert(userViewMapEntry);
      // generate first request and schedule it
      this->generateNewRequest(userViewMapEntry.first, 
              userViewMapEntry.second->getStart(), scheduler);
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
        boost::lexical_cast<string > ((day + 6) * contentNum + i), day, i, size);
      dailyCatalog[std::abs(day)].at(i) = content;
      dailyRanking.at(std::abs(day)).insert(content);
      this->addContent(content, 0);
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
            currentRound + 1, i, size);
    dailyCatalog[0].at(i) = content;
    dailyRanking.at(0).insert(content);
    this->addContent(content, currentRound+1);   
  }  
}

void IPTVTopologyOracle::generateNewRequest(PonUser user, SimTime time, 
        Scheduler* scheduler) {
  SimTime sessionEnd = userViewMap.at(user)->getEnd();
  if (time < sessionEnd) {
    boost::random::uniform_int_distribution<> indexDist(0, 17);
    uint i = (*relDayDist)(gen);
    ContentElement* content = dailyCatalog[i].at((*rankDist)(gen));
    Capacity reqLength = sessionLength[indexDist(gen)] * content->getSize();
    // check that the new request would not go too much past the desired session
    // length; this will make all requests past midnight end at midnight :(
    SimTime reqEnd = std::ceil(time + (reqLength / this->bitrate));
    if (reqEnd > sessionEnd) {
      // new reqLength can't be less than 1 second worth of traffic
      reqLength = std::max((double)1*this->bitrate,
         reqLength - (reqEnd - sessionEnd) * this->bitrate);
    }
    /* Switching to a linear session length, this will no longer be used
    if (content->getWeeklyRank() <= this->contentNum * 0.25)
      reqLength = std::ceil(sessionLength1Q[indexDist(gen)] * content->getSize());
    else if (content->getWeeklyRank() <= this->contentNum * 0.50)
      reqLength = std::ceil(sessionLength2Q[indexDist(gen)] * content->getSize());
    else if (content->getWeeklyRank() <= this->contentNum * 0.75)
      reqLength = std::ceil(sessionLength3Q[indexDist(gen)] * content->getSize());
    else
      reqLength = std::ceil(sessionLength4Q[indexDist(gen)] * content->getSize());
     * */
    Flow* request = new Flow(content, user, time);
    scheduler->schedule(request);
  }
}

void IPTVTopologyOracle::notifyEndRound(uint endingRound) {
  for (UserViewMap::iterator it = userViewMap.begin(); it != userViewMap.end(); it++) {
    delete it->second;
  }
  this->userViewMap.clear();
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
        std::pair<bool, std::set<ContentElement*> > result;
        result = localCacheMap->at(as).addToCache(content, content->getSize(),0);
        assert(result.first == true);
        assert(result.second.empty() == true);
        assert(localCacheMap->at(as).isCached(content));
      }
      else {
        isFull = true;
        break;
      }        
    }
    i++;
  }
  BOOST_LOG_TRIVIAL(warning) << "cached " << i << " elements in AS caches";
}