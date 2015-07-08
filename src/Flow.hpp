/* 
 * File:   Flow.hpp
 * Author: emanuele
 *
 * Created on 03 May 2012, 14:31
 */

#ifndef FLOW_HPP
#define	FLOW_HPP

#include "ContentElement.hpp"
#include "PLACeS.hpp"

/** Enum to define the kind of event:
 * REQUEST: Transfer yet to be initiated, has to get a source assigned
 * TRANSFER: Actual data transfer between a source and a destination
 * SNAPSHOT: take a snapshot of the network status and export it as graphml
 * TERMINATE: end the current round
 * WATCH: update the current watching position in the stream, for chunking
 */
enum class FlowType {
  REQUEST,
  TRANSFER,
  SNAPSHOT,
  TERMINATE,
  WATCH
};

/** Describes one data flow, either between peers or from a cache to the user.
 */ 
class Flow {
protected:
  PonUser source; /**< The source of the data flow, i.e., the network element pushing the content item to the destination. */
  PonUser destination; /**< The destination of the data flow, i.e., the requester of the content item. */
  SimTime start; /**< The SimTime at which this flow started. */
  SimTime eta; /**< The SimTime at which this flow is estimated to be finishing. Can change depending on the available bandwdidth over this particular flow's route. It's used by the Scheduler to sort Flows in the PriorityQueue. */
  ContentElement* content; /**< The content item requested by the destination. */
  uint chunkId; /**< The identifier of the chunk of the content item being transmitted or requested. */
  Capacity bandwidth; /**< The bandwidth currently assigned to this flow, as determined by the Topology class. */
  SimTime lastUpdate; /**< The SimTime at which the metadata of this Flow were last updated, to calculate how much has been downloaded. */
  Capacity sizeDownloaded; /**< The amount of data that has been downloaded at SimTime lastUpdate. */
  bool P2PFlow; /**< True if this is a peer-to-peer flow, i.e., both source and destination are PonUsers. False otherwise.*/
  FlowType flowType; /**< The type of this Flow, as described in the FlowType enum. */

public:
  /** Class constructor.
   * 
   * @param content the ContentElement object of the request.
   * @param destination the user requesting the ContentElement
   * @param eta the estimated time of completion of the flow, which is also used by the Scheduler to sort flows (in ascending order).
   * @param chunkId the identifier of the specific chunk of the ContentElement that is being requested/downloaded.
   * @param flowType the type of this Flow, as described in the FlowType enum.
   * @param source the selected source for this flow, which has to own a copy of the required chunk of the requested ContentElement.
   */
  Flow(ContentElement* content, PonUser destination, SimTime eta = INF_TIME, 
          uint chunkId = 0, FlowType flowType = FlowType::REQUEST,
          PonUser source = UNKNOWN);  
  ~Flow();

  SimTime getLastUpdate() const {
    return lastUpdate;
  }

  void setLastUpdate(SimTime lastUpdate) {
    this->lastUpdate = lastUpdate;
  }

  Capacity getSizeDownloaded() const {
    return sizeDownloaded;
  }

  bool isP2PFlow() const {
    return P2PFlow;
  }

  void setP2PFlow(bool P2PFlow) {
    this->P2PFlow = P2PFlow;
  }

  void setSizeDownloaded(Capacity sizeDownloaded) {
    this->sizeDownloaded = std::min(sizeDownloaded, getContent()->getSize());
  }

  void updateSizeDownloaded(SimTime now); 
  
  ContentElement* getContent() const {
    return content;
  }

  void setContent(ContentElement* content) {
    this->content = content;
  }

  PonUser getDestination() const {
    return destination;
  }

  void setDestination(PonUser destination) {
    this->destination = destination;
  }

  SimTime getEta() const {
    return eta;
  }

  void setEta(SimTime eta) {
    this->eta = eta;
  }

  PonUser getSource() const {
    return source;
  }

  void setSource(PonUser source) {
    this->source = source;
  }

  SimTime getStart() const {
    return start;
  }

  void setStart(SimTime start) {
    this->start = start;
  }

  Capacity getBandwidth() const {
    return bandwidth;
  }

  void setBandwidth(Capacity bandwidth) {
    this->bandwidth = bandwidth;
  }
  
  // legacy method to ensure compatibility with Event
  SimTime getSimTime() const {
    return this->getEta();
  }
  void setSimTime(SimTime simTime) {
    this->setEta(simTime);
  }
  
  // debug method
  std::string toString() const;

  FlowType getFlowType() const {
    return flowType;
  }

  void setFlowType(FlowType flowType) {
    this->flowType = flowType;
  }
  
  uint getChunkId() const {
    return chunkId;
  }

  /**
   * Utility method to simply retrieve the Size of the chunk being requested/transmitted
   * through this Flow.
   * @return the Size of Flow::chunkId.
   */
  Capacity getChunkSize() const {
    const ChunkPtr chunk = this->content->getChunkById(this->chunkId);
    return chunk->getSize();
  } 
  
};

/** This function is what is used by the PriorityQueue in the Scheduler to sort
 *  Flows in ascending order of eta. The only catch is that Flows of type 
 *  FlowType::TERMINATE should always be put further down the queue compared
 *  with other types of event with the same eta.
 */
struct CompareFlowPtr : public std::binary_function < Flow*, Flow*, bool> {
  bool operator()(Flow* x, Flow * y) const {
    if (x->getSimTime() == y->getSimTime()) {
      if (x->getFlowType() == FlowType::TERMINATE)
        return true;
      else if (y->getFlowType() == FlowType::TERMINATE)
        return false;
    }
    return x->getSimTime() > y->getSimTime();
  }
};

#endif	/* FLOW_HPP */

