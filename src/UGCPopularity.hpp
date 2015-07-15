/* 
 * File:   UGCPopularity.hpp
 * Author: Emanuele Di Pascale
 * 
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

/**
 * Defines a Popularity Distribution which follows the model described in [1].
 * This is used to generate requests that match those of a User Generated Content
 * website (specifically YouTube). In short, each element is assigned a peek
 * popularity week; the number of views it receives each week only depends on
 * whether it is before, at or after this peek popularity week. 
 * 
 * [1] Y. Borghol, S. Mitra, S. Ardon, N. Carlsson, D. Eager, and A. Mahanti, 
 * “Characterizing and modelling popularity of user-generated videos,” 
 * Perform. Eval., vol. 68, no. 11, pp. 1037–1055, Nov. 2011.
 * 
 * @deprecated
 */
class UGCPopularity {
protected:
  unsigned short int totalRounds; /**< Total number of rounds in this simulation run. */
  boost::math::ttpDistribution* ttpDist; /**< Composite distribution used to model the popularity of objects, as described in Borghol's paper. */
  bool perturbations; /**< Boolean parameter indicating whether we are using the optional permutation of views to better fit the real traces, as described in Borghol's paper. */
  
public:
  /**
   * Simple constructor.
   * @param totalRounds The total number of rounds (i.e., weeks) of the current simulation run.
   * @param perturbations A boolean parameter indicating whether we are using the optional permutation of views to better fit the real traces.
   */
  UGCPopularity(unsigned int totalRounds, bool perturbations);
  ~UGCPopularity();
  /**
   * Assigns the number of view that each of the ContentElements in the list will
   * receive during the current round. All the elements in the list are assumed
   * to be in the same phase with respect to their peak week of popularity.
   * @param contentList The list of ContentElements for which we need to generate views.
   * @param phase An indicator of whether the elements in the list are before their popularity peak, at the peak, or after it.
   * @return The total number of views generated collectively for all the elements in the list.
   */
  unsigned int generateViews(std::list<ContentElement*> contentList, 
    peakingPhase phase);
  /**
   * Extracts a peak popularity round from the distribution defined in Borghol's paper.
   * @return The index of the simulation round at which a ContentElement will see its peak in popularity.
   */
  unsigned int generatePeakRound();
  /**
   * Generates the weekly views for an element which is in the body of the distribution and before the peak popularity.
   * @return The weekly number of views received by the element.
   */
  unsigned int getBeforePeakViews();
  /**
   * Generates the weekly views for an element which is in the tail of the distribution and before the peak popularity.
   * @return The weekly number of views received by the element.
   */
  unsigned int getBeforePeakTailViews();
  /**
   * Generates the weekly views for an element which is in the body of the distribution and at the peak popularity.
   * @return The weekly number of views received by the element.
   */
  unsigned int getAtPeakViews();
  /**
   * Generates the weekly views for an element which is in the tail of the distribution and at the peak popularity.
   * @return The weekly number of views received by the element.
   */
  unsigned int getAtPeakTailViews();
  /**
   * Generates the weekly views for an element which is in the body of the distribution and after the peak popularity.
   * @return The weekly number of views received by the element.
   */
  unsigned int getAfterPeakViews();
  /**
   * Generates the weekly views for an element which is in the tail of the distribution and after the peak popularity.
   * @return The weekly number of views received by the element.
   */
  unsigned int getAfterPeakTailViews();

  unsigned short int getTotalRounds() const {
    return totalRounds;
  }

  void setTotalRounds(unsigned short int rounds) {
    this->totalRounds = rounds;
  }

  
};

#endif	/* UGCPOPULARITY_HPP */

