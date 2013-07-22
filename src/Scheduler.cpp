#include <iostream>
#include "Scheduler.hpp"

Scheduler::~Scheduler() {
  // delete eventual flows still scheduled but not completed
  handleMap.clear();
  pendingEvents.clear();
  delete terminate;
}

void Scheduler::schedule(Flow* event) {
  handleT handle = this->pendingEvents.push(event);
  if (handleMap.insert(std::make_pair(event, handle)).second != true)
  {
    std::cerr << "ERROR: Scheduler::schedule() - could not insert new handle in "
            "handleMap" << std::endl;
    exit(ERR_HANDLEMAP_INSERT);
  }
}

Scheduler::Scheduler(TopologyOracle* oracle, po::variables_map vm) 
{
  this->mode = (SimMode) vm["sim-mode"].as<uint>();
  if (this->mode == IPTV)
    this->roundDuration = 86400; // 1 day
  else if (this->mode == VoD)
    this->roundDuration = 604800; // 1 week
  else {
    std::cerr << "ERROR: Scheduler::Scheduler() - unrecognized SimMode" << std::endl;
    abort();
  }
  this->oracle = oracle;
  this->currentRound = 0;
  this->roundDuration = roundDuration;
  simTime = 0;
  this->terminate = new Flow(nullptr, UNKNOWN, 0, roundDuration+1);
  this->schedule(terminate);
  this->snapshotFreq = vm["snapshot-freq"].as<uint>();
  if (snapshotFreq > 0 && snapshotFreq <= roundDuration) {
    this->snapshot = new Flow(nullptr, UNKNOWN, SNAPSHOT, snapshotFreq);
    this->schedule(this->snapshot);
  } else {
    this->snapshot = nullptr;
  }
}

bool Scheduler::advanceClock() {
  if (pendingEvents.size() == 0) {
    std::cerr << "WARNING: Scheduler::advanceClock() - Empty event queue before "
            "reaching the termination event" << std::endl;
    return false;
  }
  Flow* nextEvent = const_cast<Flow*> (pendingEvents.top());
  // Check that the event is not scheduled in the past
  if (nextEvent->getSimTime() < this->getSimTime()) {
    std::cerr << "Scheduler::advanceClock() - Event scheduled in the past!" << 
            std::endl;
    std::cerr << "Simulation time: " << this->getSimTime()
            << ", event time: " << nextEvent->getSimTime()
            << ", content: " << nextEvent->getContent()->getName()
            << std::endl;
    abort();
    //exit(ERR_EVENT_IN_THE_PAST);
  }
  // Update the clock with the current event time (if > previous time)
  else if (nextEvent->getSimTime() > this->getSimTime()) {
    this->setSimTime(nextEvent->getSimTime());
    /* print current time on the screen at the current line (note: will mess up
     * printing with debug verbose
     */
    std::cout<<"Current simulation time: " << this->simTime << "/"
          << this->roundDuration << "\r" << std::flush;
  }
  // Determine what kind of event is this
  if (nextEvent->getSource() == UNKNOWN) {
    // This is a request, as the source hasn't been assigned yet
    handleMap.erase(pendingEvents.top());
    pendingEvents.pop();
    // Check whether this is a special event
    if (nextEvent->getDestination() == UNKNOWN && nextEvent->getContent() == nullptr) {
      // termination event?
      if (nextEvent->getSizeRequested() == 0) {
      std::cout << std::endl  
             << "Scheduler::advanceClock() - intercepted termination event"
              << std::endl;
      delete nextEvent;
      return false;
      } // snapshot event?
      else if (nextEvent->getSizeRequested() == SNAPSHOT) {
        oracle->takeSnapshot(this->getSimTime(), this->getCurrentRound());
        delete nextEvent;
        if (this->getSimTime() + snapshotFreq <= roundDuration) {
          this->snapshot = new Flow(nullptr, UNKNOWN, SNAPSHOT, 
                  this->getSimTime() + snapshotFreq);
          this->schedule(this->snapshot);
        }
        else {
          this->snapshot = nullptr;
        }
        return true;
      }
    }
    
    // Change the start time to now and the eta to INF_TIME until we know the 
    // available bandwidth 
    nextEvent->setStart(this->getSimTime());
    nextEvent->setEta(INF_TIME);
    nextEvent->setLastUpdate(this->getSimTime());
    // Ask the TopologyOracle to find a source for this content
    // TODO: reschedule request in case of congestion
    bool success = oracle->serveRequest(nextEvent, this);
    // Check that the flow wasn't "virtual" (i.e. content was cached at the dest)
    if (!success || nextEvent->getSource() == nextEvent->getDestination()) {
      handleMap.erase(nextEvent);
      delete nextEvent;
    }
    return true;
  } else {
//    std::cout << this->getSimTime() << ": Completed transfer of content " 
//            << flow->getContent()->getName()
//            << " from source " << flow->getSource().first
//            << " to destination " << flow->getDestination().first << std::endl;
    // Remove flow from top of the queue
    pendingEvents.pop();
    handleMap.erase(nextEvent);
    // Notify the oracle, which will update the cache mapping and free resources
    // in the topology
    oracle->notifyCompletedFlow(nextEvent, this);
    delete nextEvent;
    return true;
  }  
}


void Scheduler::updateSchedule(Flow* flow, SimTime oldEta) {
  if (handleMap.find(flow) != handleMap.end()) {
    if (flow->getEta() > oldEta)
      pendingEvents.decrease(handleMap[flow]);
    else if (flow->getEta() < oldEta)
      pendingEvents.increase(handleMap[flow]);
    else
      return;
  }
  else {
    std::cerr << "Scheduler::updateSchedule() - could not find handle for flow "
            << flow->getSource().first << "->" << flow->getDestination().first
            << std::endl;
    exit(ERR_NO_EVENT_HANDLE);
  }    
}

void Scheduler::startNewRound() {
  // update flows so that they can be moved to the new round
  std::vector<Flow*> flowVec;
  while (pendingEvents.empty() == false) {
    Flow* f = const_cast<Flow*> (pendingEvents.top());
    pendingEvents.pop();
    f->updateSizeDownloaded(this->roundDuration);
    f->setLastUpdate(0);
    f->setStart(f->getStart() - this->roundDuration); // negative
    SimTime oldEta = f->getEta();
    if (oldEta < this->roundDuration) {
      std::cerr << "ERROR: Scheduler::startNewRound() - unresolved event has eta "
              << oldEta << " < roundDuration" << std::endl;
      abort();
    }
    f->setEta(oldEta - this->roundDuration);
    if (this->mode == IPTV && f->getContent()->getReleaseDay() <= currentRound-6) {
      // expired contents in IPTV will be deleted so flows can't be completed, 
      // this event should have been truncated last round
      f->setContent(nullptr);
      std::cout << "WARNING - Scheduler::startNewRound() - carried over flow "
              "with expired content will not be completed" << std::endl;
    } else
      flowVec.push_back(f);
  }
  handleMap.clear();
  BOOST_FOREACH (Flow* f, flowVec) {
    this->schedule(f);
  }
  flowVec.clear();
  this->simTime = 0;
  this->currentRound++;
  this->terminate = new Flow(nullptr, UNKNOWN, 0, roundDuration+1);
  this->schedule(terminate);
  if (snapshotFreq > 0 && snapshotFreq <= roundDuration) {
    this->snapshot = new Flow(nullptr, UNKNOWN, SNAPSHOT, snapshotFreq);
    this->schedule(this->snapshot);
  } else {
    this->snapshot = nullptr;
  }
  return;
}