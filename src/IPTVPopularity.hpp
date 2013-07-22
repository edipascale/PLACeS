/* 
 * File:   IPTVPopularity.hpp
 * Author: dipascae
 *
 * Created on 24 August 2012, 10:31
 */

#ifndef IPTVPOPULARITY_HPP
#define	IPTVPOPULARITY_HPP
#include "ContentElement.hpp"
#include <vector>

const double dailyCoeff[] = {0.417910, 0.208955, 0.139303, 0.104478, 0.083582, 
      0.069652, 0.059701};

class IPTVPopularity {
protected:
  uint N;
  double q;
  double s;
  std::vector<double> rankCoeff;
  
public:
  IPTVPopularity(uint N = 3000, double q = 10, double s = 0.6);
  double getRankCoeff(uint rank);
  double getDailyCoeff (uint day);
  
};


#endif	/* IPTVPOPULARITY_HPP */

