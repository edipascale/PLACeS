PLACeS
======

Peer-to-peer Locality-Aware Content dElivery Simulator

Copyright 2012-2015 Emanuele Di Pascale
CONNECT (formerly CTVR) - Trinity College Dublin

See LICENSE.txt for details on licensing.

PLACeS is used to compare various content delivery strategies for
Video on Demand and time-shifted IPTV over a LRPON-based network.
It was written for academic purposes and it's most likely
unfit for usage in any production environment. 

The project requires Boost libraries version 1.58.0.
The latest updates also require the ILOCPLEX libraries from IBM's CPLEX.
PLACeS was tested only on Ubuntu and Debian, both 32 and 64 bit.

The source is currently being released mainly as a proof of concept;
minimal documentation is provided in the form of comments throughout 
the code. 

For instructions on which simulation parameters are available and how
to configure them, run the program with the --help command line argument.

==========
What's new
==========

We moved from a download model to a streaming model, where video items
are divided into fixed-size chunks and only enough chunks to fill a buffer
are fetched at each user. The downside of this approach is that, naturally,
simulations are much more cumbersome computationally. Chunking can be
disabled by setting the chunk-size parameter ('n') to 0. Note however
that this will disable the modelling of zapping behaviour, i.e., a change
of channel before the end of the current content item. Users who are 
interested in modelling such behaviours with no chunk division should
keep using the release version 1.0.