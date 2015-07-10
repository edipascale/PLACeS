/* 
 * File:   RunningAvg.hpp
 * Author: Emanuele Di Pascale
 *
 * Created on 24 settembre 2013, 10.32
 */

#ifndef RUNNINGAVG_HPP
#define	RUNNINGAVG_HPP
#include <utility>
#include <iostream>
/**
 * Keeps a time-weighted average of a certain metric. Every time a new measure
 * is taken, the class updates its running average by keeping track of the
 * value and time of the previous measurement. So in example if at time 0 the
 * measured value was 0, at time 2 it was 5, and at time 6 it was 10, the average
 * at time 6 would be (0*2+5*4)/6 = 20/6, and at time 10 it would be
 * (20/6*6+10*4)/10=60/10=6.
 */
template <class Value, class Timestamp>
class RunningAvg {
    /**
     * A DataPoint represents a single measure of the metric wee are trying to
     * average; it is composed by the pair <value,  timestamp>.
     */
  typedef typename std::pair<Value, Timestamp> DataPoint; 
  protected:  
    double avg; /**< The average calculated at the last measurement. */
    DataPoint lastEntry; /**< The last measurement taken, including its timestamp. */
    Timestamp start; /**< The time starting from which the average should be calculated. */
  
  public:
    /**
     * The standard constructor assumes that the initial value is 0 and that the
     * moment from which we should compute the average is 0 too.
     */
    RunningAvg() {
      avg = 0;
      lastEntry = std::make_pair(0,0);
      start = 0;
    }
    /**
     * This alternate constructor allows us to define the initial value and 
     * starting time of the running average.
     * @param value The value of our metric of interest at the beginning.
     * @param time The initial timestamp from which we should start computing the average.
     */
    RunningAvg(Value value, Timestamp time) {
      avg = value;
      lastEntry = std::make_pair(value, time);
      start = time;
    }
    
    /**
     * Adds a new DataPoint to the measurement set and updates the average. This
     * new DataPoint will replace the previous lastEntry. Note that multiple 
     * entries for the same timestamp will simply overwrite each other, with 
     * only the last one having an effect on the computed average. Furthermore,
     * it is not possible to insert measurements whose timestamp is antecedent
     * to the last measurement (as recorded in lastEntry). In other words, the
     * measurements should be added in ascending chronological order.
     * @param newValue The value of the new measurement.
     * @param time The timestamp of the new measurement.
     * @return True if the new measurement was successfully recorded; False otherwise.
     */
    bool add(Value newValue, Timestamp time) {
      if (time < lastEntry.second) {
        return false;
      } else if (time == lastEntry.second) {
        lastEntry.first = newValue;
        return true;
      } else {
        avg = (avg * (lastEntry.second - start) + lastEntry.first * (time -
              lastEntry.second)) / (time-start);
        lastEntry = std::make_pair(newValue, time);
        return true;
      }
    }
    
    /**
     * Extracts the running average of our metric of interest at the specified 
     * currentTime. If this value is smaller than the timestamp relative to the
     * last recorded entry, there is no way to reconstruct the correct average,
     * and the constant value DBL_MAX is returned instead. 
     * Note that this method returns a double regardless of the type of Value,
     * because it's the result of a division and integrity cannot be preserved. 
     * It is the caller's responsibility to round it or convert it in whatever 
     * way required.
     * @param currentTime The timestamp at which we want to calculate the running average. Must be greater than lastEntry.second.
     * @return A double representing the running average of our metrics of interest, if possible; DBL_MAX otherwise.
     */
    double extract (Timestamp currentTime) const {
      if (currentTime == start && lastEntry.second == start)
        return avg;
      else if (currentTime < lastEntry.second)
        return DBL_MAX;
      else
        return (avg * (lastEntry.second - start) + lastEntry.first * (currentTime - 
              lastEntry.second))/ (currentTime - start); 
    }
    
    /**
     * Works similarly to RunningAvg::add(), but allows the user to specify an
     * increment with respect to the last measurement, rather than a new value.
     * @see RunningAvg::add()
     * @param increment The increment of the measured value with respect to the last recorded measurement.
     * @param currentTime The timestamp at which the increment was recorded.
     * @return True if the increment was recorded successfully, False otherwise.
     */
    bool increment(Value increment, Timestamp currentTime) {
      if (currentTime < lastEntry.second) {
        return false;
      } else if (currentTime == lastEntry.second) {
        lastEntry.first += increment;
        return true;
      } else {
        avg = (avg * (lastEntry.second - start) + lastEntry.first * (currentTime -
              lastEntry.second)) / (currentTime - start);
        lastEntry.first += increment;
        lastEntry.second = currentTime;
        return true;
      }
    }
    
    /**
     * Resets the RunningAvg to a new starting timestamp and starting value, like
     * the constructor with the same parameters.
     * @param value The new initial value for the running average.
     * @param time The new initial timestamp for the running average.
     */
    void reset(Value value, Timestamp time) {
      avg = value;
      lastEntry = std::make_pair(value, time);;
      start = time;
    }
    
    /**
     * Retrieves the timestamp of the last recorded measurement.
     * @return The timestamp of the last recorded measurement.
     */
    Timestamp getLastTimestamp() const {
      return lastEntry.second;
    }
    
};

#endif	/* RUNNINGAVG_HPP */