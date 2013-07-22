/* 
 * File:   Popularity.hpp
 * Author: emanuele
 *
 * Created on 27 April 2012, 15:24
 */

#ifndef POPULARITY_HPP
#define	POPULARITY_HPP

// Abstract class to represent all sorts of possible popularity distributions
class Popularity {
protected:
    uint numElements;
    std::string name;
        
public:
    virtual uint getSample() = 0;
    virtual bool evolve() = 0;

    std::string getName() const {
        return name;
    }

    void setName(std::string name) {
        this->name = name;
    }

    uint getNumElements() const {
        return numElements;
    }

    void setNumElements(uint numElements) {
        this->numElements = numElements;
    }

                
        
};


#endif	/* POPULARITY_HPP */

