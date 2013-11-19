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

/* Policy to use in caches to replace old content and make space for new one
 * LRU = Least Recently Used, LFU = Least Frequently Used
 */
enum CachePolicy {LRU, LFU};

/* Struct to hold the caching parameters associated to each content element;
 * Timestamp must be a scalar which supports std::numeric_limits<Timestamp>::max()
 * Size can be any scalar which supports comparison operators (e.g. <,>,>= etc.)
 */
template <typename Timestamp, typename Size>
struct CacheEntry {
  Timestamp lastAccessed;
  unsigned int timesServed;
  Size size;
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
  
public:
  friend class CacheTestClass;
  Cache(Size maxSize, CachePolicy policy = LRU);
  std::pair<bool, std::set<Content> > addToCache(Content content, 
      Size size, Timestamp time);
  void clearCache();
  bool getFromCache(Content content, Timestamp time);
  bool isCached(Content content, Size sizeReq);
  void removeFromCache(Content content); // for expired content
  
  unsigned int getNumElementsCached() const {
    return this->cacheMap.size();
  }

  Size getCurrentSize() const {
    Size computedSize = 0;
    for (auto it = cacheMap.begin(); it != cacheMap.end(); it++) {
      computedSize += it->second.size;
    }
    assert(computedSize == currentSize);
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
Cache<Content, Size, Timestamp>::Cache(Size maxSize, CachePolicy policy) {
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
            if (it->second.lastAccessed < minTmp) {
              minTmp = it->second.lastAccessed;
              minIt = it;
            }              
          }
          // remove the least recently used element from the cache
          currentSize -= minIt->second.size;
          deletedElements.insert(minIt->first);
          cacheMap.erase(minIt);
          break;
        case LFU:
          for (typename CacheMap::iterator it = cacheMap.begin();
                  it != cacheMap.end(); it++) {
            if (it->second.timesServed < minTmp) {
              minTmp = it->second.timesServed;
              minIt = it;
            }              
          }
          // remove the least frequently used element from the cache
          currentSize -= minIt->second.size;
          deletedElements.insert(minIt->first);
          cacheMap.erase(minIt);
          break;
        default:
          std::cerr << "ERROR: Cache::addToCache - unrecognized CachePolicy"
                  << std::endl;
      }
    }
    // insert new element in the cache
    CacheEntry<Timestamp, Size> entry;
    entry.lastAccessed = time;
    entry.timesServed = oldFreqStat; // 0 if the content is new
    entry.size = size;
    if (cacheMap.insert(std::make_pair(content,entry)).second == true) {
      currentSize += size;
      assert(currentSize <= maxSize);
      return std::make_pair(true, deletedElements);
    } else {
      std::cerr << "WARNING: Cache::addToCache() - Could not insert content "
              << std::endl;
      return std::make_pair(false, deletedElements);
    }
  }
}

template <typename Content, typename Size, typename Timestamp>
void Cache<Content, Size, Timestamp>::clearCache() {
  cacheMap.clear();
  this->currentSize = 0;
}

template <typename Content, typename Size, typename Timestamp>
bool Cache<Content, Size, Timestamp>::getFromCache(Content content, Timestamp time) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    it->second.lastAccessed = time;
    it->second.timesServed++;
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
void Cache<Content, Size, Timestamp>::removeFromCache(Content content) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    this->currentSize -= it->second.size;
    assert(this->currentSize >= 0);
    cacheMap.erase(it);
  }
}

#endif	/* CACHE_HPP */

