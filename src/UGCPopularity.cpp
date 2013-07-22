

#include "UGCPopularity.hpp"
#include "ContentElement.hpp"
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/random/gamma_distribution.hpp>
#include "boost/random/uniform_real_distribution.hpp"
#include <boost/math/distributions/lognormal.hpp>
#include "boost/random/uniform_int.hpp"
#include <algorithm>

extern boost::mt19937 gen;

UGCPopularity::UGCPopularity(unsigned int totalRounds,
        bool perturbations) {
  this->totalRounds = totalRounds;
  this->ttpDist = new boost::math::ttpDistribution(totalRounds);
  this->perturbations = perturbations;
}

UGCPopularity::~UGCPopularity() {
  delete ttpDist;
}

// generate views for all the content elements at a given phase for this round only
unsigned int UGCPopularity::generateViews(std::list<ContentElement*> contentList, 
      peakingPhase phase) {
  unsigned int bodyCount, tailCount;
  tailCount = std::floor(contentList.size() / 10);
  bodyCount = contentList.size() - tailCount;
  std::list<unsigned int> views;
  if (phase == BEFORE_PEAK) {
    for (unsigned int i = 0; i < tailCount; i++) {
      views.push_back(this->getBeforePeakTailViews());
    }
    for (unsigned int i = 0; i < bodyCount; i++) {
      views.push_back(this->getBeforePeakViews());
    }
  } else if (phase == AT_PEAK) {
    for (unsigned int i = 0; i < tailCount; i++) {
      views.push_back(this->getAtPeakTailViews());
    }
    for (unsigned int i = 0; i < bodyCount; i++) {
      views.push_back(this->getAtPeakViews());
    }
  } else { // phase == AFTER_PEAK
    for (unsigned int i = 0; i < tailCount; i++) {
      views.push_back(this->getAfterPeakTailViews());
    }
    for (unsigned int i = 0; i < bodyCount; i++) {
      views.push_back(this->getAfterPeakViews());
    }
  }
  // sort the views in descending order and assign them to the contentList
  // (which is ordered by ranking) so that relative ranking remains unaltered
  views.sort(std::greater<unsigned int>());
  std::list<unsigned int>::iterator viewsIt, mIt;
  
  // introduce perturbations in the number of views, by switching views between
  // videos in the same phase and at a maximum distance g
  if (this->perturbations && phase != AT_PEAK) {
    uint g = 12; // suggested value from Borghol et al.
    std::vector<std::pair<uint, uint> > winLimits;
    uint minWin, maxWin;
    uint maxViews = views.front();
    // compute valid switching windows for each content
    BOOST_FOREACH(uint v, views) {
      minWin = std::floor(v / g);
      maxWin = std::min(maxViews, v * g);
      winLimits.push_back(std::make_pair(minWin,maxWin));
    }
    // switch views a "sufficiently large number of times" (!) between contents
    // with compatible switching windows
    
    // NOTE: the following code is so ugly it makes me want to kill kittens. 
    // try to rewrite it in a half-decent way, ok?
    boost::random::uniform_int_distribution<> uniDist(0, contentList.size()-1);
    for (uint i = 0; i < contentList.size() / 3; i++) {
      uint randC = uniDist(gen);
      viewsIt = views.begin();
      for (uint j = 0; j < randC; j++)
        viewsIt++;
      mIt = viewsIt;
      int j = 0;
      while (mIt != views.begin() && *mIt >= winLimits[randC].first) {
        j--;
        mIt--;
      }
      if (j != 0) j++;
      mIt = viewsIt;
      int k = 0;
      while (mIt != views.end() && *mIt <= winLimits[randC].second) {
        mIt++;
        k++;
      }
      if (k != 0) k--;
      if (j != 0 || k != 0) {
        boost::random::uniform_int_distribution<> switchDist(0,k-j);
        j = switchDist(gen) + j;
        if (j < 0) {
          std::swap(winLimits[randC], winLimits[randC+j]);
          mIt = viewsIt;
          while (j < 0) {
            mIt--;
            j++;
          }
          std::iter_swap(viewsIt, mIt);          
        }
        else if (j > 0) {
          std::swap(winLimits[randC], winLimits[randC+j]);
          mIt = viewsIt;
          while (j > 0) {
            mIt++;
            j--;
          }
          std::iter_swap(viewsIt, mIt);          
        }
      }      
    }
  }
  // assign views in the intended order (sorted or permuted)
  uint totalViews = 0;
  viewsIt = views.begin();
  BOOST_FOREACH (ContentElement* content, contentList) {
    content->setViewsThisRound(*viewsIt);
    totalViews += *viewsIt;
    viewsIt++;
  }
  return totalViews;
}

// Returns the round at which a content will peak in terms of views
unsigned int UGCPopularity::generatePeakRound() {
  boost::uniform_01<> uni;
  double randVal = uni(gen);
//  std::cout << "Uniform Value: " << randVal << std::endl;
  return boost::math::quantile(*ttpDist, randVal);
}

unsigned int UGCPopularity::getBeforePeakTailViews() {
  boost::math::lognormal logDist(2.000, 2.135);
  boost::random::uniform_real_distribution<> uniDist(0.903, 1);
  double randValue = uniDist(gen);
  double retValue = boost::math::quantile(logDist, randValue);  
  return static_cast<unsigned int>(0.5+retValue);
}

unsigned int UGCPopularity::getAtPeakTailViews() {
  boost::math::lognormal logDist(-3.826, 3.477);
  boost::random::uniform_real_distribution<> uniDist(0.997, 1);
  double randValue = uniDist(gen);
  double retValue = boost::math::quantile(logDist, randValue);  
  return static_cast<unsigned int>(0.5+retValue);
}

unsigned int UGCPopularity::getAfterPeakTailViews() {
  boost::math::lognormal logDist(-0.356, 2.533);
  boost::random::uniform_real_distribution<> uniDist(0.931, 1);
  double randValue = uniDist(gen);
  double retValue = boost::math::quantile(logDist, randValue);  
  return static_cast<unsigned int>(0.5+retValue);
}

unsigned int UGCPopularity::getBeforePeakViews() {
  const unsigned int xMin = 0, xThresh = 119;
  unsigned int retValue = 0;
  boost::uniform_01<> uniDist;
  double randVal = uniDist(gen);
  // std::cout << "Uniform Value: " << randVal << std::endl;
  boost::math::beta_distribution<> betaDist(0.191, 1.330);
  retValue = std::floor(0.5+xMin+(xThresh-xMin)*
          boost::math::quantile(betaDist, randVal));  
  return retValue;
}

unsigned int UGCPopularity::getAtPeakViews() {
  const unsigned int xMin = 4, xThresh = 297;
  unsigned int retValue = 0;
  boost::uniform_01<> uniDist;
  double randVal = uniDist(gen);
  // std::cout << "Uniform Value: " << randVal << std::endl;  
  boost::math::beta_distribution<> betaDist(0.543, 2.259);
  retValue = std::floor(0.5+xMin+(xThresh-xMin)*
            boost::math::quantile(betaDist, randVal));
  return retValue;
}

unsigned int UGCPopularity::getAfterPeakViews() {
  const unsigned int xMin = 0, xThresh = 30;
  unsigned int retValue = 0;
  boost::uniform_01<> uniDist;
  double randVal = uniDist(gen);
  // std::cout << "Uniform Value: " << randVal << std::endl;
  boost::math::beta_distribution<> betaDist(0.077, 0.968);
  retValue = std::floor(0.5+xMin+(xThresh-xMin)*
            boost::math::quantile(betaDist, randVal));
  return retValue;
}
