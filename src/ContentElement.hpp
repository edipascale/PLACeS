/* 
 * File:   ContentElement.hpp
 * Author: emanuele
 *
 * Created on 27 April 2012, 21:22
 */

#ifndef CONTENTELEMENT_HPP
#define	CONTENTELEMENT_HPP

#include "PLACeS.hpp"
#include <string>
#include "boost/multi_index_container.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/hashed_index.hpp"
#include "boost/multi_index/mem_fun.hpp"

class ContentElement {    
protected:
    std::string name;
    Capacity size;
    unsigned int viewsThisRound; // in VoD these are the pre-calculated views for this round; in IPTV we are using this as a counter for the view requests
    int releaseDay; // after the merge with VoD, also works as peakingRound
    unsigned int weeklyRank;
    // add a cache time? not used at the moment anyway
    
public:
  // Standard content element assumed to be 5400 Mb = 30 min at 3 Mbps
  ContentElement(std::string name, int releaseDay, 
          unsigned int weeklyRank, Capacity size = 5400);
  
  std::string getName() const {
    return name;
  }

  unsigned int getViewsThisRound() const {
    return viewsThisRound;
  }

  void increaseViewsThisRound(unsigned int increase = 1) {
    this->viewsThisRound = viewsThisRound + increase;
  }
  
  void setViewsThisRound(unsigned int views) {
    this->viewsThisRound = views;
  }
  
  void resetViewsThisRound() {
    this->viewsThisRound = 0;
  }

  unsigned int getWeeklyRank() const {
    return weeklyRank;
  }
  
  void setWeeklyRank(unsigned int weeklyRank) {
    this->weeklyRank = weeklyRank;
  }

  void setName(std::string name) {
    this->name = name;
  }

  int getReleaseDay() const {
    return releaseDay;
  }
  
  // releaseDay works as the peakingRound for VoD
  int getPeakingRound() const {
    this->getReleaseDay();
  }

  void setReleaseDay(int releaseDay) {
    this->releaseDay = releaseDay;
  }

  // releaseDay works as the peakingRound for VoD
  void setPeakingRound(int peakingRound) {
    this->setReleaseDay(peakingRound);
  }
  
  Capacity getSize() const {
    return size;
  }

  void setSize(Capacity size) {
    this->size = size;
  }
};

#endif	/* CONTENTELEMENT_HPP */

