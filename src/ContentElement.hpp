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

/**
 * A chunk is the unit of data that is transferred for a request. Each content
 * is divided into multiple chunks of the same size, with the exception of the
 * last chunk of aContentElement, which can be smaller. Chunks have a sequential
 * index identifying them within that ContentElement.
 */
class ContentChunk {
protected:
  Capacity size; /**< Size of this chunk in Mbps. Required because the last chunk of each ContentElement can be smaller than the default fixed size. */
  uint index; /**< Incremental index identifying this particular chunk within its ContentElement */
  // uint viewsThisRound; /**< keeps track of the popularity of this particular chunk. Currently not used as we track popularity by ContentElement only. */
  ContentElement* parent; /**< Pointer to the parent ContentElement of this chunk. */

public:
  ContentChunk(Capacity size, uint index, ContentElement* parent) {
    this->size = size;
    this->index = index;
    // viewsThisRound = 0;
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
 /* 
  uint getViewsThisRound() const {
    return this->viewsThisRound;
  }
  
  void increaseViewsThisRound() {
    this->viewsThisRound++;
  }
  
  void resetViewsThisRound() {
    this->viewsThisRound = 0;
  }
  */
  ContentElement* getContent() const {
    return parent;
  }
  
};

/**
 * ChunkPtr is a sahred_ptr to ContentChunk. It's what we store in caches,
 * which simplifies the memory management of this process. Note that passing
 * a shared_ptr by value is inefficient, for this reason we usually pass around
 * const references to ChunkPtr whenever we don't need to copy or modify it.
 */
typedef std::shared_ptr<ContentChunk> ChunkPtr;

/**
 * A ContentElement is a video from the multimedia catalog, e.g., a movie, TV
 * show etc. A user requests one of these contents, but the VoD service actually
 * breaks down the transfer into multiple "chunks", i.e., pieces of this video.
 */
class ContentElement {    
protected:
    std::string name; /**< Name of the video, typically an increasing integer identifier represented as a string. */
    Capacity size; /**< Total size in Mbps of this item; extracted from a normal distribution whose parameters can be defined through the command line options. */
    unsigned int totalChunks; /**< Number of total chunks in which this item is divded. */
    unsigned int viewsThisRound; /**< Number of requests this content will receive during the current simulation round. In VoD mode these are pre-calculated by the popularity model at the beginning of each round; in IPTV we are using this as a counter for the requests. */
    int releaseDay; /**< In IPTV mode, this is the round at the beginning of which this particular item was added to the catalog; in VoD mode, it represents the round in which the popularity of this item peaks, as defined by the model in Borghol et al. */
    std::vector<ChunkPtr> chunks; /**< A vector of shared pointers to the Chunks of which this item is composed. */
    
public:
  /**
   * Simple constructor.
   * @param name Name of the content. We usually use an increasing inter identifier but any string is acceptable.
   * @param releaseDay Round in which the content was released, for IPTV; round in which the popularity of this item will peak, in VoD.
   * @param size Size of the ContentElement in Mbps.
   * @param chunkSize Size of each Chunk into which this video will be split. Note that if chunkSize==0, the ContentElement will be composed of a single Chunk the same size of the content itself.
   */
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
  
  unsigned int getTotalChunks() const {
    return totalChunks;
  }
  
  std::vector<ChunkPtr> getChunks() const {
    return chunks;
  }
  
  /**
   * Retrieves a Chukn by its id.
   * @param index The id of the Chunk we are trying to retrieve.
   * @return A ChunkPtr to the requested Chunk if available; nullptr otherwise.
   */
  ChunkPtr getChunkById(uint index) {
    if (index >= 0 && index < chunks.size())
      return (chunks.at(index));
    else
      return nullptr;
  }
};

#endif	/* CONTENTELEMENT_HPP */

