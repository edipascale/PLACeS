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


// Describes one data flow, either between peers or from a cache to the user
class Flow {
protected:
  PonUser source;
  PonUser destination;
  SimTime start;
  SimTime eta;
  ContentElement* content; 
  Capacity bandwidth;
  SimTime lastUpdate;
  Capacity sizeDownloaded;
  Capacity sizeRequested; // to model zapping - partial downloads
  bool P2PFlow;

public:
  Flow(ContentElement* content, PonUser destination, Capacity sizeRequested,
          SimTime eta = INF_TIME, PonUser source = UNKNOWN);  
  ~Flow();

  SimTime getLastUpdate() const {
    return lastUpdate;
  }

  void setLastUpdate(SimTime lastUpdate) {
    this->lastUpdate = lastUpdate;
  }
  
  Capacity getSizeRequested() const {
    return sizeRequested;
  }

  void setSizeRequested(Capacity sizeRequested) {
    if (this->getContent() != nullptr)
      this->sizeRequested = std::min(sizeRequested, this->getContent()->getSize());
    else
      this->sizeRequested = sizeRequested;
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
    this->sizeDownloaded = std::min(sizeDownloaded, this->getSizeRequested());
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
 
};

struct CompareFlowPtr : public std::binary_function < Flow*, Flow*, bool> {
  bool operator()(Flow* x, Flow * y) const {
    return x->getSimTime() > y->getSimTime();
  }
};

#endif	/* FLOW_HPP */

