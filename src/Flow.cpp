#include <sstream>

#include "Flow.hpp"

Flow::Flow(ContentElement* content, PonUser destination, SimTime eta,
        uint chunkId, FlowType flowType, PonUser source) {
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

/**
 * @deprecated Debug method, no longer used.
 * @return A string with a concatation of some of the interal fields of the class.
 */
std::string Flow::toString() const {
  std::stringstream ss;
  ss << "s" << this->getSource().first << "d" << this->getDestination().first <<
          ", c" << this->getContent()->getName() << ":" << this->getChunkId() <<
          ", t:" << this->getStart() <<  "-" << this->getEta() << "; dl "
          << this->getSizeDownloaded() << " @" << this->lastUpdate
          << ", bw:" << this->getBandwidth() << ", type: " 
          << (int) this->getFlowType();
  return ss.str(); 
}

/**
 * Updates Flow::sizeDownloaded by taking the previous value and adding to it
 * the Capacity that has been downloaded with the current Flow::bandwidth
 * between Flow::lastUpdate and now. 
 * @param now current SimTime.
 */
void Flow::updateSizeDownloaded(SimTime now) {
  if (now > this->lastUpdate)
    this->sizeDownloaded += ((now - this->lastUpdate) * this->bandwidth);
  if (this->sizeDownloaded > this->getChunkSize()) {
    /* This is inevitable due to approximations and having a discrete time scale
     */
    this->sizeDownloaded = this->getChunkSize();
  }
  this->setLastUpdate(now);
}