
#include "IPTVContentHelper.hpp"

IPTVContentHelper::IPTVContentHelper(uint contentNum) {
  this->contentNum = contentNum;
  // allocate memory for the dailyCatalog
  for (uint i = 0; i < 7; i++) {
    std::vector<ContentElement*> vec(contentNum, NULL);
    dailyCatalog.push_back(vec);
  }
  this->relDayDist = new boost::random::zipf_distribution<>(7, 0, 1);
  this->rankDist = new boost::random::zipf_distribution<>(contentNum, 10, 0.6);
}

std::vector<ContentElement*> IPTVContentHelper::populateCatalog() {
  // vector of ContentElements* to return
  std::vector<ContentElement*> catalog;
  // generate content for the first day and the previous 6
  for (int day = -6; day < 1; day++) {
    for (uint i = 0; i < contentNum; i++) {
      ContentElement* content = new ContentElement(
              boost::lexical_cast<string > ((day + 6) * contentNum + i), day, i);
      dailyCatalog[std::abs(day)].at(i) = content;
      catalog.push_back(content);
    }
  }
  return catalog;
}

void IPTVContentHelper::generateRoundRequests(Scheduler* scheduler) {
  
}

