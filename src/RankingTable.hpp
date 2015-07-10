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

/**
 * A class to rank elements dynamically based on the number of "hits" they receive.
 * 
 * Elements in the RankingTable are sorted by decreasing number of hits, i.e.,
 * with items with lower rank having a larger number of hits. The class can be
 * useful for example to track the popularity of items in a catalog, where each
 * hit represents a request. Most operations can be performed either using the Element
 * itself or its rank as the key, thanks to the bi-directional map that is at
 * the base of the class.
 */
template <class Element> 
class RankingTable {
public:
    typedef typename boost::bimap<Element, uint> RankingBimap;
    typedef typename RankingBimap::right_const_iterator RightIter;
    typedef typename RankingBimap::right_map RightMap;
    typedef typename RankingBimap::left_map LeftMap;
protected:
    std::vector<uint> hits; /**< The metric used to determine the rank of an item; the more hits an item receives, the lower its rank. */
    std::vector<uint> roundHits; /**< The number of hits received by each item in the current round. */
    RankingBimap catalog; /**< The bi-directional map that represents the actual ranking table. */
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
    uint getRoundHits(Element e) const;
    uint getHitsByRank(uint rank) const;
    uint getRoundHitsByRank(uint rank) const;
    void resetRoundHits();
    void printRanking() const {
        for (auto it = catalog.right.begin(); it != catalog.right.end(); it++)
        std::cout << "element " << it->second << " has rank " << it->first 
             << " with "<< getHitsByRank(it->first) << " hits" << std::endl;
    }
};


/**
 * Attempts to insert a new Element at the bottom the RankingTable. Will not do
 * anything if the Element is already in the RankingTable.
 * @param e The Element to be inserted.
 */
template <class Element>
void RankingTable<Element>::insert(Element e) {
    // check if the element is already in the catalog
    if (catalog.left.find(e) != catalog.left.end())
        return;
    // insert it at the end
    
    catalog.left.insert(std::make_pair(e,hits.size()));
    this->hits.push_back(0);
    this->roundHits.push_back(0);
    assert(hits.size() == catalog.left.size());
    assert(hits.size() == roundHits.size());    
}

/**
 * Attempts to erase an Element from the RankingTable, if it is there. Does not 
 * do anything if the element cannot be found.
 * @param e The Element to be erased.
 */
template <class Element>
void RankingTable<Element>::erase(Element e) {
  try {
    eraseByRank(getRankOf(e));
  } catch (int exception) {
    return;
  }
}

/**
 * Attempts to erase the Element with a given rank. Does not do anything if there
 * is no Element with that rank.
 * @param rank The rank of the Element we are trying to erase.
 */
template <class Element>
void RankingTable<Element>::eraseByRank(uint rank) {
    auto init = catalog.right.find(rank);
    if (init != catalog.right.end()) {
        catalog.right.erase(init);
        hits.erase(hits.begin()+rank);
        roundHits.erase(roundHits.begin()+rank);
        assert(catalog.right.size() == hits.size());
        assert(hits.size() == roundHits.size());    
        init = catalog.right.find(rank+1);
        for (auto it = init; it != catalog.right.end(); it++) {
            catalog.right.replace_key(it, rank);
            rank++;
        }
    }        
}

/**
 * Fetches the Element that has the specified rank, if such element exists.
 * @param rank The rank of the Element we are fetching.
 * @return The Element that has the rank specified.
 * @throw An int exception (-1) if no Element with the given rank could be found.
 */
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

/**
 * Returns the rank of a given Element.
 * @param e The Element we want to know the rank of.
 * @return The rank of the given Element.
 * @throw An int exception (-1) if the specified Element could not be found.
 */
template <class Element>
uint RankingTable<Element>::getRankOf(Element e) const {
    auto rit = catalog.left.find(e);
    if (rit != catalog.left.end())
        return rit->second;
    else
        throw -1;       
}

/**
 * Checks whether the specified Element is in the RankingTable.
 * @param e The Element we are fetching.
 * @return True if the Element e is in the RankingTable, False otherwise.
 */
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
    roundHits.clear();
}

/**
 *  A utility method to swap the rank of two items, while at the same time upgrading the rank of all the intermediate ones.
 * @param a The rank of the first item.
 * @param b The rank of the second item.
 * @return True if the swap was successful, False otherwise.
 */
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
    std::swap(roundHits.at(a), roundHits.at(b));
//    std::cout << "debug: hits after swap: ";
//    for (auto vit = hits.begin(); vit != hits.end(); vit++)
//        std::cout << *vit << " ";
//    std::cout << std::endl;
    return true;
}

/**
 * Increases the number of hits received by the specified Element by 1, and 
 * updates the ranking if required.
 * @param e The Element that just received an additional hit.
 */
template <class Element>
void RankingTable<Element>::hit(Element e) {
    auto it = catalog.left.find(e);
    if (it == catalog.left.end())
        throw -1;
    uint oldRank = it->second;
    uint oldHits = hits.at(oldRank);
    hits.at(oldRank) = hits.at(oldRank) + 1;
    roundHits.at(oldRank) = roundHits.at(oldRank) + 1;
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
uint RankingTable<Element>::getRoundHits(Element e) const {
    uint rank = getRankOf(e);
    return roundHits.at(rank);
}

template <class Element>
uint RankingTable<Element>::getHitsByRank(uint rank) const {
    if (rank >= 0 && rank < hits.size())
        return hits.at(rank);
    else
        throw -1;
}

template <class Element>
uint RankingTable<Element>::getRoundHitsByRank(uint rank) const {
    if (rank >= 0 && rank < hits.size())
        return roundHits.at(rank);
    else
        throw -1;
}

template <class Element>
void RankingTable<Element>::resetRoundHits() {
  roundHits.assign(roundHits.size(), 0);
}

#endif	/* RANKINGTABLE_HPP */

