#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/src/ContentElement.o \
	${OBJECTDIR}/src/Flow.o \
	${OBJECTDIR}/src/IPTVTopologyOracle.o \
	${OBJECTDIR}/src/PLACeS.o \
	${OBJECTDIR}/src/Scheduler.o \
	${OBJECTDIR}/src/SimTimeInterval.o \
	${OBJECTDIR}/src/Topology.o \
	${OBJECTDIR}/src/TopologyOracle.o \
	${OBJECTDIR}/src/UGCPopularity.o \
	${OBJECTDIR}/src/VoDTopologyOracle.o


# C Compiler Flags
CFLAGS=`cppunit-config --cflags` 

# CC Compiler Flags
CCFLAGS=`cppunit-config --cflags` 
CXXFLAGS=`cppunit-config --cflags` 

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-L/usr/local/boost_1_58_0/stage/lib -L/home/dipascae/ibm/CPLEX_Studio126/concert/lib/x86-64_linux/static_pic -L/home/dipascae/ibm/CPLEX_Studio126/cplex/lib/x86-64_linux/static_pic -lboost_log_setup -lboost_log -lboost_program_options -lboost_date_time -lboost_filesystem -lboost_system -lboost_thread -lboost_graph -lcppunit -lilocplex -lcplex -lconcert -lcplexdistmip -lpthread `cppunit-config --libs`  

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places ${OBJECTFILES} ${LDLIBSOPTIONS} -lrt -static

${OBJECTDIR}/src/ContentElement.o: src/ContentElement.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/ContentElement.o src/ContentElement.cpp

${OBJECTDIR}/src/Flow.o: src/Flow.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Flow.o src/Flow.cpp

${OBJECTDIR}/src/IPTVTopologyOracle.o: src/IPTVTopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/IPTVTopologyOracle.o src/IPTVTopologyOracle.cpp

${OBJECTDIR}/src/PLACeS.o: src/PLACeS.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/PLACeS.o src/PLACeS.cpp

${OBJECTDIR}/src/Scheduler.o: src/Scheduler.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Scheduler.o src/Scheduler.cpp

${OBJECTDIR}/src/SimTimeInterval.o: src/SimTimeInterval.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/SimTimeInterval.o src/SimTimeInterval.cpp

${OBJECTDIR}/src/Topology.o: src/Topology.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/Topology.o src/Topology.cpp

${OBJECTDIR}/src/TopologyOracle.o: src/TopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/TopologyOracle.o src/TopologyOracle.cpp

${OBJECTDIR}/src/UGCPopularity.o: src/UGCPopularity.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/UGCPopularity.o src/UGCPopularity.cpp

${OBJECTDIR}/src/VoDTopologyOracle.o: src/VoDTopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.cc) -g -I/usr/local/boost_1_58_0 -I. -I/home/dipascae/ibm/CPLEX_Studio126/cplex/include -I/home/dipascae/ibm/CPLEX_Studio126/concert/include -std=c++11 -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/VoDTopologyOracle.o src/VoDTopologyOracle.cpp

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
