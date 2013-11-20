/*
 * File:   CacheTest.hpp
 * Author: dipascae
 *
 * Created on 08-Nov-2013, 17:29:13
 */

#ifndef CACHETEST_HPP
#define	CACHETEST_HPP

#include <cppunit/extensions/HelperMacros.h>

class CacheTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(CacheTest);

  CPPUNIT_TEST(testAddToCache);

  CPPUNIT_TEST_SUITE_END();

public:
  CacheTest();
  virtual ~CacheTest();
  void setUp();
  void tearDown();

private:
  void testAddToCache();

};

#endif	/* CACHETEST_HPP */

