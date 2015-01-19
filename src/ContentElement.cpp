#include "ContentElement.hpp"
#include "TopologyOracle.hpp"

ContentElement::ContentElement(std::string name, int releaseDay,Capacity size) {
  this->name = name;
  this->releaseDay = releaseDay;
  this->size = size;
  this->viewsThisRound = 0;
}