/*
 * File:   PLACeS.cpp
 * Author: emanuele
 *
 * Created on 27 April 2012, 11:17
 */

#include <cstdlib>
#include "IPTVTopologyOracle.hpp"
#include "VoDTopologyOracle.hpp"
#include "Scheduler.hpp"
#include "PLACeS.hpp"
#include <fstream>
#include <boost/random/mersenne_twister.hpp>
#include "Cache.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <numeric>
#include <algorithm>
#include <boost/log/core/core.hpp>
#include <boost/log/expressions.hpp>

// random generator (global because multiple classes need to access it)
boost::mt19937 gen;

/* Output collected statistics from a simulation into a file 
 */
void printToFile(po::variables_map vm, Topology* topo, TopologyOracle* oracle) {
  using namespace std;
  using namespace boost::posix_time;
  string topoFileName = topo->getFileName();
  string outFileName = vm["output"].as<string>();
  //create file handle for P2P locality stats output file
  ptime now = second_clock::universal_time();
  ofstream outputF;
  outputF.open(outFileName.c_str(), ios::out | ios::app);
  if (!outputF.is_open()) {
    cerr << "ERROR: PLACeS::main() - Could not open specified output "
            "file " << outFileName << endl;
    abort();
  }
  // recap of parameters
  outputF << "% " << to_simple_string(now)
          << " - Simulation on Topology: " << topoFileName << 
            " with seed: " << vm["seed"].as<unsigned long>() << endl;
  stringstream ss;
  ss << "-s " << vm["sim-mode"].as<uint > () << " -p " << vm["pon-cardinality"].as<uint > ()
          << " -c " << vm["contents"].as<uint > ()
          << " -C " << vm["channels"].as<uint>()
          << " -a " << vm["avg-hours-per-user"].as<double>()
          << " -b " << vm["bitrate"].as<uint>()
          << " -u " << vm["ucache-size"].as<uint>()
          << " -l " << vm["lcache-size"].as<uint>()
          << " -P " << vm["ucache-policy"].as<uint>()
          << " -L " << vm["content-length"].as<double>()
          << " -D " << vm["content-dev"].as<double>()
          << " -R " << vm["reduced-caching"].as<bool>()
          << " -m " << vm["min-flow-increase"].as<double>();
  outputF << "% Parameters: " << ss.str() << endl;
  uint rounds = vm["rounds"].as<uint>();
  NetworkStats stats = topo->getNetworkStats();
  // print network stats for each round
  outputF << "Rnd AvgTot AvgCore AvgMetro AvgUp AvgDown AvgPeakCore AvgPeakMetro AvgPeakUp AvgPeakDown "
          "PeakCore PeakMetro PeakUp PeakDown" << endl;
  for (uint i = 0; i < rounds; i++) {
    outputF << i << " " << stats.avgTot.at(i) << " "
          << stats.avgCore.at(i) << " "
          << stats.avgMetro.at(i) << " "
          << stats.avgAccessUp.at(i) << " "
          << stats.avgAccessDown.at(i) << " "
          << stats.avgPeakCore.at(i) << " "
          << stats.avgPeakMetro.at(i) << " "
          << stats.avgPeakAccessUp.at(i) << " "
          << stats.avgPeakAccessDown.at(i) << " "
          << stats.peakCore.at(i) << " " 
          << stats.peakMetro.at(i) << " " 
          << stats.peakAccessUp.at(i) << " " 
          << stats.peakAccessDown.at(i) << endl;
  }
  if (rounds > 1) {
    // Print the aggregate stats over all rounds
    outputF << "a " << (Capacity) accumulate(stats.avgTot.begin(), stats.avgTot.end(), 0.0) / rounds << " "
            << (Capacity) (accumulate(stats.avgCore.begin(), stats.avgCore.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgMetro.begin(), stats.avgMetro.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgAccessUp.begin(), stats.avgAccessUp.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgAccessDown.begin(), stats.avgAccessDown.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgPeakCore.begin(), stats.avgPeakCore.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgPeakMetro.begin(), stats.avgPeakMetro.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgPeakAccessUp.begin(), stats.avgPeakAccessUp.end(), 0.0) / rounds) << " "
            << (Capacity) (accumulate(stats.avgPeakAccessDown.begin(), stats.avgPeakAccessDown.end(), 0.0) / rounds) << " "
            << *max_element(stats.peakCore.begin(), stats.peakCore.end()) << " "
            << *max_element(stats.peakMetro.begin(), stats.peakMetro.end()) << " "
            << *max_element(stats.peakAccessUp.begin(), stats.peakAccessUp.end()) << " "
            << *max_element(stats.peakAccessDown.begin(), stats.peakAccessDown.end()) << " "
            << endl << endl;
  } else {
    outputF << endl;
  }
  // Print flow stats for each round
  FlowStats flowStats = oracle->getFlowStats();
  outputF << "Rnd Completed Served Local Local% P2P P2P% AS AS% CS CS% Blocked Blocked% "
          "AvgTime AvgP2PTime AvgASTime AvgUsrCache% AvgASCache%" << endl;
  std::vector<double> localPctg, ASPctg, P2PPctg, CSPctg, blockPctg;
  localPctg.assign(rounds, 0);
  ASPctg.assign(rounds, 0);
  P2PPctg.assign(rounds,0);
  CSPctg.assign(rounds, 0);
  blockPctg.assign(rounds, 0);
  for (uint i = 0; i < rounds; i++) {
    localPctg.at(i) = (flowStats.localRequests.at(i) /
            (double) flowStats.completedRequests.at(i)) * 100;
    ASPctg.at(i) = (flowStats.fromASCache.at(i) /
            (double) flowStats.completedRequests.at(i)) * 100;
    P2PPctg.at(i) = (flowStats.fromPeers.at(i) /
            (double) flowStats.completedRequests.at(i)) * 100;
    CSPctg.at(i) = (flowStats.fromCentralServer.at(i) /
            (double) flowStats.completedRequests.at(i)) * 100;
    blockPctg.at(i) = (flowStats.congestionBlocked.at(i) * 100) /
            (double) (flowStats.servedRequests.at(i) +
            flowStats.congestionBlocked.at(i));
    outputF << i << " " << flowStats.completedRequests.at(i) << " "
            << flowStats.servedRequests.at(i) << " "
            << flowStats.localRequests.at(i) << " " << localPctg.at(i) << " "
            << flowStats.fromPeers.at(i) << " " << P2PPctg.at(i) << " "
            << flowStats.fromASCache.at(i) << " " << ASPctg.at(i) << " "
            << flowStats.fromCentralServer.at(i) << " " << CSPctg.at(i) << " "
            << flowStats.congestionBlocked.at(i) << " " << blockPctg.at(i) << " "
            << flowStats.avgFlowDuration.at(i) << " "
            << flowStats.avgPeerFlowDuration.at(i) << " "
            << flowStats.avgCacheFlowDuration.at(i) << " "
            << flowStats.avgUserCacheOccupancy.at(i) << " "
            << flowStats.avgASCacheOccupancy.at(i) 
            << std::endl;
  }
  if (rounds > 1) {
    // print aggregate flow stats over all rounds
    outputF << "a " 
            << accumulate(flowStats.completedRequests.begin(), flowStats.completedRequests.end(), 0) << " "
            << accumulate(flowStats.servedRequests.begin(), flowStats.servedRequests.end(), 0) << " "
            << accumulate(flowStats.localRequests.begin(), flowStats.localRequests.end(), 0) << " "
            << (double) (accumulate(localPctg.begin(), localPctg.end(), 0.0) / rounds) << " "
            << accumulate(flowStats.fromPeers.begin(), flowStats.fromPeers.end(), 0) << " "
            << (double) (accumulate(P2PPctg.begin(), P2PPctg.end(), 0.0) / rounds) << " "
            << accumulate(flowStats.fromASCache.begin(), flowStats.fromASCache.end(), 0) << " "
            << (double) (accumulate(ASPctg.begin(), ASPctg.end(), 0.0) / rounds) << " "
            << accumulate(flowStats.fromCentralServer.begin(), flowStats.fromCentralServer.end(), 0) << " "
            << (double) (accumulate(CSPctg.begin(), CSPctg.end(), 0.0) / rounds) << " "
            << accumulate(flowStats.congestionBlocked.begin(), flowStats.congestionBlocked.end(), 0) << " "
            << (double) (accumulate(blockPctg.begin(), blockPctg.end(), 0.0) / rounds) << " "
            << (double) (accumulate(flowStats.avgFlowDuration.begin(), flowStats.avgFlowDuration.end(), 0.0) / rounds) << " "
            << (double) (accumulate(flowStats.avgPeerFlowDuration.begin(), flowStats.avgPeerFlowDuration.end(), 0.0) / rounds) << " "
            << (double) (accumulate(flowStats.avgCacheFlowDuration.begin(), flowStats.avgCacheFlowDuration.end(), 0.0) / rounds) << " "
            << (float) (accumulate(flowStats.avgUserCacheOccupancy.begin(), flowStats.avgUserCacheOccupancy.end(), 0.0) / rounds) << " "
            << (float) (accumulate(flowStats.avgASCacheOccupancy.begin(), flowStats.avgASCacheOccupancy.end(), 0.0) / rounds)
            << endl << endl;
  } else {
    outputF << endl;
  }
  outputF.close();
}

/*
 *
 */
int main(int argc, char** argv) {
  using namespace std;
  namespace logging = boost::log::v2s_mt_posix;
 
  std::vector<string> topoFileNameVec;

  string topoFileName = "topologies/germanTopoNew.txt";

  po::options_description clo("Command line options");
  clo.add_options()
          ("help", "show this help message")
          ("sim-mode,s", po::value<uint>()->default_value((SimMode)IPTV, "IPTV"),
              "simulation mode [0 for VoD, 1 for IPTV]")
          ("topology,t", po::value< vector<string> >(&topoFileNameVec)->composing(),
              "file name of an input topology")
          ("pon-cardinality,p", po::value<uint>()->default_value(0),
              "number of ONUs attached to each PON tree; if >0, overrides the "
              "values specified in the topology file")
          ("rounds,r", po::value<uint>()->default_value(7),
              "number of rounds [days for IPTV, weeks for VoD] to simulate")
          ("contents,c", po::value<uint>()->default_value(30),
              "# of contents generated daily by each channel [for IPTV] or "
              "# of contents in the catalog [for VoD]")
          ("channels,C", po::value<uint>()->default_value(100),
              "number of IPTV channels [IPTV only]")
          ("print-load,i", po::value<bool>()->default_value(true),
              "output average link load after each round")
          ("ucache-policy,P", po::value<uint>()->default_value((CachePolicy)LFU, "LFU"),
              "policy to enforce when replacing content in the user cache")
          ("ucache-size,u", po::value<uint>()->default_value(10),
              "size of the user cache in GB")
          ("lcache-size,l", po::value<uint>()->default_value(16384),
              "size of the local AS cache in GB")
          ("debug-verbose,d", po::value<uint>()->default_value(logging::trivial::warning),
              "minimal severity level displayed for the Boost.log filter")
          ("reduced-caching,R", po::value<bool>()->default_value(true),
              "Only use one CDN server for the network")
          ("avg-hours-per-user,a", po::value<double>()->default_value(5),
              "Average TV viewing hours per user per round")
          ("perturbations,e", po::value<bool>()->default_value(true),
              "Extend the popularity model with perturbations [VoD only]")
          ("bitrate,b", po::value<uint>()->default_value(3),
              "Average bitrate in Mbps of elements in the catalog")
          ("content-length,L", po::value<double>()->default_value(45),
              "Average length in minutes of elements in the catalog")
          ("content-dev,D", po::value<double>()->default_value(5),
              "Standard deviation of the content length")
          ("min-flow-increase,m", po::value<double>()->default_value(0),
              "Minimal bandwidth increase threshold to be applied to a flow "
              "(higher values will make for a faster but less accurate simulation)")
          ("seed,S", po::value<unsigned long>()->default_value(gen.default_seed),
              "Seed to be used with the pseudo-random generator")
          ("output,o", po::value<std::string>()->default_value("../results/out.txt"),
              "Name of the output file with the results")
          ("snapshot-freq,f", po::value<uint>()->default_value(0),
              "Frequency at which to take graphml snapshots of the network")
          ("pre-caching,M", po::value<bool>()->default_value(false),
              "if true stores most popular content in AS caches (reduced-caching must be false)")
          ;
  
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, clo), vm);
  po::notify(vm);
  
  if (vm.count("help")) {
    std::cout << clo << std::endl;
    return(0);
  }
  
  if (vm.count("topology") == 0) {
    topoFileNameVec.push_back(topoFileName);
  }
  //Initialize the internal state of the pseudo-random generator
  gen.seed(vm["seed"].as<unsigned long>());

  //initialize logging level
  logging::core::get()->set_filter
  ( 
    logging::trivial::severity >= (logging::trivial::severity_level) vm["debug-verbose"].as<uint>()
  );
  
    
  std::vector<Topology*> topoVector;
  BOOST_FOREACH (string fileName, topoFileNameVec) {
    Topology* topo = new Topology(fileName, vm);
    topoVector.push_back(topo);
  }

  uint roundDuration = 0;
  TopologyOracle* oracle = nullptr;
  
  // Start simulations
  BOOST_FOREACH(Topology* topo, topoVector) {
    std::cout << "Simulation on Topology: " << topo->getFileName() << std::endl;    
    // determine the oracle type  basing on the current simulation mode
    if ((SimMode) vm["sim-mode"].as<uint > () == IPTV) {
      roundDuration = 86400; // 1 day
      oracle = new IPTVTopologyOracle(topo, vm);
    } else {
      roundDuration = 604800; // 1 week
      oracle = new VoDTopologyOracle(topo, vm);
    }
    oracle->populateCatalog();
    if (vm["pre-caching"].as<bool>() && !vm["reduced-caching"].as<bool>())
      oracle->preCache();
    Scheduler* scheduler = new Scheduler(oracle, vm);    
    for (unsigned int currentRound = 0; currentRound < vm["rounds"].as<uint>(); currentRound++) {            
      std::cout << "Starting round " << currentRound << std::endl;
      oracle->generateUserViewMap(scheduler);
      // start the clock
      while (scheduler->advanceClock());
      // simulation over, print stats
      if (vm["print-load"].as<bool>()) {
        topo->printNetworkStats(currentRound, roundDuration);
      }
      oracle->printStats(currentRound);
      scheduler->startNewRound();
      oracle->notifyEndRound(currentRound);
      if (currentRound+1< vm["rounds"].as<uint>()) {
        oracle->updateCatalog(currentRound);
      }
    }
    // print stats to the putput file
    printToFile(vm, topo, oracle);
    delete scheduler;
    delete oracle;
  }  
  BOOST_FOREACH (Topology* topo, topoVector) {
    delete topo;
  }
  return 0;
}

