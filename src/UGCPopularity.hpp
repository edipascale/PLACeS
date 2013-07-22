/* 
 * File:   UGCPopularity.hpp
 * Author: emanuele
 * 
 * Defines a Popularity Distribution which follows the model of Borghol et al.,
 * "Characterizing and Modeling Popularity of User-Generated Videos", 2011.
 * Created on 30 April 2012, 15:20
 */

#ifndef UGCPOPULARITY_HPP
#define	UGCPOPULARITY_HPP
#include "boost/math/distributions.hpp"
#include "ContentElement.hpp"
#include <list>

enum peakingPhase {
  BEFORE_PEAK, AT_PEAK, AFTER_PEAK
};

namespace boost {
  namespace math {

    template <class RealType = double, class Policy = policies::policy<> >
    class time_to_peak_distribution {
    public:
      typedef RealType value_type;
      typedef Policy policy_type;

      time_to_peak_distribution(unsigned int totalRounds = 10,
              unsigned int peakSingularity = 6, RealType lambda = 0.268)
      {
        this->totalRounds = totalRounds;
        this->peakSingularity = peakSingularity;
        this->lambda = lambda;        
      } 

      unsigned int getTotalRounds() const {
        return totalRounds;
      }
      
      unsigned int getPeakSingularity() const {
        return peakSingularity;
      }
      
      RealType getLambda() const{
        return lambda;
      }

    private:
      unsigned int totalRounds;
      unsigned int peakSingularity;
      RealType lambda;
    };

  typedef time_to_peak_distribution<double> ttpDistribution;
  
  template <class RealType, class Policy>
    inline RealType cdf(const time_to_peak_distribution<RealType, Policy>& dist, 
          const unsigned int& x) {
      BOOST_MATH_STD_USING // for ADL of std functions

              static const char* function = "boost::math::cdf(const time_to_peak_distribution<%1%>&, %1%)";

      RealType result = cdf(exponential(dist.getLambda()), x);      
      if (x > dist.getPeakSingularity())
        result += std::exp(dist.getPeakSingularity() * dist.getLambda()) * 
                (x - dist.getPeakSingularity()) / 
                (dist.getTotalRounds() - dist.getPeakSingularity());

      return result;
    } // cdf
  template <class RealType, class Policy>
    inline RealType quantile(const time_to_peak_distribution<RealType, Policy>& dist,
          const RealType& p) {
      BOOST_MATH_STD_USING // for ADL of std functions

              static const char* function = "boost::math::quantile(const time_to_peak_distribution<%1%>&, %1%)";

      RealType result;
      RealType lambda = dist.getLambda();
      // peakCdf is the CDF value at which the exponential turns into a uniform dist.
      RealType peakCdf = cdf(exponential(dist.getLambda()), (RealType) dist.getPeakSingularity());
      if (0 == detail::verify_lambda(function, lambda, &result, Policy()))
        return result;
      if (0 == detail::check_probability(function, p, &result, Policy()))
        return result;

      if (p == 0)
        return 0;
      if (p == 1)
        return policies::raise_overflow_error<RealType > (function, 0, Policy());
      
      if(p < peakCdf)
        result = -boost::math::log1p(-p, Policy()) / lambda;
      else
        result = dist.getPeakSingularity() + 1/(1-peakCdf)*(p-peakCdf)*
                (dist.getTotalRounds() - dist.getPeakSingularity());
      return result;
    } // quantile

  }
}

class UGCPopularity {
protected:
  unsigned short int totalRounds;
  boost::math::ttpDistribution* ttpDist;
  bool perturbations;
  
public:
  UGCPopularity(unsigned int totalRounds, bool perturbations);
  ~UGCPopularity();
  unsigned int generateViews(std::list<ContentElement*> contentList, 
    peakingPhase phase);
  unsigned int generatePeakRound();
  unsigned int getBeforePeakViews();
  unsigned int getBeforePeakTailViews();
  unsigned int getAtPeakViews();
  unsigned int getAtPeakTailViews();
  unsigned int getAfterPeakViews();
  unsigned int getAfterPeakTailViews();

  unsigned short int getTotalRounds() const {
    return totalRounds;
  }

  void setTotalRounds(unsigned short int rounds) {
    this->totalRounds = rounds;
  }

  
};

#endif	/* UGCPOPULARITY_HPP */

