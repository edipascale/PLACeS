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
    VertexMap asCacheMap; /**< A map matching each AS identifier with the vertex where the CDN server is located. */
    NetworkStats stats; /**< A collection of statistic measurements of the traffic circulating over this topology. @see NetworkStats */
    VertexVec ponNodes; /**< A vector of all the graph vertices with a non-zero number of NetworkNode::ponCustomers attached to them. These vertices will have a single link connecting them to a metro or metro/core node, representing the shared fiber tree of a PON. */
    string fileName; /**< The name of the input file used to generate the topology. */
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
    /**
     * Builds the topology from a topology file. The recommended option is to
     * specify a graphml topology file (which MUST have a .graphml extension). 
     * Look at some of the default graphml topologies in the Topologies folder
     * for examples of the properties recognized by the simulator. Comments 
     * inside the constructor's code itself can also be useful in this regard.
     * 
     * Alternatively, the topology can be specified using a text file with the following conventions 
     * (no quotes, #something means number of elements of type something, arguments
     * in brackets are optional):
     * The first line must be "#CoreNodes #CoreEdges". #CoreEdges is assumed to be 
     * the number of undirected core edges, there will actually be 2*#CoreEdges in 
     * the graph (see below). 
     * Then #CoreNodes lines should follow with the format: 
     * "asid #PONNodes AvgPONCustomers DevPONCustomers downstreamCapacity upstreamCapacity (cs)"
     * asid is the identifier of the metro/core node and its relative Access Segment;
     * it is currently ignored and assumed to be a progressive integer starting from 0.
     * #PONNodes is the number of PONs attached to that metro/core node;
     * AvgPONCustomers is the mean value of a gaussian distribution describing the
     * number of customers attached to each of this node's PONs;
     * DevPONCustomers is the standard deviation of the above mentioned distribution;
     * downstreamCapacity and upstreamCapacity are the capacities in Mbps of each PON;
     * cs (optional) indicates that this metro/core node hosts the central repository
     * server.
     * After this, #CoreEdges lines should follow, each defining an edge between 
     * two metro/core nodes, in the following format: 
     * "sourceId destId capacity capacityRev"
     * The two capacities are used to define respectively the capacities
     * of the sourceId->destId edge and the destId->sourceId edge (to account for 
     * asymmetric links); if negative, it is assumed to be infinite.
     * The graph is directed but for each pair of vertices, sourceId and destId, two 
     * links (one per direction) are created.
     * 
     * @param fileName the name of the file from which the topology should be generated.
     * @param vm the set of simulation parameters specified at command line by the user.
     */
    Topology(string fileName, po::variables_map vm);
    /**
     * Retrieves all the vertices with customers attached to them.
     * In other words, all the vertices with a non-zero value of
     * NetworkNode::ponCustomers. 
     * @return a vector of vertices with end-users attached to them.
     */
    VertexVec getPonNodes() const;
    /**
     * Computes the hop-distance between two vertices.
     * @param source The source vertex of the route.
     * @param dest The destination vertex of the route.
     * @return The number of hops required to go from source to dest over the shortest route.
     */
    uint getDistance(uint source, uint dest) const;
    /**
     * Retrieves the shortest path between a source and a destination Vertex.
     * @param source The source vertex of the route.
     * @param dest The destination vertex of the route.
     * @return A vector of Edges that constitute the shortest route between source and dest.
     */
    std::vector<Edge> getRoute(uint source, uint dest) const;
    /**
     * Retrieves the shortest path between a source and a destination user.
     * @param source The source user of the route.
     * @param destination The destination user of the route.
     * @return A vector of Edges that constitute the shortest route between source and destination.
     */
    std::vector<Edge> getRoute(PonUser source, PonUser destination) const {
      return this->getRoute(source.first, destination.first);
    }
    /**
     * Updates the available capacity over the edges of the topology after the 
     * addition or removal of a data Flow. 
     * A centralized bandwidth sharing algorithm is implemented here. Each streaming 
     * flow transiting over a particular link is entitled to at least an equal share 
     * of the available capacity of that link. This process is repeated across each 
     * link of the flow's route to identify the tightest capacity constraint 
     * (i.e., the bottleneck), which determines the capacity assigned to the flow. 
     * This capacity is updated dynamically whenever a flow enters (respectively leaves) 
     * the network; however, only the bottleneck for the new (respectively leaving) 
     * flow is used to calculate the new capacity allocation. This is done to 
     * alleviate the time complexity of the bandwidth sharing algorithm, 
     * at the expense of some loss in terms of optimal resource allocation. 
     * No individual flow is allowed to receive more than MAX_FLOW_SPEED of 
     * bandwidth, in an attempt to model both network operators constraints on 
     * maximum available capacity per user and the limits imposed by TCP 
     * mechanisms (e.g., due to the congestion control mechanisms, as we do not 
     * simulate packet loss). 
     * @param flow The data flow that has been just added or removed.
     * @param scheduler A pointer to the Scheduler, which will ensure that any change to the ETA of some Flow will be reflected in the ordering of the queued events.
     * @param addNotRemove True if the flow is a new transfer being added, false if it is a completed flow that needs to be removed.
     */
    void updateCapacity(Flow* flow, Scheduler* scheduler, 
          bool addNotRemove);
    /**
     * Computes the traffic statistics for the round that just ended, stores them 
     * in the NetworkStats structures, and prints them to screen.
     * @param currentRound The simulation round that just ended.
     * @param roundDuration The length in seconds of the simulation round that just ended.
     */
    void printNetworkStats(uint currentRound, uint roundDuration);
    /**
     * Removes all the Flows from the network edges, restoring their capacity
     * to their original maximum. 
     */
    void resetFlows();
    /**
     * Updates the LoadMap by adding the data transmitted through the specified 
     * Flow to each of the Edges composing its route. Invoked by the TopologyOracle
     * once the Flow has completed its transfer.
     * @param flow The Flow that has just completed its data transfer.
     */
    void updateLoadMap(Flow* flow);
    /**
     * Resets the content of LoadMap, setting the total transferred Mb for each
     * Edge in the topology to 0.
     */
    void resetLoadMap();
    /**
     * Retrieves the Vertex where the central repository is located.
     * @return The Vertex where the central repository is located.
     */
    Vertex getCentralServer() const;
    /**
     * Checks whether a Flow between source and dest would be local, i.e., if it
     * could be completed without leaving the Access Section (AS) of the destination.
     * @param source The proposed source for the Flow.
     * @param dest The destination of the Flow, i.e., the requester of the video item.
     * @return True if the transfer between source and dest would be local, false otherwise.
     */
    bool isLocal(Vertex source, Vertex dest) const;
    /**
     * Retrieves the Vertex hosting the local CDN cache for the AS of the specified node.
     * @param node The Vertex for which we are seeking the local CDN.
     * @return The Vertex hosting the local CDN cache for the AS of the specified node.
     */
    Vertex getLocalCache(Vertex node) const;
    /**
     * Retrieves a vector with all the Vertices hosting local CDN caches for some AS.
     * @return A vector with all the Vertices hosting local CDN caches for some AS.
     */
    VertexVec getLocalCacheNodes() const;
    /**
     * Checks whether there is enough spare capacity in the network to serve a request
     * from source to destination.
     * @param source The proposed source for the item requested.
     * @param destination The destination of the request.
     * @return True if there is enough bandwidth to at least transfer the item at its encoding bitrate, false otherwise.
     */
    bool isCongested(PonUser source, PonUser destination) const;
    /**
     * Adds a new uni-directional Edge between two Vertices. Utility method that 
     * is not currently used in the simulator.
     * @deprecated
     * @param src The origin Vertex of the new Edge.
     * @param dest The destination Vertex of the new Edge.
     * @param cap The maximum capacity available on the new Edge. If negative, it is assumed to be UNLIMITED.
     * @param type The EdgeType of the new Edge.
     * @return True if the new Edge was successfully added, false otherwise.
     */
    bool addEdge(Vertex src, Vertex dest, Capacity cap, EdgeType type);
    /**
     * Prints a graphml snapshot of the current topology, including the available
     * capacity on metro/core edges. Useful to visualize the traffic generated
     * by the model being used.
     * @param time The SimTime at which the snapshot is being taken.
     * @param round The current round of simulation.
     */
    void printTopology(SimTime time, uint round);
    /**
     * Retrieves the index of the Access Section (AS) to which the specified user belongs.
     * @param node The user we want to know the AS of.
     * @return The index of the AS of the specified user.
     */
    uint getAsid(PonUser node) const {
      return topology[node.first].asid;
    }
    /**
     * Returns the type of the specified Edge, i.e., whether it is a core, metro,
     * or access edge.
     * @param e The Edge we want to know the type of.
     * @return The EdgeType of the specified Edge.
     * @see EdgeType
     */
    EdgeType getEdgeType(Edge e) const;
    /**
     * Retrieves the number of customers attached to the specified Vertex.
     * @param v The Vertex we want to know the number of customers attached to.
     * @return The number of cusotmers attached to the specified Vertex.
     */
    uint getPonCustomers(Vertex v) const;

  string getFileName() const {
    return fileName;
  }
  /**
   * Returns the total number of customers (or end-users) present in the network.
   * @return The total number of customers present in the network.
   */
  uint getNumCustomers() const {
    return numCustomers;
  }
  /**
   * Returns the total number of Access Sections (ASes) present in the network.
   * @return The total number of ASes present in the network.
   */
  uint getNumASes() const {
    return numASes;
  }

  NetworkStats getNetworkStats() const {
    return this->stats;
  }
  
  /**
   * Retrieves the number of customers for a given Access Section (AS).
   * @param asid The identifier of the AS we want to know the number of customers of.
   */
  uint getASCustomers(uint asid) const {
    return ASCustomersMap.at(asid);
  }
};

#endif	/* TOPOLOGY_HPP */

