/* 
 * File:   SimTimeInterval.hpp
 * Author: manuhalo
 *
 * Created on 29 agosto 2012, 12.43
 */

#ifndef SIMTIMEINTERVAL_HPP
#define	SIMTIMEINTERVAL_HPP

#include "PLACeS.hpp"

/**
 * Values returned by the SimInterval::overlap() method.
 * @see SimInterval::overlap()
 */
enum OverlapResult { NoOverlap, LeftOverlap, RightOverlap};

/**
 * Simple Class representing a time interval, as defined by a start and an end time.
 * The assumption is that start should be strictly less than end. An overlap
 * method is defined to check whether this interval overlaps with another.
 */
class SimTimeInterval {
protected:
    SimTime start; /**< The starting time of the interval. */
    SimTime end; /**< The ending time of the interval. */
    
public:
  /**
   * Simple constructor.
   * @param start The starting time of the interval.
   * @param end The ending time of the interval.
   * @pre{start<end}
   */
    SimTimeInterval(SimTime start, SimTime end);
    /**
     * Checks whether this interval overlaps with another.
     * @param interval The second interval that we want to check for a possible overlap with.
     * @return{ - NoOVerlap if the two intervals are separated;
     *          - LeftOverlap if there is an overlap and the middle point of this interval is less than the midpoint of the second one;
     *          - RightOverlap if there is an overlap and the middle point of this interval is greater than the midpoint of the second one.}
     */
    OverlapResult overlap(SimTimeInterval interval);
    
    SimTime getEnd() const {
      return end;
    }
    
    /**
     * Set the end time for this interval.
     * @param end the new end time for this interval.
     * @pre{start<end}
     */
    void setEnd(SimTime end) {
      assert(end > start);
      this->end = end;
    }

    SimTime getStart() const {
      return start;
    }
    
    /**
     * Set the starting time for this interval.
     * @param start The new starting time for this interval.
     * @pre{start<end}
     */
    void setStart(SimTime start) {
      assert(end > start);
      this->start = start;
    }

};

#endif	/* SIMTIMEINTERVAL_HPP */

