/*
 * File:   CacheTestClass.cpp
 * Author: dipascae
 *
 * Created on 30-Apr-2013, 14:18:22
 */

#include "CacheTestClass.hpp"
#include "Cache.hpp"


CPPUNIT_TEST_SUITE_REGISTRATION(CacheTestClass);

CacheTestClass::CacheTestClass() {
}

CacheTestClass::~CacheTestClass() {
}

void CacheTestClass::setUp() {
}

void CacheTestClass::tearDown() {
}

void CacheTestClass::testAddToCache() {
  
  for (int policy = LRU; policy <= LFU; policy++) {
    Cache<int, int, int> cache(5, static_cast<CachePolicy>(policy));
    // test constructor
    CPPUNIT_ASSERT(cache.getCurrentSize() == 0);
    CPPUNIT_ASSERT(cache.getMaxSize() == 5);
    // fill the cache and test addToCache
    std::pair<bool, std::set<int> > result;
    for (int i = 0; i < 5; i++) {
      result = cache.addToCache(i, 1, i);
      CPPUNIT_ASSERT(result.first == true);
      CPPUNIT_ASSERT(result.second.empty());
      CPPUNIT_ASSERT(cache.getCurrentSize() == i + 1);
      CPPUNIT_ASSERT(cache.isCached(i, 1));
    }
    // test invariance of maxSize
    CPPUNIT_ASSERT(cache.getMaxSize() == 5);
    // test failure of searching for a non-cached element
    CPPUNIT_ASSERT(!cache.isCached(10, 1));
    // test failure of searching for a cached element with a bigger size
    CPPUNIT_ASSERT(!cache.isCached(0, 5));
    // test success of searching for a cached element with a smaller size
    CPPUNIT_ASSERT(cache.isCached(1, 0));
    // test addToCache when the cache is full (content 0 will be evicted)
    result = cache.addToCache(5, 1, 5);
    CPPUNIT_ASSERT(result.first);
    CPPUNIT_ASSERT(!result.second.empty());
    CPPUNIT_ASSERT(cache.isCached(5, 1));
    CPPUNIT_ASSERT(!cache.isCached(0, 1));
    CPPUNIT_ASSERT(cache.getCurrentSize() == cache.getMaxSize());
    CPPUNIT_ASSERT(cache.getMaxSize() == 5);
    // test cache removal
    cache.removeFromCache(4);
    CPPUNIT_ASSERT(cache.getMaxSize() == 5);
    CPPUNIT_ASSERT(cache.getCurrentSize() == cache.getMaxSize() - 1);
    CPPUNIT_ASSERT(!cache.isCached(4, 1));
    // test cache fetching (and related fields update)
    CPPUNIT_ASSERT(cache.getFromCache(1, 6));
    CPPUNIT_ASSERT(cache.cacheMap.at(1).lastAccessed == 6);
    CPPUNIT_ASSERT(cache.cacheMap.at(1).timesServed == 1);
    // test failed cache fetching
    CPPUNIT_ASSERT(!cache.getFromCache(4, 7));
    // test adding element which is already cached with same size
    result = cache.addToCache(2,1,8);
    CPPUNIT_ASSERT(result.first == false);
    CPPUNIT_ASSERT(result.second.empty());
    // test adding element which is already cached with bigger size
    result = cache.addToCache(2,2,9);
    CPPUNIT_ASSERT(result.first);
    CPPUNIT_ASSERT(result.second.empty()); // just enough space
    CPPUNIT_ASSERT(cache.cacheMap.at(2).lastAccessed == 9); //updated
    CPPUNIT_ASSERT(cache.cacheMap.at(2).size == 2);
    // test cache clearing
    cache.clearCache();
    CPPUNIT_ASSERT(cache.getCurrentSize() == 0);
    CPPUNIT_ASSERT(cache.getMaxSize() == 5);
    CPPUNIT_ASSERT(cache.isCached(2, 1) == 0);
  }
  
}

