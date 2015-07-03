#include "ContentElement.hpp"
#include "TopologyOracle.hpp"
#include <math.h>

/* When a new content is created, divide it into chunks of the specified size
 * (the last chunk is allowed to be smaller) and save shared pointers to those
 * chunks in a vector. The chunks are what is actually going to be transferred
 */
ContentElement::ContentElement(std::string name, int releaseDay, Capacity size,
        Capacity chunkSize) {
  this->name = name;
  this->releaseDay = releaseDay;
  this->size = size;
  this->viewsThisRound = 0;
  // if chunkSize==0 then we are not using chunking (i.e., one chunk per content)
  if (chunkSize == 0) {
    this->totalChunks = 1;
    ChunkPtr p = std::make_shared<ContentChunk>(size, 0, this);
    chunks.push_back(p);
    return;
  }
  this->totalChunks = std::ceil(size / chunkSize);
  chunks.reserve(totalChunks);
  for (uint i = 0; i < totalChunks; i++) {
    ChunkPtr p = std::make_shared<ContentChunk>(chunkSize, i, this);
    chunks.push_back(p);
  }
  Capacity residualSize = fmod(size, chunkSize);
  if (residualSize > 0 ) 
    chunks.at(totalChunks-1)->setSize(residualSize);
}

ContentElement::~ContentElement() {
//  for (auto i = chunks.begin(); i != chunks.end(); i++) {
//    i->reset();
//  }
  chunks.clear();
}