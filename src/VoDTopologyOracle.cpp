#include "VoDTopologyOracle.hpp"
#include "Scheduler.hpp"
#include "boost/random/normal_distribution.hpp"
#include "UGCPopularity.hpp"
#include <boost/lexical_cast.hpp>
#include <initializer_list>
#include <boost/random/mersenne_twister.hpp>

// random generator
extern boost::mt19937 gen;

VoDTopologyOracle::VoDTopologyOracle(Topology* topo, po::variables_map vm, uint roundDuration)
    : TopologyOracle(topo, vm, roundDuration) {
  this->contentNum = vm["contents"].as<uint>();
  this->popularity = new UGCPopularity(vm["rounds"].as<uint>(), vm["perturbations"].as<bool>());
}

VoDTopologyOracle::~VoDTopologyOracle() {
  beforePeak.clear();
  atPeak.clear();
  afterPeak.clear();
}

void VoDTopologyOracle::populateCatalog() {
  // normal distribution to generate content length in minutes
  boost::random::normal_distribution<> normDist(this->avgContentLength, 
          this->devContentLength);
  Capacity size = 0;
  // generate content
  for (uint i = 0; i < contentNum; i++) {
    size = std::ceil(normDist(gen) * 60 * this->bitrate);
    ContentElement* content = new ContentElement(boost::lexical_cast<string > (i),
            0, size, size);
    content->setPeakingRound(popularity->generatePeakRound());
    if (content->getPeakingRound() == 0)
      this->atPeak.push_back(content);
    else
      this->beforePeak.push_back(content);
    this->addContent(content, 0);
  }
}

void VoDTopologyOracle::generateUserViewMap(Scheduler* scheduler) {
  uint totalViews = 0;
  if (!beforePeak.empty()) {
    totalViews += popularity->generateViews(beforePeak, BEFORE_PEAK);
  }
  if (!atPeak.empty()) {
    totalViews += popularity->generateViews(atPeak, AT_PEAK);
  }
  if (!afterPeak.empty()) {
    totalViews += popularity->generateViews(afterPeak, AFTER_PEAK);
  }
  uint totalPeers = topo->getNumCustomers();
  double avgViewsPerPeer = (double) totalViews / (double) totalPeers;
  double avgHoursPerUserGen = avgViewsPerPeer * (this->avgContentLength / 60);
  if (avgHoursPerUserGen != this->avgHoursPerUser) {
    // scale content views to available customers
    std::cout << "Scaling views to ensure the average hours of view "
            "per peer per round is approximately " << this->avgHoursPerUser
            << " (current average: " << avgHoursPerUserGen << ")"
            << std::endl;
    double scale =  this->avgHoursPerUser / avgHoursPerUserGen;
    totalViews = 0;

    BOOST_FOREACH(ContentElement* content, beforePeak) {
      if (content->getViewsThisRound() > 0) {
        content->setViewsThisRound(std::max(1,
                (int) std::floor(content->getViewsThisRound() * scale)));
        totalViews += content->getViewsThisRound();
      }
    }

    BOOST_FOREACH(ContentElement* content, atPeak) {
      if (content->getViewsThisRound() > 0) {
        content->setViewsThisRound(std::max(1,
                (int) std::floor(content->getViewsThisRound() * scale)));
        totalViews += content->getViewsThisRound();
      }
    }

    BOOST_FOREACH(ContentElement* content, afterPeak) {
      if (content->getViewsThisRound() > 0) {
        content->setViewsThisRound(std::max(1,
                (int) std::floor(content->getViewsThisRound() * scale)));
        totalViews += content->getViewsThisRound();
      }
    }
    std::cout << "Total views after scaling: " << totalViews
              << ", avgViewsPerPeer: " << (double) totalViews / (double) totalPeers
              << std::endl;
  }  
  // Views have been generated, now we need to map them to random users
  this->scheduleRequests(beforePeak, scheduler);
  this->scheduleRequests(atPeak, scheduler);
  this->scheduleRequests(afterPeak, scheduler);
}

// This method is not used in VoD simulations, remove it
void VoDTopologyOracle::generateNewRequest(PonUser user, SimTime time, 
        Scheduler* scheduler) {
  return;
  }

void VoDTopologyOracle::updateCatalog(uint currentRound) {
  // move content that peaked last round to afterPeak
  afterPeak.splice(afterPeak.begin(), atPeak);
  // move content that peaks in the next round from beforePeak to atPeak
  std::list<ContentElement*>::iterator temp;
  std::list<ContentElement*>::iterator it = beforePeak.begin();
  while (it != beforePeak.end()) {
    if ((*it)->getPeakingRound() == currentRound + 1) {
      temp = it;
      temp++;
      atPeak.splice(atPeak.end(), beforePeak, it);
      it = temp;
    } else
      it++;
  }
}

void VoDTopologyOracle::scheduleRequests(std::list<ContentElement*> list, 
                                        Scheduler* scheduler) {
  boost::random::discrete_distribution<> hourDist(usrPctgByHour);
  boost::random::discrete_distribution<> dayDist(dayWeights);
  boost::random::uniform_int_distribution<> minSecDist(0, 3599);
  std::vector<Vertex> ponNodes = this->getTopology()->getPonNodes();
  uint totUsers = topo->getNumCustomers();
  std::set<uint> assignedUsers;
  uint randUser;
  PonUser randPonUser;
  BOOST_FOREACH (ContentElement* content, list) {    
    // Newest implementation: ensures that a random value is computed exactly
    // viewsThisWeek times, no more. However this doesn't check that the 
    // selected user does not have this content cached. After all, cached 
    // content CAN be requested for a new view and that's part of the model.
    
    // Check that we are going to have enough users to be mapped to requests
    uint views = content->getViewsThisRound();
    if (totUsers < views) {
      std::cout << "VoDTopologyOracle::serveRequests() - Not enough users to map all "
              << views <<
              " requests for content " << content->getName() <<
              ", truncating to " << totUsers << std::endl;
      content->setViewsThisRound(totUsers);
    }
    // Select random users to map requests
    for (uint j = totUsers - views; j < totUsers; j++) {
      boost::random::uniform_int_distribution<> userDist(0, j-1);
      randUser = userDist(gen);
      if (assignedUsers.insert(randUser).second == false) {
        randUser = j;
        auto rValue = assignedUsers.insert(randUser);
        assert(rValue.second == true);
      }
      /* this worked very well when all PONs had the same # of customers, now
       * we are forced to go through the following loop to map a planar integer 
       * to the right PonUser, unless I can find a better way to deal with this.
       */
      uint t(0), oldt(0), i(0);
      while (randUser >= t) {
        oldt = t;
        t += topo->getPonCustomers(ponNodes.at(i));
        i++;
      }
      assert(topo->getPonCustomers(ponNodes.at(i - 1)) > randUser - oldt);
      randPonUser = std::make_pair(ponNodes.at(i - 1), randUser - oldt);
      // generate a random request time
      SimTime reqTime = dayDist(gen) * 86400 + // day
              hourDist(gen) * 3600 + // hour
              minSecDist(gen); // minutes and seconds
      // generate request
      // TODO: Add support for zapping in VoD too.
      Flow* req = new Flow(content, randPonUser, reqTime);
      scheduler->schedule(req);        
    }
    assignedUsers.clear();
    
    /* New implementation: roll over the amount of users and add those already
     * chosen to a set to avoid repetitions. Hopefully faster.     
    
    // Check that we are going to have enough users to be mapped to requests
    // TODO: might want to do this even if views do not reach this value but
    // are numerous enough that rolling for a random unassigned user becomes a
    // significant time sink
    uint unavailableUsers = oracle->getSources(content).size();
    if (totUsers -  unavailableUsers <= content->getViewsThisRound()) {
      std::cout << "TestRun::serveRequests() - Not enough users to map all "
              << content->getViewsThisRound() <<
              " requests for content " << content->getName() <<
              ", truncating to " << totUsers -  unavailableUsers << std::endl;
      content->setViewsThisRound(totUsers -  unavailableUsers);
      // assign the views sequentially rather than randomly, as we know all
      // users are going to be involved anyway
      BOOST_FOREACH(Vertex v, ponNodes) {
        for (uint i = 0; i < ponCardinality; i++) {          
          // check that this user has not the content in its cache
          if (oracle->checkIfCached(std::make_pair(v,i), content) == false) {
            // generate a random request time
            SimTime reqTime = dayDist(gen) * 86400 + // day
                    hourDist(gen) * 3600 + // hour
                    minSecDist(gen); // minutes and seconds
            // generate request
            Flow* req = new Flow(content, std::make_pair(v, i), reqTime);
            scheduler->schedule(req);
          }
        }
      }
    } else { // we've got more users than requests, roll randomly
      PonUser randPonUser;
      uint ponNodeIndex, userIndex;
      for (unsigned int i = 0; i < content->getViewsThisRound(); i++) {
        // choose a random new Pon node as the requester of the content;
        // checks that the user hasn't already requested that content this round
        // and that it is not caching it
        do {
          randUser = userDist(gen);
          ponNodeIndex = std::floor(randUser / ponCardinality);
          userIndex = randUser % ponCardinality;
          randPonUser = std::make_pair(ponNodes[ponNodeIndex],userIndex);                  
        } while (assignedUsers.insert(randUser).second != false
                && oracle->checkIfCached(randPonUser, content));
        // generate a random request time
          SimTime reqTime = dayDist(gen) * 86400 + // day
                  hourDist(gen) * 3600 + // hour
                  minSecDist(gen); // minutes and seconds
          // generate request
          Flow* req = new Flow(content, randPonUser, reqTime);
          scheduler->schedule(req);
      }
    }
    assignedUsers.clear();
    */
    
    /* Old implementation - building a vector of all available PonUsers and 
     removing them from the list as we select them. Terribly slow. 

    // build a vector of all the available PonUsers
    std::vector<PonUser> availableUsers;
    BOOST_FOREACH(Vertex v, ponNodes) {
      for (unsigned int i = 0; i < ponCardinality; i++) {
        availableUsers.push_back(std::make_pair<unsigned int, unsigned int>(v,i));
      }
    }
    for (unsigned int i = 0; i < content->getViewsThisRound(); i++) {
      // choose a random new Pon node as the requester of the content
      boost::uniform_int<unsigned int> uniDist(0, availableUsers.size());
      unsigned int randUser = uniDist(gen);
      // generate a random request time
      SimTime reqTime = dayDist(gen) * 86400 + // day
              hourDist(gen) * 3600 + // hour
              minSecDist(gen); // minutes and seconds
      std::vector<PonUser>::iterator vIt = availableUsers.begin()+randUser;
      Request* req = new Request(reqTime, *vIt, content);
      scheduler->schedule(req);
      // erase the pon nodes from the available requesters (1 request
      // per node per content)
      availableUsers.erase(vIt);
      if (availableUsers.size() == 0) {
        // we ran out of requesters so we cannot allocate any more requests
        std::cout << "TestRun::execute() - not enough PON nodes to serve "
                "requests for content " << content->getName() << std::endl;
        break;
      }
    }
   */    
  }
}