/* 
 * File:   RankingTable.hpp
 * Author: Emanuele Di Pascale, CTVR - Trinity College Dublin.
 *
 * Created on 29 July 2014, 18:24
 */

#ifndef RANKINGTABLE_HPP
#define	RANKINGTABLE_HPP
#include "boost/bimap.hpp"
#include <vector>
using uint = unsigned int;

template <class Element> 
class RankingTable {
public:
    typedef typename boost::bimap<Element, uint> RankingBimap;
    typedef typename RankingBimap::right_const_iterator RightIter;
    typedef typename RankingBimap::right_map RightMap;
    typedef typename RankingBimap::left_map LeftMap;
protected:
    std::vector<uint> hits;
    RankingBimap catalog;
    bool swap(uint a, uint b);
public:
    RankingTable(){};
    void insert(Element e);
    void erase(Element e);
    void eraseByRank(uint rank);
    Element getElementByRank(uint rank) const;
    const RightMap getRightMap() const;
    const LeftMap getLeftMap() const;
    uint getRankOf(Element e) const;
    bool isInCatalog(Element e) const;
    void hit(Element e);
    uint size() const;
    void clear();
    uint getHits(Element e) const;
    uint getHitsByRank(uint rank) const;
    void printRanking() const {
        for (auto it = catalog.right.begin(); it != catalog.right.end(); it++)
        std::cout << "element " << it->second << " has rank " << it->first 
             << " with "<< getHitsByRank(it->first) << " hits" << std::endl;
    }
};

template <class Element>
void RankingTable<Element>::insert(Element e) {
    // check if the element is already in the catalog
    if (catalog.left.find(e) != catalog.left.end())
        return;
    // insert it at the end
    
    catalog.left.insert(std::make_pair(e,hits.size()));
    this->hits.push_back(0);
    assert(this->hits.size() == catalog.left.size());    
}

template <class Element>
void RankingTable<Element>::erase(Element e) {
  try {
    eraseByRank(getRankOf(e));
  } catch (int exception) {
    return;
  }
}

template <class Element>
void RankingTable<Element>::eraseByRank(uint rank) {
    auto init = catalog.right.find(rank);
    if (init != catalog.right.end()) {
        catalog.right.erase(init);
        hits.erase(hits.begin()+rank);
        assert(catalog.right.size() == hits.size());
        init = catalog.right.find(rank+1);
        for (auto it = init; it != catalog.right.end(); it++) {
            catalog.right.replace_key(it, rank);
            rank++;
        }
    }        
}

template <class Element>
Element RankingTable<Element>::getElementByRank(uint rank) const {
    auto rit = catalog.right.find(rank);
    if (rit != catalog.right.end())
        return rit->second;
    else
      throw -1;
}

template <class Element>
const typename RankingTable<Element>::RightMap RankingTable<Element>::getRightMap() const {
    return catalog.right;
}

template <class Element>
const typename RankingTable<Element>::LeftMap RankingTable<Element>::getLeftMap() const {
    return catalog.left;
}

template <class Element>
uint RankingTable<Element>::getRankOf(Element e) const {
    auto rit = catalog.left.find(e);
    if (rit != catalog.left.end())
        return rit->second;
    else
        throw -1;       
}

template <class Element>
bool RankingTable<Element>::isInCatalog(Element e) const {
    return (catalog.left.find(e) != catalog.left.end());
}

template <class Element>
uint RankingTable<Element>::size() const {
    return catalog.size();
}

template <class Element>
void RankingTable<Element>::clear() {
    catalog.clear();
    hits.clear();
}

template <class Element>
bool RankingTable<Element>::swap(uint a, uint b) {
    uint temp = hits.size();
    auto itA = catalog.right.find(a);
    auto itB = catalog.right.find(b);
    if (itA == catalog.right.end() || itB == catalog.right.end())
        return false;
    bool success = catalog.right.replace_key(itA, temp);
    if (!success) return false;
    success = catalog.right.replace_key(itB, a);
    if (!success) return false;
    success = catalog.right.replace_key(itA, b);
    if (!success) return false;
//    std::cout << "debug: hits before swap: ";
//    for (auto vit = hits.begin(); vit != hits.end(); vit++)
//        std::cout << *vit << " ";
//    std::cout << std::endl;
    std::swap(hits.at(a), hits.at(b));
//    std::cout << "debug: hits after swap: ";
//    for (auto vit = hits.begin(); vit != hits.end(); vit++)
//        std::cout << *vit << " ";
//    std::cout << std::endl;
    return true;
}

template <class Element>
void RankingTable<Element>::hit(Element e) {
    auto it = catalog.left.find(e);
    if (it == catalog.left.end())
        throw -1;
    uint oldRank = it->second;
    uint oldHits = hits.at(oldRank);
    hits.at(oldRank) = hits.at(oldRank) + 1;
    int index = 0;
    for (index = oldRank; index > 0 && hits.at(index-1) < hits.at(oldRank); index--);
    if (index != oldRank) {
//        std::cout << "debug: swapping " << oldRank << "," << index << std::endl;
        bool success = swap(oldRank, index);
//        printRanking();
        assert(success);
    }
}

template <class Element>
uint RankingTable<Element>::getHits(Element e) const {
    uint rank = getRankOf(e);
    return hits.at(rank);
}

template <class Element>
uint RankingTable<Element>::getHitsByRank(uint rank) const {
    if (rank >= 0 && rank < hits.size())
        return hits.at(rank);
    else
        throw -1;
}

#endif	/* RANKINGTABLE_HPP */

