#include "Topology.hpp"
#include <fstream>
#include "Scheduler.hpp"

#include <boost/graph/detail/adjacency_list.hpp>
#include <sstream>
#include <stdlib.h>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/graph/graphml.hpp>

extern boost::mt19937 gen;

/* Utility method to add a PON link to the network
 */
bool Topology::addEdge(Vertex src, Vertex dest, Capacity cap) {
  auto result = boost::add_edge(src, dest, this->topology);
  if (result.second) {
    if (cap < 0) {
      // this link capacity is unlimited
      topology[result.first].maxCapacity = UNLIMITED;
      topology[result.first].spareCapacity = UNLIMITED;
    } else {
      topology[result.first].maxCapacity = cap;
      topology[result.first].spareCapacity = cap;
    }
    topology[result.first].peakCapacity = 0;
    topology[result.first].activeFlows.clear();
  } else {
    BOOST_LOG_TRIVIAL(warning) << "Topology::Topology() - could not add edge between vertices "
            << src << " and " << dest << std::endl;
  }
  return result.second;
}

/* Builds the topology from a topology file, either from a graphml topology file
 * (which MUST have a .grapml extension) or a text file with the following conventions 
 * (no quotes, #something means number of elements of type something, arguments
 * in brackets are optional)
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
 */
Topology::Topology(string fileName, po::variables_map vm) {
  this->minFlowIncrease = std::max(vm["min-flow-increase"].as<double>(),0.0);
  this->fileName = fileName;
  this->bitrate = vm["bitrate"].as<uint>();
  uint ponCardinality = vm["pon-cardinality"].as<uint>();
  using namespace std;
  centralServer = std::numeric_limits<unsigned int>::max();
  numCoreEdges = 0;
  numCustomers = 0;
  // initialize dynamic_properties for the network snapshot if needed
  if (vm["snapshot-freq"].as<uint>() > 0) {
    snapDp.property("asid", boost::get(&NetworkNode::asid, topology));
    snapDp.property("ponCustomers", boost::get(&NetworkNode::ponCustomers, topology));
    snapDp.property("length", boost::get(&NetworkEdge::length, topology));
    snapDp.property("maxCapacity", boost::get(&NetworkEdge::maxCapacity, topology));
    snapDp.property("spareCapacity", boost::get(&NetworkEdge::spareCapacity, topology));
    snapDp.property("peakCapacity", boost::get(&NetworkEdge::peakCapacity, topology));
    boost::associative_property_map<LoadMap> loadPMap(this->loadMap);
    snapDp.property("totalLoad", loadPMap);
    //TODO: create property for central server identification
  }
  
  ifstream stream;
  string graphml = ".graphml";
  stream.open(fileName.c_str());
  if (!stream.is_open()) {
    cerr << "ERROR: Topology::Topology() - Could not open specified input "
            "file " << fileName.c_str() << endl;
	  abort();
  }
  else if (fileName.length() > graphml.length() &&
    0 == fileName.compare(fileName.length()-graphml.length(),graphml.length(),graphml)) 
  {
    //read it as a graphml graph
    // first define attributes that are mapped directly to bundled properties
    boost::dynamic_properties dp(&boost::ignore_other_properties);
    dp.property("asid", boost::get(&NetworkNode::asid, topology));
    dp.property("ponCustomers", boost::get(&NetworkNode::ponCustomers, topology));
    dp.property("length", boost::get(&NetworkEdge::length, topology));
    dp.property("maxCapacity", boost::get(&NetworkEdge::maxCapacity, topology));
    dp.property("spareCapacity", boost::get(&NetworkEdge::spareCapacity, topology));
    dp.property("peakCapacity", boost::get(&NetworkEdge::peakCapacity, topology));
    /* then define property maps for those attributes which won't be stored in
     * vertices or edges, but that are required to properly build the topology
     */
    std::map<Vertex, int> avgUsersMap, numPonMap;
    boost::associative_property_map<std::map<Vertex, int> >
        avgUsersPMap(avgUsersMap);
    // mean number of users per PON attached to each core vertex
    dp.property("avgUsers", avgUsersPMap);
    boost::associative_property_map<std::map<Vertex, int> > 
        numPonPMap(numPonMap);
    // number of PON vertices to generate attached to each core vertex
    dp.property("numPon", numPonPMap);
    std::map<Vertex, Capacity> devUsersMap, upCapMap, downCapMap;
    boost::associative_property_map<std::map<Vertex, Capacity> >
        devUsersPMap(devUsersMap);
    // std. dev. of users per PON attached to each core vertex
    dp.property("devUsers", devUsersPMap);
    boost::associative_property_map<std::map<Vertex, Capacity> >
        upCapPMap(upCapMap);
    // upstream capacity of edges between PONs and their core vertices
    dp.property("upCapacity", upCapPMap);
    boost::associative_property_map<std::map<Vertex, Capacity> >
        downCapPMap(downCapMap);
    // downstream capacity of edges between PONs and their core vertices
    dp.property("downCapacity", downCapPMap);
    std::map<Vertex, bool> csMap;
    boost::associative_property_map<std::map<Vertex, bool> >
        csPMap(csMap);
    // true if this node is the central server. If multiple true values are 
    // present, only the last one is recorded (there can be only one cs)
    dp.property("centralServer", csPMap);
    // try to read the file    
    try {
      boost::read_graphml(stream, this->topology, dp);
    } catch (boost::parse_error pe) {
      BOOST_LOG_TRIVIAL(error) << pe.what();
      abort();
    } catch (boost::bad_parallel_edge bpe) {
      BOOST_LOG_TRIVIAL(error) << bpe.what();
      abort();
    } catch (boost::directed_graph_error dge) {
      BOOST_LOG_TRIVIAL(error) << dge.what();
      abort();
    } catch (boost::undirected_graph_error uge) {
      BOOST_LOG_TRIVIAL(error) << uge.what();
      abort();
    } 
    this->numCoreEdges = boost::num_edges(topology);
    // if capacity of some edge is negative, make it unlimited
    BOOST_FOREACH(Edge e, boost::edges(topology)) {
      if (topology[e].maxCapacity < 0) {
        topology[e].maxCapacity = UNLIMITED;
        topology[e].spareCapacity = UNLIMITED;
      } else {
        topology[e].spareCapacity = topology[e].maxCapacity;
      }
      topology[e].peakCapacity = 0;
    }
    // collect core vertices (the ones defined in the graph))
    VertexVec coreVertices(boost::vertices(topology).first, 
            boost::vertices(topology).second);
    BOOST_FOREACH(Vertex v, coreVertices) {
      int avgPonC = avgUsersMap.at(v);
      double devPonC = devUsersMap.at(v);
      int asid = topology[v].asid;
      /* FIXME: if multiple core nodes belong to the same AS, the insertion will
       * fail; each asid should be associated to a set, not to a single vertex.
       */
      if (asCacheMap.insert(std::pair<Vertex, int>(v,asid)).second == true)
        this->numASes++;
      if (csMap.at(v) == true)
        centralServer = boost::vertex(v, topology);
      boost::random::normal_distribution<> ponDist(avgPonC, devPonC);
      int ponC(0), ponNum(numPonMap.at(v));
      Capacity upCapacity(upCapMap.at(v)), downCapacity(downCapMap.at(v));
      // for each PON node, determine the number of active customers
      for (auto j = 0; j < ponNum; j++) {
        Vertex pon = boost::add_vertex(topology);
        // checks if the topology values should be overridden
        if (ponCardinality > 0)
          ponC = ponCardinality;
        else
          ponC = std::max((int) std::floor(ponDist(gen) + 0.5), 0);
        numCustomers += ponC;
        topology[pon].ponCustomers = ponC;
        topology[pon].asid = asid;
        //add this to the PON Nodes list
        ponNodes.push_back(pon);
        // create the downstream PON link
        auto result = this->addEdge(v, pon, downCapacity);
        assert(result);
        // create the upstream PON link
        result = this->addEdge(pon, v, upCapacity);
        assert(result);
      }
    }
    this->numEdges = boost::num_edges(topology);
    this->numVertices = boost::num_vertices(topology);
    // end of graphml topology import
  } else {
    // read it as a text file with our custom-defined schema
	  string line, cs;
    line.clear();
    cs.clear();
    uint sourceId, destId, asId, vertices, edges, ponNum, ponC;
    double avgPonC, devPonC;
    Capacity capacity, revCapacity;
	  istringstream lineBuffer;
    lineBuffer.clear();
    // Read number of core nodes and core edges
    getline(stream,line);
    lineBuffer.str(line);            
    lineBuffer >> vertices >> edges; 
    this->numASes = vertices;
    // add metro/core nodes to the topology
    for (int i = 0; i < vertices; i ++) {
      Vertex node = boost::add_vertex(topology);
      topology[node].ponCustomers = 0; // core nodes have no customers
      topology[node].asid = i;
      asCacheMap.insert(std::make_pair(i,node));
    }
    // generate PON nodes for each metro/core node    
    for (int i = 0; i < vertices; i++) {
      line.clear();
      lineBuffer.clear();
      getline(stream,line);
      lineBuffer.str(line);
      lineBuffer >> asId >> ponNum >> avgPonC >> devPonC 
              >> capacity >> revCapacity;
      if (lineBuffer.peek() != EOF) {
        lineBuffer >> cs;
        if (cs.compare("cs") == 0)
          centralServer = boost::vertex(i, topology);
      }
      boost::random::normal_distribution<> ponDist(avgPonC, devPonC);
      // for each PON node, determine the number of active customers
      for (int j = 0; j < ponNum; j++) {
        Vertex pon = boost::add_vertex(topology);
        // checks if the topology values should be overridden
        if (ponCardinality > 0) 
          ponC = ponCardinality;
        else 
          ponC = std::max((int)std::floor(ponDist(gen) + 0.5),0);
        numCustomers += ponC;
        topology[pon].ponCustomers = ponC;
        topology[pon].asid = i;
        //add this to the PON Nodes list
        ponNodes.push_back(pon);
        // create the downstream PON link
        auto result = this->addEdge(i, pon, capacity);
        assert(result);
        // create the upstream PON link
        result = this->addEdge(pon, i, revCapacity);
        assert(result);
      }
    }
    // read the core links from the topology file
	  for (uint i = 0; i < edges; i ++) {
      line.clear();
      lineBuffer.clear();
      getline(stream,line);
      lineBuffer.str(line);            
      lineBuffer >> sourceId >> destId >> capacity >> revCapacity;
      auto result = this->addEdge(sourceId, destId, capacity);
      if (result) {
        numCoreEdges++;
      }
      assert(result);
      // Reverse link
      result = this->addEdge(destId, sourceId, revCapacity);
      if (result) {
        numCoreEdges++;
      }
      assert(result);
    }
    numVertices = boost::num_vertices(topology);
    numEdges = boost::num_edges(topology);
  }
  stream.close();
  // Finds shortest path distances between all pairs of vertices
  for ( unsigned int vIt = 0; vIt < numASes; vIt++) {
    Vertex v = boost::vertex(vIt, topology);
    /* new approach: you don't actually need pMaps for ONU nodes, you know 
     you have to go into the core node (the only outEdge you have) and from
     there you can use that node's pMap. Distance is distance from that node +1 
     */
    assert(topology[vIt].ponCustomers == 0);
    std::vector<Vertex> pVec;
    pVec.resize(numVertices, vIt);
    std::vector<unsigned int> dVec;
    dVec.resize(numVertices, 0);
    boost::dijkstra_shortest_paths(topology, v,
            boost::weight_map(boost::get(&NetworkEdge::length, topology)).
            predecessor_map(&pVec[0]).
            distance_map(&dVec[0]));
    assert(dMap.insert(std::make_pair(v, dVec)).second == true);
    assert(pMap.insert(std::make_pair(vIt, pVec)).second == true);
  }
  // Initialize Network statistics
  BOOST_FOREACH(Edge e, boost::edges(topology)) {
    loadMap.insert(std::pair<Edge, Capacity>(e,0));
  }
  // reserve memory for the historical stats vectors
  uint numRounds = vm.at("rounds").as<uint>();
  stats.avgTot.assign(numRounds, 0);
  stats.avgCore.assign(numRounds, 0);
  stats.avgAccessDown.assign(numRounds, 0);
  stats.avgAccessUp.assign(numRounds, 0);
  stats.avgPeakCore.assign(numRounds, 0);
  stats.avgPeakAccessDown.assign(numRounds, 0);
  stats.avgPeakAccessUp.assign(numRounds, 0);
  stats.peakCore.assign(numRounds, 0);
  stats.peakAccessDown.assign(numRounds, 0);
  stats.peakAccessUp.assign(numRounds, 0);
  
}

VertexVec Topology::getPonNodes() const {
  return ponNodes;
}

uint Topology::getPonCustomers(Vertex v) const {
  return topology[v].ponCustomers;
}

uint Topology::getDistance(uint source, uint dest) const {
  if (topology[source].ponCustomers > 0) {
    assert(boost::out_degree(source, topology) == 1);
    Edge outEdge = *(boost::out_edges(source, topology).first);
    Vertex mcnode = boost::target(outEdge, topology);
    return dMap.at(mcnode)[dest] + 1;
  } else
    return dMap.at(source)[dest];
}

std::vector<Edge> Topology::getRoute(uint source, uint dest) {
  VertexVec visitedNodes;
  std::vector<Edge> route;
  visitedNodes.clear();
  Vertex currentNode = boost::vertex(dest, topology);
  Vertex sourceV = boost::vertex(source, topology);
  if (topology[source].ponCustomers > 0) {
    // as ONU nodes have no pMap entry, we need to go to the metro/core node and 
    // route from there. Incidentally, this also solves the issue with users
    // on the same PON exchanging data (the route goes to the m/c node as it 
    // should)
    // NOTE: as the graph is not bidirectedS, in_edges is not available
    assert(boost::out_degree(source, topology) == 1);
    Edge outEdge = *(boost::out_edges(source, topology).first);
    sourceV = boost::target(outEdge, topology);
  }
  
  while (currentNode != sourceV) {
    visitedNodes.push_back(currentNode);
    currentNode = pMap[sourceV][currentNode];
  }
  visitedNodes.push_back(sourceV);
  if (topology[source].ponCustomers > 0)
    visitedNodes.push_back(source);
  // Walk back through the visited nodes and record the edges in the right order
  std::pair<Edge, bool> returnValue;
  for (VertexVec::reverse_iterator rvIt = visitedNodes.rbegin(); 
          rvIt+1 != visitedNodes.rend(); rvIt++) {
    returnValue = boost::edge(*rvIt, *(rvIt+1), topology);
    // assert(returnValue.second == true);
    if (!returnValue.second) {
      std::cerr << "Topology::getRoute() - Failed to retrieve edge from Vertex "
              << *(rvIt-1) << " to Vertex " << *rvIt << std::endl;
      exit(ERR_FAILED_ROUTING);
    }
    route.push_back(returnValue.first);
  }
  return route;
}

Vertex Topology::getCentralServer() {
  return centralServer;
}

void Topology::updateCapacity(Flow* flow, Scheduler* scheduler,
        bool addNotRemove) {
  SimTime now = scheduler->getSimTime();
  std::vector<Edge> flowRoute = this->getRoute(flow->getSource(), 
          flow->getDestination());
  Capacity minSpareCapacity(UNLIMITED), minCut(UNLIMITED), maxBneckBw(UNLIMITED);
  Edge bottleneck = flowRoute.back();
  if (addNotRemove) {
    // new flow, traverse route to determine minSpareCapacity and minCut
    BOOST_FOREACH (Edge e, flowRoute) {
      topology[e].activeFlows.insert(flow);
      minSpareCapacity = std::min(minSpareCapacity, topology[e].spareCapacity);
      if (topology[e].maxCapacity != UNLIMITED && 
            topology[e].maxCapacity / topology[e].activeFlows.size() < minCut) {
        minCut = topology[e].maxCapacity / topology[e].activeFlows.size();
        bottleneck = e;
      }
    }
    if (minSpareCapacity >= MAX_FLOW_SPEED) {
      // there's enough spare capacity, no need to reduce other flows' bw
      flow->setBandwidth(MAX_FLOW_SPEED);
      this->updateRouteCapacity(flowRoute, -MAX_FLOW_SPEED);
      this->updateEta(flow, scheduler);
      return;
    } else {
      // assign bw basing on the bottleneck for this flow
      // rounding to prevent capacity overflow, but need to ensure that we
      // don't round to 0!
      if (std::floor(minCut) > 0)
        minCut = std::floor(minCut); 
      assert(minCut > 0);
      flow->setBandwidth(minCut);
      this->updateRouteCapacity(flowRoute, -minCut);
      this->updateEta(flow, scheduler);
      // reduce bw to other flows to match link capacity constraints
      BOOST_FOREACH(Flow* f, topology[bottleneck].activeFlows) {
        if (f->getBandwidth() > minCut) {
          this->updateRouteCapacity(this->getRoute(f->getSource(), f->getDestination()),
                  f->getBandwidth() - minCut);
          f->updateSizeDownloaded(now);
          f->setBandwidth(minCut);
          this->updateEta(f, scheduler);
        }
      }
      // we need this check to account for rounding errors. 
      if (topology[bottleneck].spareCapacity < 0) {
        // std::cerr << "WARNING: Topology::updateCapacity() - negative spareCapacity ("
        //        << topology[bottleneck].spareCapacity << std::endl;
        topology[bottleneck].spareCapacity = 0;
      }
    }
  }
  else {
    // flow removal is more tricky, as you really can't be conservative when 
    // increasing capacity to a flow - you have to check that you've got
    // enough spare capacity on other links.
    // free bw across the route and check how much bw are other flows consuming
    BOOST_FOREACH(Edge e, flowRoute) {
      topology[e].activeFlows.erase(flow);
      // if this link has infinite capacity, we're done here
      if (topology[e].maxCapacity != UNLIMITED) {
        topology[e].spareCapacity += flow->getBandwidth();
        // minCut is the minimum (across links) of the average bw of impacted flows 
        if (topology[e].activeFlows.size() > 0)
          minCut = std::min(minCut, (topology[e].maxCapacity - topology[e].spareCapacity)
                / topology[e].activeFlows.size());
        // maxBneckBw is the maximum bw a flow can get with fair sharing on the
        // bottleneck for the removed flow route
        if (topology[e].activeFlows.size() > 0 &&
                topology[e].maxCapacity / topology[e].activeFlows.size() < maxBneckBw) {
          maxBneckBw = topology[e].maxCapacity / topology[e].activeFlows.size();
          bottleneck = e;
        }
      }
    }
    if (minCut >= MAX_FLOW_SPEED) {
      // every other flow is already at max speed, no need to do anything else
      return;
    }
    else {
      // rounding to prevent capacity overflow, but need to ensure that we
      // don't round to 0!
      if (std::floor(maxBneckBw) > 0)
        maxBneckBw = std::floor(maxBneckBw); 
      maxBneckBw = std::min (MAX_FLOW_SPEED, maxBneckBw); 
      std::vector<Edge> fRoute;
      BOOST_FOREACH (Flow* f, topology[bottleneck].activeFlows) {
        // check if there's room to increase this flow bw (basing only on the
        // removed flow bottleneck, hence not optimal as the previous method)
        Capacity increase = maxBneckBw - f->getBandwidth();
        if (increase > this->minFlowIncrease) {
          fRoute = this->getRoute(f->getSource(), f->getDestination());
          bool safeToGrow = true;
          BOOST_FOREACH (Edge e, fRoute) {
            if (topology[e].spareCapacity <= increase ) {
              safeToGrow = false;
              break;
            }
          }
          if (safeToGrow) {
            // we've got enough capacity to increase this flow's bw
            this->updateRouteCapacity(fRoute, -increase);
            f->updateSizeDownloaded(now);
            f->setBandwidth(maxBneckBw);
            this->updateEta(f, scheduler);
          }
          fRoute.clear();
        }
      }
      // we need this check to account for rounding errors. 
      if (topology[bottleneck].spareCapacity < 0) {
        // std::cerr << "WARNING: Topology::updateCapacity() - negative spareCapacity ("
        //        << topology[bottleneck].spareCapacity << std::endl;
        topology[bottleneck].spareCapacity = 0;
      }
    }
  }
} 

bool loadCompare(LoadMap::value_type &i1, LoadMap::value_type &i2) {
  return i1.second < i2.second;
}

void Topology::printNetworkStats(uint currentRound, uint roundDuration) {
  Capacity averageTot(0), averageCore(0), averageAccessUp(0), temp(0), max(0);
  Capacity peakAccessUp(0), peakCore(0), avgPeakAccessUp(0), avgPeakCore(0);
  Capacity averageAccessDown(0), avgPeakAccessDown(0), peakAccessDown(0), tempPeak(0);
  /* Take the first edge of the graph and use it to initialize the following 
   * variables; this is required because, if all the core edges have unlimited 
   * capacity, peakCoreEdge would otherwise be returned as an undefined variable
   * in the previous implementation.
   */
  Edge nullE = *(boost::edges(topology).first) ;
  Edge maxEdge(nullE), peakAccessUpEdge(nullE), peakAccessDownEdge(nullE), peakCoreEdge(nullE);
  uint numAccessEdges = (this->numEdges - this->numCoreEdges) / 2;
  BOOST_FOREACH (Edge e, boost::edges(topology)) {
    if (loadMap[e] > 0) {
      temp = loadMap[e] / roundDuration;
      tempPeak = topology[e].peakCapacity;
      averageTot += temp;
      if (this->getEdgeType(e) == CORE) {
        averageCore += temp;
        avgPeakCore += tempPeak;
        if (tempPeak > peakCore) {
          peakCore = tempPeak;
          peakCoreEdge = e;
        }
      }
      else if (this->getEdgeType(e) == UPSTREAM) {
        averageAccessUp += temp;
        avgPeakAccessUp += tempPeak;
        if (tempPeak > peakAccessUp) {
          peakAccessUp = tempPeak;
          peakAccessUpEdge = e;
        }
      }
      else { // DOWNSTREAM
        averageAccessDown += temp;
        avgPeakAccessDown += tempPeak;
        if (tempPeak > peakAccessDown) {
          peakAccessDown = tempPeak;
          peakAccessDownEdge = e;
        }
      }
      if (temp > max) {
        max = temp;
        maxEdge = e;
      }
    }
  }
  // store the values in the stats vectors for future access or manipulation
  stats.avgTot.at(currentRound) = (Capacity)(averageTot / this->numEdges);
  stats.avgCore.at(currentRound) = (Capacity)(averageCore / this->numCoreEdges);
  stats.avgAccessUp.at(currentRound) = (Capacity)(averageAccessUp / numAccessEdges);
  stats.avgAccessDown.at(currentRound) = (Capacity)(averageAccessDown / numAccessEdges);
  stats.peakCore.at(currentRound) = peakCore;
  stats.peakAccessUp.at(currentRound) = peakAccessUp;
  stats.peakAccessDown.at(currentRound) = peakAccessDown;
  stats.avgPeakCore.at(currentRound) = (Capacity)(avgPeakCore / this->numCoreEdges);
  stats.avgPeakAccessUp.at(currentRound) = (Capacity)(avgPeakAccessUp / numAccessEdges);
  stats.avgPeakAccessDown.at(currentRound) = (Capacity)(avgPeakAccessDown / numAccessEdges);
  // Print stats on the screen for human visualization
  std::cout << "Average load: " 
          << stats.avgTot.at(currentRound)
          << " (core: " << stats.avgCore.at(currentRound)
          << "; access_up: " << stats.avgAccessUp.at(currentRound)
          << "; access_down: " << stats.avgAccessDown.at(currentRound)
          << "); maximum average load on edge " << maxEdge.m_source
          << "-" << maxEdge.m_target << " (" 
          << max << ")" << std::endl;
  std::cout << "Average peak core load: " 
          << stats.avgPeakCore.at(currentRound)
          << ", maximum peak core load on edge " << peakCoreEdge.m_source
          << "-" << peakCoreEdge.m_target << " (" 
          << stats.peakCore.at(currentRound) << ")" << std::endl;
  std::cout << "Average peak upstream access load: " 
          << stats.avgPeakAccessUp.at(currentRound)
          << ", maximum peak upstream access load on edge " << peakAccessUpEdge.m_source
          << "-" << peakAccessUpEdge.m_target << " (" 
          << stats.peakAccessUp.at(currentRound) << ")" << std::endl;
  std::cout << "Average peak downstream access load: " 
          << stats.avgPeakAccessDown.at(currentRound)
          << ", maximum peak downstream access load on edge " << peakAccessDownEdge.m_source
          << "-" << peakAccessDownEdge.m_target << " (" 
          << stats.peakAccessDown.at(currentRound) << ")" << std::endl;
}

/* Method to reset network loads at the end of each round, now that we are
 * calculating stats over individual rounds rather than cumulatively over all
 * the past history. 
 * TODO: double-check that this is called at the end of each
 * round and it doesn't mess up with calculations if there are leftover flows
 * from previous round.
 */
void Topology::resetLoadMap() {
  BOOST_FOREACH(Edge e, boost::edges(topology)) {
    loadMap.at(e) = 0;
  }
}

void Topology::resetFlows() {
  BOOST_FOREACH(Edge e, boost::edges(topology)) {
    topology[e].spareCapacity = topology[e].maxCapacity;
    topology[e].activeFlows.clear();
  }
}

void Topology::updateLoadMap(Flow* flow) {
  Capacity cSize = flow->getSizeDownloaded();
  BOOST_FOREACH(Edge e, this->getRoute(flow->getSource(), flow->getDestination())){
    loadMap[e] += cSize;
  }  
}

bool Topology::isLocal(Vertex source, Vertex dest) {
  return (topology[source].asid == topology[dest].asid);
}

Vertex Topology::getLocalCache(Vertex node) {
  return this->asCacheMap.at(topology[node].asid);
}

VertexVec Topology::getLocalCacheNodes() {
  VertexVec cacheNodes;
  for (VertexMap::iterator it = asCacheMap.begin(); it != asCacheMap.end(); it++) {
    cacheNodes.push_back(it->second);
  }
  return cacheNodes;
}

// Checks whether adding a new flow would reduce QoE below the minimal threshold
bool Topology::isCongested(PonUser source, PonUser destination) {
  std::vector<Edge> route = this->getRoute(source, destination);
  bool congested = false;
  BOOST_FOREACH (Edge e, route) {
    if (topology[e].maxCapacity != UNLIMITED && 
        topology[e].spareCapacity < this->bitrate &&
        topology[e].maxCapacity / (topology[e].activeFlows.size()+1) < this->bitrate) {
      congested = true;
      break;
    }
  }
  return congested;
}

// checks whether this is an upstream PON link, downstream PON link or core link
EdgeType Topology::getEdgeType(Edge e) const{
  if (topology[e.m_source].ponCustomers > 0)
    return UPSTREAM;
  else if (topology[e.m_target].ponCustomers > 0)
    return DOWNSTREAM;
  else
    return CORE;
}

void Topology::updateRouteCapacity(std::vector<Edge> route, Capacity toAdd) {
  BOOST_FOREACH(Edge e, route) {
    // Note: because of this check, capacity is actually unlimited, but
    // the peakCapacity is not recorded for unlimited bw links.
    if (topology[e].maxCapacity != UNLIMITED) {
      topology[e].spareCapacity += toAdd;
      // update peakCapacity if need be
      if (toAdd < 0) {
        Capacity usedBw = std::min(topology[e].maxCapacity - topology[e].spareCapacity,
            topology[e].maxCapacity);
        if (usedBw > topology[e].peakCapacity)
          topology[e].peakCapacity = usedBw;
      }
    }
  }
}

void Topology::updateEta(Flow* flow, Scheduler* scheduler) {
  /* Set the flow to end when the whole content has been downloaded or the
   * user decides to switch channel, whichever happens first
   */
  SimTime userViewEta, downloadEta, oldEta(flow->getEta());
  SimTime now = scheduler->getSimTime();
  // time at which the user is going to change channel
  userViewEta = flow->getStart() +
          std::floor((flow->getSizeRequested() / this->bitrate) + 0.5);
  // this is to prevent problems with flows carried over from previous rounds
  if (flow->getStart() > now && userViewEta > scheduler->getRoundDuration())
    userViewEta = userViewEta % scheduler->getRoundDuration();
  downloadEta = now + std::floor(((flow->getContent()->getSize() - flow->getSizeDownloaded())
          / flow->getBandwidth()) + 0.5);
  flow->setEta(std::min(userViewEta, downloadEta));
  // Ensure that at least 1 second of flow is simulated
  if (flow->getEta() == flow->getStart())
    flow->setEta(flow->getStart() + 1);
 /*
  std::cout << now << ": oldEta: " << oldEta << " newEta: " << flow->getEta()
                << " sizeReq: " << flow->getSizeRequested() << " sizeDow: "
                << flow->getSizeDownloaded() << " bw: " << flow->getBandwidth()
                << " downloadEta: " << downloadEta << " userViewEta: "
                << userViewEta << " content: " << flow->getContent()->getName()
                << std::endl;
  */
  assert(flow->getEta() >= now);
  scheduler->updateSchedule(flow, oldEta);
}

/* Prints a snapshot of the network topology in graphml format, so that it can
 * be visualized through third party tools such as Gephi
 */
void Topology::printTopology(SimTime time, uint round) {
  std::ofstream outG;
  std::string index = boost::lexical_cast<std::string, uint>(round) + "_" +
          boost::lexical_cast<std::string, uint>(time);
  std::string filename = "./topologies/out_" + index + ".graphml";
  outG.open(filename);
  if (!outG.is_open()) {
    BOOST_LOG_TRIVIAL(error) << "could not open output graph file " << filename;
    abort();
  }  
  boost::write_graphml(outG, topology, snapDp);
  outG.close();  
}