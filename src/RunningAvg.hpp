/* 
 * File:   RunningAvg.hpp
 * Author: manuhalo
 *
 * Created on 24 settembre 2013, 10.32
 */

#ifndef RUNNINGAVG_HPP
#define	RUNNINGAVG_HPP
#include <utility>
#include <iostream>

template <class T1, class T2>
class RunningAvg {
  typedef typename std::pair<T1, T2> DataPoint;
  protected:
    double avg;
    DataPoint lastEntry;
  
  public:
    RunningAvg() {
      avg = 0;
      lastEntry = std::make_pair(0,0);
    }
    
    RunningAvg(DataPoint initial) {
      avg = initial.first;
      lastEntry = initial;
    }
    
    bool add(DataPoint newEntry) {
      if (newEntry.second < lastEntry.second) {
       // std::cerr << "RunningAvg: newEntry preceeds lastEntry (" 
       //         << newEntry.second << "<" << lastEntry.second << ")" 
       //         << std::endl;
        return false;
      } else if (newEntry.second == lastEntry.second) {
        lastEntry.first = newEntry.first;
        return true;
      } else {
        avg = (avg * lastEntry.second + lastEntry.first * (newEntry.second -
              lastEntry.second)) / newEntry.second;
        lastEntry = newEntry;
        return true;
      }
    }
    
    double extract (T2 currentTime) {
      return (avg * lastEntry.second + lastEntry.first * (currentTime - 
              lastEntry.second))/currentTime; 
    }
    
    bool increment(T1 increment, T2 currentTime) {
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
    
    void reset(DataPoint initial) {
      avg = initial.first;
      lastEntry = initial;
    }
    
    T2 getLastTimestamp() const {
      return lastEntry.second;
    }
};

#endif	/* RUNNINGAVG_HPP */