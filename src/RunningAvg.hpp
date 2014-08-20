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

template <class Value, class Timestamp>
class RunningAvg {
  typedef typename std::pair<Value, Timestamp> DataPoint;
  protected:
    double avg;
    DataPoint lastEntry;
    Timestamp start;
  
  public:
    RunningAvg() {
      avg = 0;
      lastEntry = std::make_pair(0,0);
      start = 0;
    }
    
    RunningAvg(Value value, Timestamp time) {
      avg = value;
      lastEntry = std::make_pair(value, time);
      start = time;
    }
    
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
    
    /* Returns a double because it's the result of a division and integrity
     * cannot be preserved. The caller can round it in whatever way he requires
     * if need be. Returns DBL_MAX if currentTime is less than the last timestamp
     */
    double extract (Timestamp currentTime) {
      if (currentTime == start && lastEntry.second == start)
        return avg;
      else if (currentTime < lastEntry.second)
        return DBL_MAX;
      else
        return (avg * (lastEntry.second - start) + lastEntry.first * (currentTime - 
              lastEntry.second))/ (currentTime - start); 
    }
    
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
    
    void reset(Value value, Timestamp time) {
      avg = value;
      lastEntry = std::make_pair(value, time);;
      start = time;
    }
    
    Timestamp getLastTimestamp() const {
      return lastEntry.second;
    }
    
};

#endif	/* RUNNINGAVG_HPP */