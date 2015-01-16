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
#include "boost/heap/binomial_heap.hpp"

typedef typename boost::heap::binomial_heap<Flow *, 
        boost::heap::compare<CompareFlowPtr> > FlowQueue;
typedef FlowQueue::handle_type handleT;

class Scheduler {
protected:
  SimMode mode;
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

