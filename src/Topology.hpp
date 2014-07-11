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

/* enum to differentiate upstream, downstream and core (symmetric) links;
 * note that this is not assigned to the edge, but rather computed through the
 * values of ponCustomers of the two vertices connected by the edge. 
 */
enum EdgeType {UPSTREAM, DOWNSTREAM, METRO, CORE, UNKNOWN_TYPE};

// Create structs to define bundled properties for the graph
struct NetworkNode {
  /* number of customers attached to this PON node. if == 0, then this is a core
   * node and not a PON.
   */
    int ponCustomers;
  /* all the PONs attached to the same (set of) core node(s) belong to the same 
   * AS (this is needed to compute locality in terms of access sections)
   */
    int asid;
};
struct NetworkEdge {
    double length;
    Capacity maxCapacity;
    Capacity spareCapacity;
    std::set<Flow*> activeFlows;
    Capacity peakCapacity;
    EdgeType type;
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

struct NetworkStats {
  std::vector<Capacity> avgTot, avgCore, avgAccessUp, avgAccessDown, peakCore,
      peakAccessUp, peakAccessDown, avgPeakCore, avgPeakAccessUp, 
      avgPeakAccessDown, avgMetro, peakMetro, avgPeakMetro;
};

class Topology {
protected:
    DGraph topology;
    uint numVertices;
    uint numEdges;
    uint numMetroEdges;
    uint numCoreEdges;
    uint numCustomers;
    uint numASes;
    DistanceMap dMap;
    PredecessorMap pMap;
    Vertex centralServer;
    VertexMap asCacheMap;
    NetworkStats stats;
    VertexVec ponNodes;
    string fileName;
    LoadMap loadMap;
    /* While not technically a topology parameter, the bitrate of the encoded 
     * content is required both in updateCapacity to estimate the time at which
     * users will change channel (and thus to set the userViewEta) and to
     * figure out if there's enough capacity to serve a new customer.
     */
    uint bitrate;
    /* minFlowIncrease is used in updateCapacity when adding bandwidth to a flow
     * due to the removal of some completed flow; if the increase is below this
     * threshold, no update is performed (to reduce unnecessary small changes
     * and speed up simulations). Note: this seems to hardly have any effect on
     * simulation speed, and might be removed in the future.
     */
    Capacity minFlowIncrease;
    /* dynamic_properties are used to take snapshots of the network in
     * printTopology, e.g. write a graphml file with all the network metrics.
     * Generating this object every time in that method would be needlessly
     * expensive, so it's populated in the constructor provided that snapshots
     * are actually going to be taken (i.e. sanpshotFreq > 0).
     */
    boost::dynamic_properties snapDp;
    /* the following map keeps track of how many peers are present in each AS.
     * This is needed when calculating content rates in the cache optimization
     * problem. The map is populated in the constructor, when determining the 
     * number of users per PON.
     */
    std::map<uint, uint> ASCustomersMap;
    //utility methods
    void updateRouteCapacity(std::vector<Edge> route, Capacity toAdd);
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
          bool addNotRemove = true);
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

