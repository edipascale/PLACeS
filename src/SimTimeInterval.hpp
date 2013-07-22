/* 
 * File:   SimTimeInterval.hpp
 * Author: manuhalo
 *
 * Created on 29 agosto 2012, 12.43
 */

#ifndef SIMTIMEINTERVAL_HPP
#define	SIMTIMEINTERVAL_HPP

#include "PLACeS.hpp"

enum OverlapResult { NoOverlap, LeftOverlap, RightOverlap};

class SimTimeInterval {
protected:
    SimTime start;
    SimTime end;
    
public:
    SimTimeInterval(SimTime start, SimTime end);
    OverlapResult overlap(SimTimeInterval interval);
    
    SimTime getEnd() const {
        return end;
    }

    void setEnd(SimTime end) {
        this->end = end;
    }

    SimTime getStart() const {
        return start;
    }

    void setStart(SimTime start) {
        this->start = start;
    }

};

#endif	/* SIMTIMEINTERVAL_HPP */

