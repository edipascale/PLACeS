#include "IPTVPopularity.hpp"
#include <math.h>

IPTVPopularity::IPTVPopularity(uint N, double q, double s) {
  this->N = N;
  this->q = q;
  this->s = s;
  double Hnqs = 0;
  uint k;
  std::vector<double> pows;
  pows.resize(N,0);
  for (k = 1; k <= N; k++) {
    pows[k-1] = pow(k + q, s);
    Hnqs += 1 / pows[k-1];
  }
  this->rankCoeff.resize(N, 0);
  for (k = 1; k <= N; k++) {
    this->rankCoeff[k-1] = 1/(pows[k-1] * Hnqs);
  }
}

double IPTVPopularity::getRankCoeff(uint rank) {
  if (rank > 0 && rank <= this->N)
    return this->rankCoeff[rank-1];
  else
    return 0;
}

double IPTVPopularity::getDailyCoeff(uint day) {
  if (day >= 0 && day < 7) {
    assert (dailyCoeff[day] != 0);
    return dailyCoeff[day];
  }
  else
    return 0;
}
