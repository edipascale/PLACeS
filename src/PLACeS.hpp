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
/**
 * Basic granularity of time in the simulator (1 second).
 */
typedef int SimTime;
/**
 * Measure of data size, bandwidth etc., expressed in Mbps.
 */
typedef double Capacity ;  // in Mbps

/**
 * A generic user is identified by a pair of int, the first of which identifies
 * the PON in that Access Section (AS) and the second of which identifies the 
 * user within the PON (as PONs are shared between TopologyOracle::ponCardinality 
 * users).
 */
typedef std::pair<unsigned int, unsigned int> PonUser;

/**
 * Simulate either a VoD system or a time-shifted IPTV system: has consequences
 * on the popularity model used, on the length of simulation rounds, on how
 * users requests are generated etc.
 */ 
enum SimMode {VoD, IPTV};

/**
 * INF_TIME represents a placeholder infinite SimTime.
 * 
 * It is used whenever the actual desired SimTime cannot be computed yet,
 * i.e., the starting time of a transfer when a source has not been defined yet
 * or its ETA when the bandwidth assigned to that flow has not been defined.
 */
const SimTime INF_TIME = std::numeric_limits<int>::max();
/**
 * UNKNOWN is used as a placeholder for an unspecified PonUser.
 * 
 * It is used whenever the actual PonUser required has not been defined yet,
 * i.e., for the source field of a Flow which has not been served yet.
 */
const PonUser UNKNOWN = std::make_pair(std::numeric_limits<uint>::max(),
        std::numeric_limits<uint>::max());
/**
 * MAX_FLOW_SPEED represents the maximum bandwidth a Flow is allowed to receive.
 * 
 * It is meant to roughly simulate bandwidth constraints imposed by the operators
 * and the effect of TCP congestion control mechanics (since we do not simulate
 * packet loss).
 */
const Capacity MAX_FLOW_SPEED = 1024; // 1Gbps

/**
 * UNLIMITED represents an infinite bandwidth capacity for a link.
 * 
 * It is used when we do not want to simulate link congestion in the core, i.e.,
 * under the assumption that core bandwidth is provisioned based on the peak 
 * request and thus will never be the limiting factor.
 */
const Capacity UNLIMITED = std::numeric_limits<double>::max();

// Error exit codes, deprecated (use BOOST_LOG_TRIVIAL instead)
#define ERR_FAILED_ROUTING 1
#define ERR_VANILLA_EVENT 2
#define ERR_NO_EVENT_HANDLE 3
#define ERR_EVENT_IN_THE_PAST 4
#define ERR_HANDLEMAP_INSERT 5
#define ERR_INPUT_PARAMETERS 6
#define ERR_UNKNOWN_CACHE_POLICY 7

#endif	/* PLACES_HPP */

