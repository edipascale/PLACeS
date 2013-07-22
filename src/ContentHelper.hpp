/* 
 * File:   ContentHelper.hpp
 * Author: dipascae
 *
 * Created on 14 January 2013, 16:47
 */

#ifndef CONTENTHELPER_HPP
#define	CONTENTHELPER_HPP
#include "Scheduler.hpp"
#include "ContentElement.hpp"

/* Abstract Class; manages ContentElements and interfaces with the popularity
 * models to generate content requests on behalf of users. Its methods are
 * made available to the TopologyOracle, which in turns keeps track of the users
 * in the system and their respective caches.
 * The virtual methods need to be overridden for the two specific cases of IPTV
 * and VoD.
 */
class ContentHelper {
public:
  /* This method instantiates the ContentElement that compose the catalog, and 
   * returns a vector of pointers to them so that the TopologyOracle can keep
   * track of what contents are available.
   */
  virtual std::vector<ContentElement*> populateCatalog() = 0;
  /* This method generates a set of requests basing on the popularity model
   * associated with the specific implementation of the content helper.
   */
  virtual void generateRoundRequests(Scheduler* scheduler) = 0;
  /* This method performs general maintenance at the end of the round, e.g. 
   * erasing expired content, generating new content etc, updating an object's
   * popularity etc.
   */
  virtual void endRound(std::vector<ContentElement*> *toErase,
                        std::vector<ContentElement*> *toAdd) = 0;
  
};

#endif	/* CONTENTHELPER_HPP */

