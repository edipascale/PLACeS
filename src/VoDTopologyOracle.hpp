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



/**
 * Video-on-Demand version of the TopologyOracle. Based on the popularity model
 * described by Borghol et al. in their paper [1]. Currently deprecated as all
 * of the latest additions and modifications to the simulator have only been
 * tested for the IPTV model. 
 * The main difference with the IPTV oracle is that here the views for each
 * ContentElement are determined at the beginning of each round. Simulation
 * rounds last a week, rather than a day like in the IPTV case. Elements in the
 * catalog have a week of peek popularity, after which they decline.
 * 
 * [1] Y. Borghol, S. Mitra, S. Ardon, N. Carlsson, D. Eager, and A. Mahanti, 
 * “Characterizing and modelling popularity of user-generated videos,” 
 * Perform. Eval., vol. 68, no. 11, pp. 1037–1055, Nov. 2011.
 * @deprecated
 * @see UGCPopularity, TopologyOracle, IPTVTopologyOracle
 */
class VoDTopologyOracle: public TopologyOracle {
protected:
  UGCPopularity* popularity; /**< The popularity model used to generate views for the elements in the catalog. */
  std::list<ContentElement*> beforePeak, /**< The ContentElements that have not reached yet their week of peak popularity. */
          atPeak, /**< The ContentElements that are currently in their week of peak popularity. */
          afterPeak;  /**< The ContentElements that are past their week of peak popularity. */
  /**
   * Schedules the required number of views for the next simulation round for
   * each of the elements in the specified list. This is done by randomly 
   * selecting users from the Topology until the required number of views has
   * been reached or we run out of users.
   * @param list The list of ContentElements for which we need to schedule views.
   * @param scheduler A pointer to the Scheduler in order to queue the requests.
   */
  void scheduleRequests(std::list<ContentElement*> list, Scheduler* scheduler);
  
public:
  VoDTopologyOracle(Topology* topo, po::variables_map vm, uint roundDuration);
  ~VoDTopologyOracle();
  void generateUserViewMap(Scheduler* scheduler);
  void populateCatalog();
  void updateCatalog(uint currentRound);
  void generateNewRequest(PonUser user, SimTime time, Scheduler* scheduler);
  /**
   * @todo{Implement pre-caching for VoD. It requires changing the moment it is 
   * invoked to after the requests are scheduled (so that we can figure out
   * which contents will be popular) but in that case it should be called at the
   * beginning of every round and I still haven't decided what to do with it.}
   * @deprecated
   */
  void preCache() {
    BOOST_LOG_TRIVIAL(error) << "Pre-caching currently not implemented for VoD "
            "simulations, aborting";
    abort();
  };
};

#endif	/* VODTOPOLOGYORACLE_HPP */

