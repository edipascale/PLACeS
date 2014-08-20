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
  
  public:
    RunningAvg() {
      avg = 0;
      lastEntry = std::make_pair(0,0);
    }
    
    RunningAvg(Value value, Timestamp time) {
      avg = value;
      lastEntry = std::make_pair(value, time);
    }
    
    bool add(Value newValue, Timestamp time) {
      if (time < lastEntry.second) {
        return false;
      } else if (time == lastEntry.second) {
        lastEntry.first = newValue;
        return true;
      } else {
        avg = (avg * lastEntry.second + lastEntry.first * (time -
              lastEntry.second)) / time;
        lastEntry = std::make_pair(newValue, time);
        return true;
      }
    }
    
    /* Returns a double because it's the result of a division and integrity
     * cannot be preserved. The caller can round it in whatever way he requires
     * if need be. Returns DBL_MAX if currentTime is less than the last timestamp
     */
    double extract (Timestamp currentTime) {
      if (currentTime == 0 && lastEntry.second == 0)
        return avg;
      else if (currentTime < lastEntry.second)
        return DBL_MAX;
      else
        return (avg * lastEntry.second + lastEntry.first * (currentTime - 
              lastEntry.second))/currentTime; 
    }
    
    bool increment(Value increment, Timestamp currentTime) {
      if (currentTime < lastEntry.second) {
        std::cerr << "RunningAvg: newEntry preceeds lastEntry (" 
                << currentTime << "<" << lastEntry.second << ")" 
                << std::endl;
        return false;
      } else if (currentTime == lastEntry.second) {
        lastEntry.first += increment;
        return true;
      } else {
        avg = (avg * lastEntry.second + lastEntry.first * (currentTime -
              lastEntry.second)) / currentTime;
        lastEntry.first += increment;
        lastEntry.second = currentTime;
        return true;
      }
    }
    
    void reset(Value value, Timestamp time) {
      avg = value;
      lastEntry = std::make_pair(value, time);;
    }
    
    Timestamp getLastTimestamp() const {
      return lastEntry.second;
    }
};

#endif	/* RUNNINGAVG_HPP */