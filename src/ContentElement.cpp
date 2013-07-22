#include "ContentElement.hpp"
#include "TopologyOracle.hpp"

ContentElement::ContentElement(std::string name, int releaseDay, 
        unsigned int weeklyRank, Capacity size) {
  this->name = name;
  this->releaseDay = releaseDay;
  this->weeklyRank = weeklyRank;
  this->size = size;
  this->viewsThisRound = 0;
}