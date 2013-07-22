/* boost-supplement random/discrete_distribution.hpp header file
 *
 * Copyright (C) 2008 Kenta Murata.
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id: discrete_distribution.hpp 5906 2008-01-30 15:09:35Z mrkn $
 * 
 */

#ifndef BOOST_SUPPLEMENT_ZIPF_DISTRIBUTION_HPP
#define BOOST_SUPPLEMENT_ZIPF_DISTRIBUTION_HPP 1

#include <boost/random/discrete_distribution.hpp>

#include <cmath>

namespace boost { namespace random {

// Zipf-Mandelbrot distribution
//
// Let N, q, and s be num, shift, and exp, respectively.  The
// probability distribution is P(k) = (k + q)^{-s} / H_{N,q,s} where k
// = 1, 2, ..., N, and H_{N,q,s} is generalized harmonic number, that
// is H_{N,q,s} = \sum_{i=1}^N (i+q)^{-s}.
//
// http://en.wikipedia.org/wiki/Zipf-Mandelbrot_law.
template<class IntType = long, class RealType = double>
class zipf_distribution
{
public:
  typedef RealType input_type;
  typedef IntType result_type;

#if !defined(BOOST_NO_LIMITS_COMPILE_TIME_CONSTANTS) && !(defined(BOOST_MSVC) && BOOST_MSVC <= 1300)
  BOOST_STATIC_ASSERT(std::numeric_limits<IntType>::is_integer);
  BOOST_STATIC_ASSERT(!std::numeric_limits<RealType>::is_integer);
#endif

private:
  result_type num_;
  input_type shift_;
  input_type exp_;

  typedef discrete_distribution<IntType, RealType> dist_type;
  dist_type dist_;

  dist_type make_dist(result_type num, input_type shift, input_type exp)
    {
      std::vector<input_type> buffer(num);
      for (result_type k = 1; k <= num; ++k)
        buffer[k-1] = std::pow(k + shift, -exp);
      return dist_type(buffer.begin(), buffer.end());
    }

public:
  zipf_distribution(result_type num, input_type shift, input_type exp)
    : num_(num), shift_(shift), exp_(exp),
      dist_(make_dist(num, shift, exp))
    {}

  result_type num() const { return num_; }

  input_type shift() const { return shift_; }

  input_type exponent() const { return exp_; }

  template<class Engine>
  result_type operator()(Engine& eng) { return dist_(eng); }
};

} }

#endif // BOOST_SUPPLEMENT_ZIPF_DISTRIBUTION_HPP
