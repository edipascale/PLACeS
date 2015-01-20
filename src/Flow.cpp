#include <sstream>

#include "Flow.hpp"

Flow::Flow(ContentElement* content, PonUser destination, SimTime eta,
        uint chunkId, PonUser source, FlowType flowType) {
  this->source = source;
  this->destination = destination;
  this->content = content;
  this->eta = eta;
  this->bandwidth = 0;
  this->sizeDownloaded = 0;
  this->start = INF_TIME;
  this->lastUpdate = start;
  this->P2PFlow = true;
  this->flowType = flowType;
  this->chunkId = chunkId;
}

Flow::~Flow() {
  content = nullptr;
}

// debug method
std::string Flow::toString() const {
  std::stringstream ss;
  ss << "s" << this->getSource().first << "d" << this->getDestination().first <<
          " c:" << this->getContent()->getName() << ", t:"
          << this->getStart() <<  "-" << this->getEta() << "; dl "
          << this->getSizeDownloaded() << " @" << this->lastUpdate
          << ", bw:" << this->getBandwidth() << ", type: " 
          << (int) this->getFlowType();
  return ss.str(); 
}

void Flow::updateSizeDownloaded(SimTime now) {
  if (now > this->lastUpdate)
    this->sizeDownloaded += ((now - this->lastUpdate) * this->bandwidth);
  if (this->sizeDownloaded > this->content->getSize()) {
    /* This is inevitable due to approximations and having a discrete time scale
     */
    this->sizeDownloaded = this->content->getSize();
  }
  this->setLastUpdate(now);
}