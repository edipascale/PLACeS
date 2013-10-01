/* 
 * File:   TopologyOracle.hpp
 * Author: manuhalo
 *
 * Created on 25 giugno 2012, 10.36
 */

#ifndef VODTOPOLOGYORACLE_HPP
#define	VODTOPOLOGYORACLE_HPP

#include "TopologyOracle.hpp"
#include "UGCPopularity.hpp"



/* The TopologyOracle keeps track of what everyone is caching and uses that
 * information to match requesters to sources with the appropriate content.
 * Essentially it serves as a global tracker (in the BitTorrent sense).
 * We keep both a map of all the user caching a particular content (to fetch
 * potential sources quickly) and a map of what each individual user is caching
 * (to be able to simulate finite-size caching).
 */
class VoDTopologyOracle: public TopologyOracle {
protected:
  UGCPopularity* popularity;
  std::list<ContentElement*> beforePeak, atPeak, afterPeak;  
  void scheduleRequests(std::list<ContentElement*> list, Scheduler* scheduler);
  
public:
  VoDTopologyOracle(Topology* topo, po::variables_map vm);
  ~VoDTopologyOracle();
  void generateUserViewMap(Scheduler* scheduler);
  void populateCatalog();
  void updateCatalog(uint currentRound);
  void generateNewRequest(PonUser user, SimTime time, Scheduler* scheduler);
  /* FIXME: Implement pre-caching for VoD. It requires changing the moment it is 
   * invoked to after the requests are scheduled (so that we can figure out
   * which contents will be popular) but in that case it should be called at the
   * beginning of every round and I still haven't decided what to do with it
   */
  void preCache() {
    BOOST_LOG_TRIVIAL(error) << "Pre-caching currently not implemented for VoD "
            "simulations, aborting";
    abort();
  };
};

#endif	/* VODTOPOLOGYORACLE_HPP */

