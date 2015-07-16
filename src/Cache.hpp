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

/**
 * Policy to use in caches to replace old content and make space for new ones.
 */
enum CachePolicy {
  LRU, /**< Least-Recently Used: delete the element with the oldest Timestamp. */
  LFU  /**< Least-Frequently Used: delete the element with the lowest timesServed. */
};

/**
 * Struct to hold the caching parameters associated to each content element;
 * Timestamp must be a scalar which supports std::numeric_limits<Timestamp>::max().
 * Size can be any scalar which supports comparison operators (e.g. <,>,>= etc.).
 */
template <typename Timestamp, typename Size>
struct CacheEntry {
  Timestamp lastAccessed; /**< Last time this item was accessed. Used with the LRU policy. */
  unsigned int timesServed; /**< Number of times this item was accessed. Used with the LFU policy. */
  Size size; /**< Size of the item, i.e., the amount of storage it occupies. */
  unsigned int uploads; /**< Number of current uploads of this item; used to calculate the bandwidth used and to ensure that we do not erase an element which is currently being pushed upstream. */
};

/**
 * Implements a generic LRU or LFU cache, with a fixed maximum capacity maxSize
 * and elements of type Content which can be of variable size. Based on
 * std::map (not optimized for performance!). All methods but addToCache()
 * accept const references to a Content, since it is assumed that it will be
 * some sort of shared pointer.
 */
template <typename Content, typename Size, typename Timestamp> 
class Cache {
  /**
   * A std::map associating each item of the Cache with its metadata.
   */
  typedef std::map<Content, CacheEntry<Timestamp, Size> > CacheMap;
protected:
  CacheMap cacheMap; /**< The content of the cache, i.e., the set of items cached together with their caching metadata. */
  Size maxSize; /**< The maximum storage space available on this Cache. */
  Size currentSize; /**< The current storage occupancy of this Cache. */
  CachePolicy policy; /**< The replacement policy used to make space for a new item when the Cache is full. */
  RunningAvg<double, Timestamp> cacheOccupancy; /**< A tracker of the time-weighted average occupancy of the cache. */
  /**
   * Updates the average Cache occupancy after a modification to the storage
   * (i.e., an element was erased or added).
   * @param time The Timestamp at which the modification occurred.
   */
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
  friend class CacheTestClass; /**< Class used for unit-testing purposes. */
  /**
   * Simple constructor.
   * @param maxSize The maximum space available in the Cache.
   * @param policy The replacement policy to be used when the Cache is full and a new element must be inserted.
   */
  Cache(Size maxSize, CachePolicy policy = LRU);
  /**
   * Attempt to insert a new item in the Cache.
   * @param content The item that we want to add to the Cache.
   * @param size The size of the item we are adding to the Cache.
   * @param time The Timestamp at which the insertion is taking place.
   * @return The first element of the pair is a bool that is true if content was successfully cached, false otherwise. The second element of the pair is a set of items that had to be deleted from the cache according to the replacement policy. This is required to update any external map keeping track of the content of the Cache.
   */
  std::pair<bool, std::set<Content> > addToCache(const Content content, 
      const Size size, const Timestamp time);
  /**
   * Delete all the items from the Cache.
   */
  void clearCache();
  /**
   * Updates the metadata related to a cached item after it has been accessed.
   * @param content The item that has just been accessed.
   * @param time The Timestamp at which the item has been accessed.
   * @param local True if the request is local to the owner of the Cache, meaning that it will not be uploaded anywhere. False otherwise.
   * @return True if the update was successful, false if the item could not be found in the Cache.
   */
  bool getFromCache(const Content& content, const Timestamp time, const bool local);
  /**
   * Checks whether a given item is in the Cache.
   * @param content The item we are fetching.
   * @return True if content is in the Cache, false otherwise.
   */
  bool isCached(const Content& content) const;
  /**
   * Erases an item from the Cache, freeing up some storage space.
   * @param content The item that needs to be deleted.
   * @param time The Timestamp of the deletion.
   */
  void removeFromCache(const Content& content, const Timestamp time); // for expired content
  /**
   * Notifies the Cache that the upload of a cached item has been completed, so
   * that the related metadata can be updated to reflect that.
   * @param content The item whose upload has just been completed.
   * @return True if the update is successful, false if content could not be found in the Cache.
   */
  bool uploadCompleted(const Content& content); // to decrease the upload counter
  /**
   * Retrieves the number of concurrent uploads of a specified item.
   * @param content The item we are interested in.
   * @return The number of concurrent uploads of the specified content.
   */
  int getCurrentUploads(const Content& content) const;
  /**
   * Retrieves the total number of concurrent uploads for all the elements in the Cache.
   * @return The total number of concurrent uploads for all the elements in the Cache.
   */
  int getTotalUploads() const;
  /**
   * Retrieve the average cache occupancy as calculated at the specified Timestamp.
   * @param time The Timestamp at which we want to know the average occupancy.
   * @return The average cache occupancy as calculated at the specified Timestamp.
   * @see RunningAvg
   */
  double getAvgOccupancy(const Timestamp time) const {
    double avg = cacheOccupancy.extract(time);
    return avg;
  }
  /**
   * Resets the time-weighted average occupancy of the Cache.
   * @param time The new "instant 0" for the time-weighted average.
   */
  void resetOccupancy(const Timestamp time) {
    double value = 100 * currentSize / maxSize;
    cacheOccupancy.reset(value, time);
  }
  
  /**
   * Get full access to the content of the Cache. This method was included to
   * allow external classes to implement more advanced functionalities, like
   * the storage space optimization of TopologyOracle::optimizeCaching().  
   * @return The full content of the Cache with the related metadata information.
   */
  CacheMap getCacheMap() const {
    return this->cacheMap;
  }
  
  /**
   * Retrieves the number of items cached at the moment.
   * @return the number of items currently stored in the Cache.
   */
  unsigned int getNumElementsCached() const {
    return this->cacheMap.size();
  }

  /**
   * Retrieve the current storage occupation of the Cache.
   * @return The storage space used by the items in the Cache.
   */
  Size getCurrentSize() const {
    return this->currentSize;
  }

  /**
   * Retrieve the maximum space available in the Cache, i.e., its capacity.
   * @return The maximum capacity of the Cache.
   */
  Size getMaxSize() const {
    return maxSize;
  }
  
  /**
   * Checks whether an item of Size size could currently fit in the Cache, 
   * without having to delete any of the elements already cached.
   * @param size The Size of the element we would like to add to the Cache.
   * @return True if the element would fit without having to delete anything, false otherwise.
   */
  bool fitsInCache(const Size size) const {
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
bool Cache<Content, Size, Timestamp>::getFromCache(const Content& content, 
        const Timestamp time, const bool local) {
  typename CacheMap::iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    it->second.lastAccessed = time;
    it->second.timesServed++;
    if (!local)
      it->second.uploads++;
    return true;
  } 
  else
    return false;
}

template <typename Content, typename Size, typename Timestamp>
bool Cache<Content, Size, Timestamp>::isCached(const Content& content) const {
  typename CacheMap::const_iterator cIt = cacheMap.find(content);
  if (cIt != cacheMap.end())
    return true;
  else 
    return false;
}

template <typename Content, typename Size, typename Timestamp>
void Cache<Content, Size, Timestamp>::removeFromCache(const Content& content, 
        const Timestamp time) {
  typename CacheMap::const_iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    this->currentSize -= it->second.size;
    assert(this->currentSize >= 0);
    cacheMap.erase(it);
    updateOccupancy(time);
  }
}

template <typename Content, typename Size, typename Timestamp>
bool Cache<Content, Size, Timestamp>::uploadCompleted(const Content& content) {
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
int Cache<Content, Size, Timestamp>::getCurrentUploads(const Content& content) const {
  typename CacheMap::const_iterator it = cacheMap.find(content);
  if (it != cacheMap.end()) {
    return it->second.uploads;
  } else {
    return -1;
  }
}

template <typename Content, typename Size, typename Timestamp>
int Cache<Content, Size, Timestamp>::getTotalUploads() const {
  int uploads = 0;
  for (typename CacheMap::const_iterator it = cacheMap.begin(); it != cacheMap.end(); it++)
    uploads += it->second.uploads;
  return uploads;
}

#endif	/* CACHE_HPP */

