/* 
 * File:   TopologyOracle.hpp
 * Author: manuhalo
 *
 * Created on 25 giugno 2012, 10.36
 */

#ifndef IPTVTOPOLOGYORACLE_HPP
#define	IPTVTOPOLOGYORACLE_HPP

#include "TopologyOracle.hpp"
#include "zipf_distribution.hpp"
#include "boost/random/uniform_real_distribution.hpp"


/* The TopologyOracle keeps track of what everyone is caching and uses that
 * information to match requesters to sources with the appropriate content.
 * Essentially it serves as a global tracker (in the BitTorrent sense).
 * We keep both a map of all the user caching a particular content (to fetch
 * potential sources quickly) and a map of what each individual user is caching
 * (to be able to simulate finite-size caching).
 */
class IPTVTopologyOracle : public TopologyOracle{
protected:
  UserViewMap userViewMap;
  boost::random::zipf_distribution<>* relDayDist;
  boost::random::zipf_distribution<>* rankDist;
  /* the following distributions are used to generate the parameters for the
   * Zipf-Mandelbrot popularity distribution, which is going to change each
   * day.
   */
  boost::random::uniform_int_distribution<>* shiftDist;
  boost::random::uniform_real_distribution<>* expDist;
  
  /* the daily catalog contains all the available contents sorted by day of
   * release, with dailyCatalog[0] being the content released in the current
   * day and dailyCatalog[6] the content released 6 days ago (and thus about 
   * to expire).
   */
  std::vector< std::vector<ContentElement*> > dailyCatalog;
  
public:
  IPTVTopologyOracle(Topology* topo, po::variables_map vm, uint roundDuration);
  ~IPTVTopologyOracle();
  void generateUserViewMap(Scheduler* scheduler);
  void populateCatalog();
  void updateCatalog(uint currentRound);
  void generateNewRequest(PonUser user, SimTime time, Scheduler* scheduler);
  void preCache();
  void notifyEndRound(uint endingRound);
};

#endif	/* IPTVTOPOLOGYORACLE_HPP */

