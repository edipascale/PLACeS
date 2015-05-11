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

/* Enum to define the kind of event:
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

// Describes one data flow, either between peers or from a cache to the user
class Flow {
protected:
  PonUser source;
  PonUser destination;
  SimTime start;
  SimTime eta;
  ContentElement* content; 
  uint chunkId;
  Capacity bandwidth;
  SimTime lastUpdate;
  Capacity sizeDownloaded;
  bool P2PFlow;
  FlowType flowType;

public:
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

 
};

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

