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
    BOOST_LOG_TRIVIAL(error) << "Scheduler::schedule() - could not insert new handle in "
            "handleMap";
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
    BOOST_LOG_TRIVIAL(error) << "Scheduler::Scheduler() - unrecognized SimMode";
    abort();
  }
  this->oracle = oracle;
  this->currentRound = 0;
  this->roundDuration = roundDuration;
  simTime = 0;
  this->terminate = new Flow(nullptr, UNKNOWN, roundDuration+1);
  this->terminate->setFlowType(FlowType::TERMINATE);
  this->schedule(terminate);
  this->snapshotFreq = vm["snapshot-freq"].as<uint>();
  if (snapshotFreq > 0 && snapshotFreq <= roundDuration) {
    this->snapshot = new Flow(nullptr, UNKNOWN, snapshotFreq);
    snapshot->setFlowType(FlowType::SNAPSHOT);
    this->schedule(this->snapshot);
  } else {
    this->snapshot = nullptr;
  }
}

bool Scheduler::advanceClock() {
  if (pendingEvents.size() == 0) {
    BOOST_LOG_TRIVIAL(warning) << "Scheduler::advanceClock() - Empty event queue before "
            "reaching the termination event" << std::endl;
    return false;
  }
  Flow* nextEvent = const_cast<Flow*> (pendingEvents.top());
  // Check that the event is not scheduled in the past
  if (nextEvent->getSimTime() < this->getSimTime()) {
    BOOST_LOG_TRIVIAL(error) << "Scheduler::advanceClock() - Event scheduled in the past!";
    BOOST_LOG_TRIVIAL(error) << "Simulation time: " << this->getSimTime()
            << ", event time: " << nextEvent->getSimTime()
            << ", content: " << nextEvent->getContent()->getName();
    abort();
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
  handleMap.erase(pendingEvents.top());
  pendingEvents.pop();
  // Determine what kind of event is this
  switch(nextEvent->getFlowType()) {
    case FlowType::TERMINATE:
      BOOST_LOG_TRIVIAL(info) << std::endl  
             << "Scheduler::advanceClock() - intercepted termination event";
      delete nextEvent;
      return false;
    
    case FlowType::SNAPSHOT:
      oracle->takeSnapshot(this->getSimTime(), this->getCurrentRound());
      delete nextEvent;
      if (this->getSimTime() + snapshotFreq <= roundDuration) {
        this->snapshot = new Flow(nullptr, UNKNOWN,
                this->getSimTime() + snapshotFreq);
        snapshot->setFlowType(FlowType::SNAPSHOT);
        this->schedule(this->snapshot);
      } else {
        this->snapshot = nullptr;
      }
      return true;
    
    case FlowType::REQUEST: {
      // Change the start time to now and the eta to INF_TIME until we know the 
      // available bandwidth 
      nextEvent->setStart(this->getSimTime());
      nextEvent->setEta(INF_TIME);
      nextEvent->setLastUpdate(this->getSimTime());
      // Ask the TopologyOracle to find a source for this content
      // TODO: reschedule request in case of congestion
      bool success = oracle->serveRequest(nextEvent, this);
      // Check that the flow wasn't "virtual" (i.e. content was cached at the dest)
      // also in that case we need to put the chunk in the watching buffer
      if (nextEvent->getSource() == nextEvent->getDestination()) {
        oracle->notifyCompletedFlow(nextEvent, this);
        handleMap.erase(nextEvent);
        delete nextEvent;
      }
      else if (!success) {
        //TODO: retry to fetch the content at a later time
        handleMap.erase(nextEvent);
        delete nextEvent;
      }
      return true;
    }
    case FlowType::TRANSFER:
    case FlowType::WATCH:
      // Notify the oracle, which will update the cache mapping and free resources
      // in the topology
      oracle->notifyCompletedFlow(nextEvent, this);
      delete nextEvent;
      return true;      
  }  
  
/*  if (nextEvent->getSource() == UNKNOWN) {
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
      else if (nextEvent->getFlowType() == FlowType::SNAPSHOT) {
        oracle->takeSnapshot(this->getSimTime(), this->getCurrentRound());
        delete nextEvent;
        if (this->getSimTime() + snapshotFreq <= roundDuration) {
          this->snapshot = new Flow(nullptr, UNKNOWN, 0, 
                  this->getSimTime() + snapshotFreq);
          snapshot->setFlowType(FlowType::SNAPSHOT);
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
 */ 
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
    BOOST_LOG_TRIVIAL(error) << "Scheduler::updateSchedule() - could not find handle for flow "
            << flow->getSource().first << "->" << flow->getDestination().first;
    exit(ERR_NO_EVENT_HANDLE);
  }    
}

void Scheduler::startNewRound() {
  // update flows so that they can be moved to the new round
  std::vector<Flow*> flowVec;
  while (pendingEvents.empty() == false) {
    Flow* f = const_cast<Flow*> (pendingEvents.top());
    pendingEvents.pop();
    if (f->getFlowType() == FlowType::WATCH) {
      // there is no point in carrying over watch flows, as they will be discarded
      BOOST_LOG_TRIVIAL(trace) << "Deleting watch flow for chunk" << f->getChunkId()
              << " of content " << f->getContent()->getName() << " for user "
              << f->getDestination().first << "," << f->getDestination().second;
      delete f;
      continue;
    }
    f->updateSizeDownloaded(this->roundDuration);
    f->setLastUpdate(0);
    f->setStart(f->getStart() - this->roundDuration); // negative
    SimTime oldEta = f->getEta();
    if (oldEta < this->roundDuration) {
      BOOST_LOG_TRIVIAL(error) << "Scheduler::startNewRound() - unresolved event has eta "
              << oldEta << " < roundDuration";
      abort();
    }
    f->setEta(oldEta - this->roundDuration);
    if (this->mode == IPTV && f->getContent()->getReleaseDay() <= currentRound-6) {
      // expired contents in IPTV will be deleted so flows can't be completed, 
      // this event should have been truncated last round
      f->setContent(nullptr);
      BOOST_LOG_TRIVIAL(info) << "Scheduler::startNewRound() - carried over flow "
              "with expired content will not be completed";
      if (f->getFlowType() == FlowType::TRANSFER) {
        // this is hacky, but needs to be done. Ideally we should not get here at all.
        oracle->getTopology()->updateCapacity(f, this, false);
      }
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
  this->terminate = new Flow(nullptr, UNKNOWN, roundDuration+1);
  this->terminate->setFlowType(FlowType::TERMINATE);
  this->schedule(terminate);
  if (snapshotFreq > 0 && snapshotFreq <= roundDuration) {
    this->snapshot = new Flow(nullptr, UNKNOWN, snapshotFreq);
    snapshot->setFlowType(FlowType::SNAPSHOT);
    this->schedule(this->snapshot);
  } else {
    this->snapshot = nullptr;
  }
  return;
}