/* 
 * File:   IPTVContentHelper.hpp
 * Author: dipascae
 *
 * Created on 14 January 2013, 17:31
 */

#ifndef IPTVCONTENTHELPER_HPP
#define	IPTVCONTENTHELPER_HPP

#include "ContentHelper.hpp"
#include "zipf_distribution.hpp"
#include "SimTimeInterval.hpp"

// % of requests taking places at a give hour, starting from midnight
const double usrPctgByHour[] = {
  4, 2, 1.5, 1.5, 1, 0.5, 0.5, 1, 1, 1.5, 2, 2.5, 3, 4, 4.5, 4.5, 4, 4, 4, 6, 12, 15, 12, 8
//0  1   2    3   4   5    6   7  8   9   10  11 12 13  14   15  16 17 18 19  20  21  22  23
};
// weight of the days of the week when determining request scheduling (M->S)
const double dayWeights[] = {0.8, 0.9, 1, 0.8, 1.2, 1.3, 1.2};
//                            M    T   W   T    F    S    S

/* Switching to a linear session length, this will no longer be used
// average session length (percentage) for the 0-25% quartile (most popular)
const double sessionLength1Q[] = {0.18, 0.20, 0.23, 0.27, 0.30, 0.32, 0.39, 0.48, 0.57, 0.70};

// average session length (percentage) for the 25-50% quartile
const double sessionLength2Q[] = {0.19, 0.21, 0.29, 0.33, 0.36, 0.45, 0.54, 0.59, 0.63, 0.70};

// average session length (percentage) for the 50-75% quartile
const double sessionLength3Q[] = {0.20, 0.30, 0.35, 0.42, 0.49, 0.53, 0.57, 0.59, 0.63, 0.80};

// average session length (percentage) for the 75-100% quartile (least popular)
const double sessionLength4Q[] = {0.20, 0.30, 0.33, 0.37, 0.43, 0.49, 0.56, 0.60, 0.65, 0.81};
*/

const double sessionLength[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1,
        1, 1, 1, 1, 1, 1, 1, 1}; // 50% linear zapping, 50% entire content 

class IPTVContentHelper: public ContentHelper {
protected:
  uint contentNum;
  boost::random::zipf_distribution<>* relDayDist;
  boost::random::zipf_distribution<>* rankDist;
  /* the daily catalog contains all the available contents sorted by day of
   * release, with dailyCatalog[0] being the content released in the current
   * day and dailyCatalog[6] the content released 6 days ago (and thus about 
   * to expire).
   */
  std::vector< std::vector<ContentElement*> > dailyCatalog;
public:
  IPTVContentHelper(uint contentNum);
  std::vector<ContentElement*> populateCatalog();
  void generateRoundRequests(Scheduler* scheduler);
  void endRound(std::vector<ContentElement*> *toErase,
                 std::vector<ContentElement*> *toAdd);
  ContentElement* generateSingleRequest();
};


#endif	/* IPTVCONTENTHELPER_HPP */

