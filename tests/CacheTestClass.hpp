/*
 * File:   CacheTestClass.hpp
 * Author: dipascae
 *
 * Created on 30-Apr-2013, 14:18:21
 */

#ifndef CACHETESTCLASS_HPP
#define	CACHETESTCLASS_HPP

#include <cppunit/extensions/HelperMacros.h>

class CacheTestClass : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(CacheTestClass);

  CPPUNIT_TEST(testAddToCache);

  CPPUNIT_TEST_SUITE_END();

public:
  CacheTestClass();
  virtual ~CacheTestClass();
  void setUp();
  void tearDown();

private:
  void testAddToCache();

};

#endif	/* CACHETESTCLASS_HPP */

