/* 
 * File:   ContentElement.hpp
 * Author: emanuele
 *
 * Created on 27 April 2012, 21:22
 */

#ifndef CONTENTELEMENT_HPP
#define	CONTENTELEMENT_HPP

#include "PLACeS.hpp"
#include <string>
#include "boost/multi_index_container.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/hashed_index.hpp"
#include "boost/multi_index/mem_fun.hpp"

/* forward declaration
 */
class ContentElement;

/* A chunk is the unit of data that is transferred for a request. Each content
 * is divided into multiple chunks of the same size. Chunks have a sequential
 * index; each one has its own popularity based on the number of hits it received.
 */
class ContentChunk {
protected:
  Capacity size;
  uint index;
  uint viewsThisRound;
  ContentElement* parent;

public:
  ContentChunk(Capacity size, uint index, ContentElement* parent) {
    this->size = size;
    this->index = index;
    viewsThisRound = 0;
    this->parent = parent;
  }
  
  Capacity getSize() const {
    return this->size;
  }
  
  void setSize(Capacity size) {
    this->size = size;
  }
  
  uint getIndex() const {
    return this->index;
  }
  
  void setIndex(uint index) {
    this->index = index;
  }
  
  uint getViewsThisRound() const {
    return this->viewsThisRound;
  }
  
  void increaseViewsThisRound() {
    this->viewsThisRound++;
  }
  
  void resetViewsThisRound() {
    this->viewsThisRound = 0;
  }
  
  ContentElement* getContent() const {
    return parent;
  }
  
};

typedef std::shared_ptr<ContentChunk> ChunkPtr;

/* A ContentElement is a video from the multimedia catalog, e.g., a movie, TV
 * show etc. A user selects one of these contents, but the VoD service actually
 * requests "chunks", i.e., pieces of this video.
 */
class ContentElement {    
protected:
    std::string name;
    Capacity size;
    Capacity chunkSize;
    unsigned int viewsThisRound; // in VoD these are the pre-calculated views for this round; in IPTV we are using this as a counter for the view requests
    int releaseDay; // after the merge with VoD, also works as peakingRound
    std::vector<ChunkPtr> chunks;    
    
public:
  ContentElement(std::string name, int releaseDay, Capacity size, 
          Capacity chunkSize);
  
  ~ContentElement();
  
  std::string getName() const {
    return name;
  }

  unsigned int getViewsThisRound() const {
    return viewsThisRound;
  }

  void increaseViewsThisRound(unsigned int increase = 1) {
    this->viewsThisRound = viewsThisRound + increase;
  }
  
  void setViewsThisRound(unsigned int views) {
    this->viewsThisRound = views;
  }
  
  void resetViewsThisRound() {
    this->viewsThisRound = 0;
  }

  void setName(std::string name) {
    this->name = name;
  }

  int getReleaseDay() const {
    return releaseDay;
  }
  
  // releaseDay works as the peakingRound for VoD
  int getPeakingRound() const {
    this->getReleaseDay();
  }

  void setReleaseDay(int releaseDay) {
    this->releaseDay = releaseDay;
  }

  // releaseDay works as the peakingRound for VoD
  void setPeakingRound(int peakingRound) {
    this->setReleaseDay(peakingRound);
  }
  
  Capacity getSize() const {
    return size;
  }

  void setSize(Capacity size) {
    this->size = size;
  }
  
  Capacity getChunkSize() const {
    return chunkSize;
  }
  
  std::vector<ChunkPtr> getChunks() const {
    return chunks;
  }
  
  ChunkPtr getChunkById(uint index) {
    if (index >= 0 && index < chunks.size())
      return (chunks.at(index));
    else
      return nullptr;
  }
};

#endif	/* CONTENTELEMENT_HPP */

