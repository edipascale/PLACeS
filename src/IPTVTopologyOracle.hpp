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
#include <queue>




/** 
 * IPTV specialization of the Locality Oracle for the current Topology.
 */
class IPTVTopologyOracle : public TopologyOracle{
protected:
 
  /**
   * Zipf distribution used to select a release day for a new content request.
   * Content released more recently have higher probability of being selected.
   */
  boost::random::zipf_distribution<>* relDayDist;
  /**
   * Zipf-Mandelbrot distribution used to select a content item from those
   * released on the selected day; lower ranks indicate a higher popularity.
   * The parameters of this distribution are extracted each day from shiftDist
   * and expDist.
   */
  boost::random::zipf_distribution<>* rankDist;
  /**
   * Distribution from which we extract the shift parameter for the
   * Zipf-Mandelbrot popularity distribution, which is going to change each day.
   */
  boost::random::uniform_int_distribution<>* shiftDist;
  /**
   * Distribution from which we extract the exponent parameter for the
   * Zipf-Mandelbrot popularity distribution, which is going to change each day.
   */
  boost::random::uniform_real_distribution<>* expDist;
  
  /**
   * The daily catalog contains all the available contents sorted by day of
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

