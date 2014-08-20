/* 
 * File:   Cache.hpp
 * Copyright: Emanuele Di Pascale (dipascae aT tcd dOt ie)
 * 
 * Licensed under the Apache License v2.0 (see attached README.TXT file)
 * Created on 30 April 2013, 11:23
 */

#ifndef CACHE_HPP
#define	CACHE_HPP

#include <map>
#include <set>
#include <assert.h>
#include <iostream>
#include <limits>
#include "RunningAvg.hpp"

/* Policy to use in caches to replace old content and make space for new one
 * LRU = Least Recently Used, LFU = Least Frequently Used
 */
enum CachePolicy {LRU, LFU};

/* Struct to hold the caching parameters associated to each content element;
 * Timestamp must be a scalar which supports std::numeric_limits<Timestamp>::max()
 * Size can be any scalar which supports comparison operators (e.g. <,>,>= etc.)
 * uploads is used to calculate the bandwidth used and to ensure that we do not
 * erase an element which is currently required
 */
template <typename Timestamp, typename Size>
struct CacheEntry {
  Timestamp lastAccessed;
  unsigned int timesServed;
  Size size;
  unsigned int uploads;
};

/* Implements a generic LRU or LFU cache, with a fixed maximum capacity maxSize
 * and elements of type Content which can be of variable size. Implemented over
 * std::map (not optimized for performance!)
 */
template <typename Content, typename Size, typename Timestamp> 
class Cache {
  typedef std::map<Content, CacheEntry<Timestamp, Size> > CacheMap;
protected:
  CacheMap cacheMap;
  Size maxSize;
  Size currentSize;
  CachePolicy policy;
  RunningAvg<double, Timestamp> cacheOccupancy;
  void updateOccupancy(Timestamp time) {
    double occ = 100 * currentSize / maxSize;
    bool result = cacheOccupancy.add(occ, time);
    if (!result) {
      std::cerr << "Failed to update the cache occupancy at time " 
              << time << ", last timestamp: " << cacheOccupancy.getLastTimestamp()
              << "; aborting. " << std::endl;
      abort();
    }
  }
  
public:
  friend class CacheTestClass;
  Cache(Size maxSize, CachePolicy policy = LRU);
  std::pair<bool, std::set<Content> > addToCache(Content content, 
      Size size, Timestamp time);
  void clearCache();
  // to update metadata about the content (lastAccess, timesAccessed, uploads..)
  bool getFromCache(Content content, Timestamp time);
  bool isCached(Content content, Size sizeReq);
  void removeFromCache(Content content, Timestamp time); // for expired content
  bool uploadCompleted(Content content); // to decrease the upload counter
  int getCurrentUploads(Content content);
  int getTotalUploads();
  double getAvgOccupancy(Timestamp time) {
    double avg = cacheOccupancy.extract(time);
    return avg;
  }
  void resetOccupancy(double value, Timestamp time) {
    cacheOccupancy.reset(value, time);
  }
  
  // to give the optimizer full access to the content of the cache
  CacheMap getCacheMap() const {
    return this->cacheMap;
  }
  
  unsigned int getNumElementsCached() const {
    return this->cacheMap.size();
  }

  Size getCurrentSize() const {
    return this->currentSize;
  }

  Size getMaxSize() const {
    return maxSize;
  }
  
  bool fitsInCache(Size size) const {
    if (maxSize - currentSize >= size)
      return true;
    else
      return false;
  }

};

template <typename Content, typename Size, typename Timestamp>
Cache<Content, Size, Timestamp>::Cache(Size maxSize, CachePolicy policy) : cacheOccupancy() {
  this->maxSize = maxSize;
  this->policy = policy;
  this->currentSize = 0;
}

template <typename Content, typename Size, typename Timestamp>
std::pair<bool, std::set<Content> > Cache<Content, Size, Timestamp>::addToCache(
        Content content, Size size, Timestamp time) {
  std::set<Content> deletedElements;
  deletedElements.clear();
  // check if the content was already cached
  typename CacheMap::iterator cIt = cacheMap.find(content);
  if ((cIt != cacheMap.end() && cIt->second.size >= size) // content already cached
      || size > getMaxSize()) { // content cannot possibly fit in the cache
    // already cached or too big to be cached, quit 
    return std::make_pair(false, deletedElements);
  }  
  else {
    unsigned int oldFreqStat = 0;
    if (cIt!= cacheMap.end()) {
      // the content was cached, but with a smaller chunk, delete it (but save
      // the caching info - after all it's the same content)
      this->currentSize -= cIt->second.size;
      oldFreqStat = cIt->second.timesServed;
      /* FIXME: if something goes wrong and we cannot cache the new element,
       * we will lose the previous (partial) copy
       */
      this->cacheMap.erase(cIt);
    }
    while (currentSize + size > maxSize) {
      // Replace content according to selected policy
      Timestamp minTmp = std::numeric_limits<Timestamp>::max();
      typename CacheMap::iterator minIt = cacheMap.end();
      switch (policy) {
        case LRU:          
          for (typename CacheMap::iterator it = cacheMap.begin();
                  it != cacheMap.end(); it++) {
            if (it->second.uploads == 0 && it->second.lastAccessed < minTmp) {
              minTmp = it->second.lastAccessed;
              minIt = it;
            }              
          }
          break;
        case LFU:
          for (typename CacheMap::iterator it = cacheMap.begin();
                  it != cacheMap.end(); it++) {
            if (it->second.uploads == 0 && it->second.timesServed < minTmp) {
              minTmp = it->second.timesServed;
              minIt = it;
            }              
          }
          break;
        default:
          std::cerr << "ERROR: Cache::addToCache - unrecognized CachePolicy"
                  << std::endl;
      }
      // check that there is an element we can erase (due to uploads)
      if (minIt == cacheMap.end()) {
        // all elements are being used for uploads, cannot cache
        return std::make_pair(false, deletedElements);
      }
      // else remove the identified element from the cache
      currentSize -= minIt->second.size;
      deletedElements.insert(minIt->first);
      cacheMap.erase(minIt);
    }
    // insert new element in the cache
    CacheEntry<Timestamp, Size> entry;
    entry.lastAccessed = time;
    entry.timesServed = oldFreqStat; // 0 if the content is new
    entry.size = size;
    entry.uploads = 0;
    if (cacheMap.insert(std::make_pair(content,entry)).second == true) {
      currentSize += size;
      assert(currentSize <= maxSize);
      updateOccupancy(time);
      return std::make_pair(true, deletedElements);
    } else {
      std::cerr << "WARNING: Cache::addToCache() - Could not insert content "
              << std::endl;
      updateOccupancy(time);
      return std::make_pair(false, deletedElements);
    }
  }
}

template <typename Content, typename Size, typename Timestamp>
void Cache<Content, Size, Timestamp>::clearCache() {
  cacheMap.clear();
  this->currentSize = 0;
  cacheOccupancy.reset(0,0);
}

template <typename Content, typename Size, typename Timestamp>
bool Cache<Content, Size, Timestamp>::getFromCache(Content content, Timestamp time) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    it->second.lastAccessed = time;
    it->second.timesServed++;
    it->second.uploads++;
    return true;
  } 
  else
    return false;
}

template <typename Content, typename Size, typename Timestamp>
bool Cache<Content, Size, Timestamp>::isCached(Content content, Size sizeReq) {
  typename CacheMap::iterator cIt = cacheMap.find(content);
  if (cIt != cacheMap.end() && cIt->second.size >= sizeReq)
    return true;
  else // shall we return size instead to allow for multiple sources?
    return false;
}

template <typename Content, typename Size, typename Timestamp>
void Cache<Content, Size, Timestamp>::removeFromCache(Content content, Timestamp time) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    this->currentSize -= it->second.size;
    assert(this->currentSize >= 0);
    cacheMap.erase(it);
    updateOccupancy(time);
  }
}

template <typename Content, typename Size, typename Timestamp>
bool Cache<Content, Size, Timestamp>::uploadCompleted(Content content) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    it->second.uploads = it->second.uploads - 1;
    assert(it->second.uploads >= 0);
    return true;
  } else {
    return false;
  }
}

template <typename Content, typename Size, typename Timestamp>
int Cache<Content, Size, Timestamp>::getCurrentUploads(Content content) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    return it->second.uploads;
  } else {
    return -1;
  }
}

template <typename Content, typename Size, typename Timestamp>
int Cache<Content, Size, Timestamp>::getTotalUploads() {
  int uploads = 0;
  for (typename CacheMap::iterator it = cacheMap.begin(); it != cacheMap.end(); it++)
    uploads += it->second.uploads;
  return uploads;
}

#endif	/* CACHE_HPP */

