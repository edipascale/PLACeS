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
  SimTime simTime; /**< The current time of this round of the simulation. */
  FlowQueue pendingEvents; /**< The event queue, where new Flows are sorted by increasing ETA. */
  TopologyOracle* oracle; /**< A pointer to the TopologyOracle for this simulation. */
  std::map<Flow*, handleT> handleMap; /**< A map matchign Flows with their handles in the event queue. It is required to update the ordering whenever the ETA for a Flow changes. */
  SimTime roundDuration; /**< The number of seconds that a simulation round should last in the current SimMode. */
  uint currentRound; /**< The index of teh current round. The first round has index 0. */
  SimTime snapshotFreq; /**< The frequency at which we should take graphml snapshots of the network, in seconds. If 0, no snapshot will be taken. */
  Flow* terminate; /**< A pointer to the termination Flow, which indicates that the current round is finised. */
  Flow* snapshot; /**< a pointer to the snapshot Flow, which indicates that a snapshot of the network should be exported to graphml. */
public:
/**
 * Simple constructor.
 * @param oracle The TopologyOracle for the current simulation.
 * @param vm The map of command line parameters specified by the user. Options for these parameters are specified in Places.cpp. 
 */
  Scheduler(TopologyOracle* oracle, po::variables_map vm);
  ~Scheduler();
  /**
   * Processes the next event in the queue. If the next event has an ETA greater
   * than the current time, the latter is moved forward accordingly.
   * @return True if the event was processed normally, False if the termination event was encountered, meaning that we need to finish the current round.
   */
  bool advanceClock(); 
  /**
   * Add a new Flow event to the queue.It will be inserted in the right position
   * based on its ETA, and its handle will be saved in the handleMap.
   * @param event The Flow event to be added to the queue.
   */
  void schedule(Flow* event); 
  /**
   * Updates the order of the event queue after one of the Flows has changed its 
   * ETA.
   * @param flow The Flow event whose ETA has changed.
   * @param oldEta The ETA of the event before the change. Required to understand whether the ETA has increased or decreased.
   */
  void updateSchedule(Flow* flow, SimTime oldEta);
  
  /**
   * Retrieve the current simulation time.
   * @return The current simulation time.
   */
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
  
  /**
   * Performs a number of maintenance operations before starting a new simulation
   * round. The events that are still in the queue (because their ETA was after
   * the end of the previous round) are moved to a new ETA relative to the starting
   * round. Events related to ContentElement that expired are deleted, and the related
   * resources in the Topology must be freed. New terminate and snapshot Flows are
   * generated for the round about to start.
   */
  void startNewRound();

};


#endif	/* SCHEDULER_HPP */

