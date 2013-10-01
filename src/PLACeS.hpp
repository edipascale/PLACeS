/* 
 * File:   PLACeS.hpp
 * Author: Emanuele Di Pascale
 *
 * Created on 03 May 2012, 14:29
 */

#ifndef PLACES_HPP
#define	PLACES_HPP
#include <limits>
#include "boost/foreach.hpp"
#include "boost/program_options.hpp"
#include <boost/log/trivial.hpp>

namespace po = boost::program_options;

typedef int SimTime;
// typedeffed so that if I change it later on I won't have to change it everywhere
typedef double Capacity ;  // in Mbps

typedef std::pair<unsigned int, unsigned int> PonUser;

/* Simulate either a VoD system or a time-shifted IPTV system: has consequences
 * on the popularity model used, on the length of simulation rounds, on how
 * users requests are generated etc.
 */ 
enum SimMode {VoD, IPTV};

const SimTime INF_TIME = std::numeric_limits<int>::max();
const PonUser UNKNOWN = std::make_pair(std::numeric_limits<uint>::max(),
        std::numeric_limits<uint>::max());
const Capacity MAX_FLOW_SPEED = 1000; // 1Gbps
// const Capacity MIN_FLOW_SPEED = 3; // Minimal bitrate for mid-tier streaming
const Capacity UNLIMITED = std::numeric_limits<double>::max();

// Error exit codes
#define ERR_FAILED_ROUTING 1
#define ERR_VANILLA_EVENT 2
#define ERR_NO_EVENT_HANDLE 3
#define ERR_EVENT_IN_THE_PAST 4
#define ERR_HANDLEMAP_INSERT 5
#define ERR_INPUT_PARAMETERS 6
#define ERR_UNKNOWN_CACHE_POLICY 7

#endif	/* PLACES_HPP */

