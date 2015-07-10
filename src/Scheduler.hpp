/* 
 * File:   Scheduler.hpp
 * Author: emanuele
 *
 * Created on 03 May 2012, 17:20
 */

#ifndef SCHEDULER_HPP
#define	SCHEDULER_HPP

#include "Flow.hpp"
#include "TopologyOracle.hpp"
#include <map>
#include "boost/heap/pairing_heap.hpp"

typedef typename boost::heap::pairing_heap<Flow *, 
        boost::heap::compare<CompareFlowPtr> > FlowQueue;
typedef FlowQueue::handle_type handleT;

/**
 * The Scheduler manages the event queue of the simulation; it is responsible for
 * scheduling new events (in the form of Flows), keeping them sorted by increasing
 * ETAs, processing them one at a time and moving the clock forward whenever 
 * there are no more events at the current simulation time.
 */
class Scheduler {
protected:
  SimMode mode; /**< Determines the simulation mode, i.e., VoD or IPTV. */
  SimTime simTime;
  FlowQueue pendingEvents;
  TopologyOracle* oracle;
  std::map<Flow*, handleT> handleMap;
  SimTime roundDuration;
  uint currentRound;
  SimTime snapshotFreq;
  Flow* terminate;
  Flow* snapshot;
public:

  Scheduler(TopologyOracle* oracle, po::variables_map vm);
  ~Scheduler();
  bool advanceClock(); 
  void schedule(Flow* event); 
  void updateSchedule(Flow* flow, SimTime oldEta);
  
  SimTime getSimTime() const {
    return simTime;
  }  
  void setSimTime(SimTime time) {
    this->simTime = time;
  }
  
  uint getCurrentRound() const {
    return currentRound;
  }

  SimTime getRoundDuration() const {
    return roundDuration;
  }
  
  void startNewRound();

};


#endif	/* SCHEDULER_HPP */

