#include "SimTimeInterval.hpp"

SimTimeInterval::SimTimeInterval(SimTime start, SimTime end) {
  this->start = start;
  this->end = end;
  assert(start < end);
}

OverlapResult SimTimeInterval::overlap(SimTimeInterval interval) {
  SimTime midPoint1 = std::floor((this->start + this->end)/2);
  SimTime midPoint2 = std::floor((interval.start + interval.end)/2);
  if (this->start > interval.end || this->end < interval.start)
    return NoOverlap;
  else if (midPoint1 < midPoint2)
    return LeftOverlap;
  else
    return RightOverlap;
}
