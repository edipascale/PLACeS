/* 
 * File:   Topology.hpp
 * Author: emanuele
 *
 * Created on 27 April 2012, 11:18
 */

#ifndef TOPOLOGY_HPP
#define	TOPOLOGY_HPP

#include "ContentElement.hpp"
#include <string>
#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/graph_traits.hpp"
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <boost/property_map/dynamic_property_map.hpp>

// forward declarations to avoid include circles
class Scheduler;
class Flow;

using std::string;

/**
 * enum to differentiate upstream, downstream, metro and core (symmetric) links.
 * Note that this is not assigned to the edge, but rather computed through the
 * values of ponCustomers of the two vertices connected by the edge. 
 */
enum EdgeType {UPSTREAM, DOWNSTREAM, METRO, CORE, UNKNOWN_TYPE};

/**
 *  A struct used to define bundled properties for nodes in the topology graph.
 */
struct NetworkNode {
    int ponCustomers; /**< number of customers attached to this PON node. if == 0, then this is a core node and not a PON. */
    int asid; /**< All the PONs attached to the same (set of) core node(s) belong to the same Access Section (AS). This is needed to compute locality. */
};
/**
 * A struct used to define bundled properties for edges in the topology graph.
 * Note that edges are uni-directional, i.e., a link is a composed by a pair
 * of edges with opposite directions.
 */
struct NetworkEdge {
    double length; /**< The length of the edge in meters. */
    Capacity maxCapacity; /**< The maximum bandwidth available on this edge. */
    Capacity spareCapacity; /**< The amount of bandwidth currently availabe on this edge, i.e., after substracting the capacity already used by Flows transiting on it. */
    std::set<Flow*> activeFlows; /**< The set of Flows actively using this edge at the moment. */
    Capacity peakCapacity; /**< The highest bandwidth collectively used by Flows on this edge at any given time in the current simulation round. */
    EdgeType type; /**< The EdgeType of this edge. Used to differentiate between core, metro and access traffic. */
};

// Our graph template
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, 
        NetworkNode, NetworkEdge> DGraph;
// Utility definitions to save time
typedef DGraph::vertex_descriptor Vertex;
typedef boost::graph_traits<DGraph>::vertex_iterator VertexIterator;
typedef boost::graph_traits<DGraph>::edge_descriptor Edge;
typedef boost::graph_traits<DGraph>::edge_iterator EdgeIterator;
typedef std::vector<Vertex> VertexVec;
typedef std::map<unsigned int, std::vector<Vertex> > PredecessorMap;

typedef std::map<unsigned int, Vertex> VertexMap;
typedef std::map<Vertex, std::vector<unsigned int> > DistanceMap;
typedef std::map<Edge, Capacity> LoadMap;

/**
 * A struct used to keep track of a number of statistics on traffic observed
 * during the simulations. Each vector has as many elements as the number of
 * rounds in the simulation; at the end of each round the appropriate statistics
 * are computed and saved here.
 */
struct NetworkStats {
  std::vector<Capacity> avgTot; /**< Average traffic observed on an edge, across the whole topology. */ 
  std::vector<Capacity> avgCore; /**< Average traffic observed on a core edge. */
  std::vector<Capacity> avgAccessUp; /**< Average traffic observed on an upstream access edge. */
  std::vector<Capacity> avgAccessDown; /**< Average traffic observed on a downstream access edge. */
  std::vector<Capacity> peakCore; /**< Peak traffic observed on a core edge. */
  std::vector<Capacity> peakAccessUp; /**< Peak traffic observed on an upstream access edge. */
  std::vector<Capacity> peakAccessDown; /**< Peak traffic observed on a downstream access edge. */
  std::vector<Capacity> avgPeakCore; /**< Average peak traffic observed on a core edge. */
  std::vector<Capacity> avgPeakAccessUp; /**< Average peak traffic observed on an upstream access edge. */
  std::vector<Capacity> avgPeakAccessDown; /**< Average peak traffic observed on a downstream access edge. */
  std::vector<Capacity> avgMetro; /**< Average traffic observed on a metro edge (if present). */
  std::vector<Capacity> peakMetro; /**< Peak traffic observed on a metro edge (if present). */
  std::vector<Capacity> avgPeakMetro; /**< Average peak traffic observed on a metro edge (if present). */
};

/**
 * A representation of the physical topology over which the data is transmitted.
 * It is based on the Boost Graph library, specifically on the Adjacency_List.
 * Bundled properties are used to store information of interests for edges and
 * links. Routing, bandwidth assignment and traffic statistic collection are all
 * performed by this class.
 */
class Topology {
protected:
    DGraph topology; /**< The graph representation of the topology. */
    uint numVertices; /**< The total number of vertices in the topology. */
    uint numEdges; /**< The total number of edges in the topology. */
    uint numMetroEdges; /**< The number of metro edges in the topology. */
    uint numCoreEdges; /**< The number of core edges in the topology. */
    uint numCustomers; /**< The total number of custoemrs (i.e., end-users) in the topology. */
    uint numASes;  /**< The total number of Access Sections (ASes) in the topology. Typically each core (or metro/core) node has an associated AS. */
    DistanceMap dMap; /**< A map keeping track of the distance between vertices, used for routing purposes. */
    PredecessorMap pMap; /**< A map of predecessors in the pre-computed shourtest path routes, used for routing purposes. */
    Vertex centralServer; /**< The core network vertex where we are placing the central repository, i.e., the source of all ContentElement when they are first introduced in the catalog or when there is no other source available. */
    VertexMap asCacheMap; /**< */
    NetworkStats stats; /**< A collection of statistic measurements of the traffic circulating over this topology. @see NetworkStats */
    VertexVec ponNodes; /**< */
    string fileName; /**< */
    LoadMap loadMap; /**< A Map associating to each edge the total traffic it has observed. Used to compute the traffic statistics at the end of each round. */
    uint bitrate; /**< While not technically a topology parameter, the bitrate of the encoded content is required both in updateCapacity to estimate the time at which users will change channel (and thus to set the userViewEta) and to figure out if there's enough capacity to serve a new customer.*/
    Capacity minFlowIncrease;/**< minFlowIncrease is used in updateCapacity when adding bandwidth to a flow
     * due to the removal of some completed flow; if the increase is below this
     * threshold, no update is performed (to reduce unnecessary small changes
     * and speed up simulations). Note: this seems to hardly have any effect on
     * simulation speed, and might be removed in the future. @deprecated */
    boost::dynamic_properties snapDp; /**< dynamic_properties are used to take snapshots of the network in
     * Topology::printTopology(), e.g., write a graphml file with all the network metrics.
     * Generating this object every time in that method would be needlessly
     * expensive, so it's populated in the constructor provided that snapshots
     * are actually going to be taken (i.e., if sanpshotFreq > 0). */
    std::map<uint, uint> ASCustomersMap; /**< A map that keeps track of how many peers are present in each AS.
     * This is needed when calculating content rates in the cache optimization
     * problem. The map is populated in the constructor, when determining the 
     * number of users per PON. */
    
    // Utility methods to streamline internal subroutines.
    /**
     * Iteratively adjusts the available bandwidth at each of the edges that 
     * compose the specified route.
     * @param route The route we want to update the capacity of.
     * @param toAdd The amount of bandwidth that needs to be added (or subtracted, if negative) to each of the edges of the specified route.
     */
    void updateRouteCapacity(std::vector<Edge> route, Capacity toAdd);
    
    /**
     * Updates the ETA of a Flow after a bandwidth assignment change.
     * @param flow The Flow whose bandwidth has just changed.
     * @param scheduler A pointer to the Scheduler, which is needed to update the queue order in case of a change of ETA.
     */
    void updateEta(Flow* flow, Scheduler* scheduler);
    
public:
    Topology(string fileName, po::variables_map vm);
    VertexVec getPonNodes() const;
    uint getDistance(uint source, uint dest) const;
    std::vector<Edge> getRoute(uint source, uint dest);
    // utility method after the addition of PonUsers
    std::vector<Edge> getRoute(PonUser source, PonUser destination) {
      return this->getRoute(source.first, destination.first);
    }
    void updateCapacity(Flow* flow, Scheduler* scheduler, 
          bool addNotRemove);
    void printNetworkStats(uint currentRound, uint roundDuration);
    void resetFlows();
    void updateLoadMap(Flow* flow);
    void resetLoadMap();
    Vertex getCentralServer();
    bool isLocal(Vertex source, Vertex dest);
    Vertex getLocalCache(Vertex node);
    VertexVec getLocalCacheNodes();
    bool isCongested(PonUser source, PonUser destination);
    bool addEdge(Vertex src, Vertex dest, Capacity cap, EdgeType type);
    void printTopology(SimTime time, uint round);
    uint getAsid(PonUser node) {
      return topology[node.first].asid;
    }
    EdgeType getEdgeType(Edge e) const;
    uint getPonCustomers(Vertex v) const;

  string getFileName() const {
    return fileName;
  }

  uint getNumCustomers() const {
    return numCustomers;
  }

  uint getNumASes() const {
    return numASes;
  }

  NetworkStats getNetworkStats() const {
    return this->stats;
  }
  
  /* returns the number of customers for a given AS id
   */
  uint getASCustomers(uint asid) {
    return ASCustomersMap.at(asid);
  }
};

#endif	/* TOPOLOGY_HPP */

