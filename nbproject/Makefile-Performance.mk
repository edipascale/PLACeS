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
CND_CONF=Performance
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

# Test Directory
TESTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}/tests

# Test Files
TESTFILES= \
	${TESTDIR}/TestFiles/f1

# C Compiler Flags
CFLAGS=

# CC Compiler Flags
CCFLAGS=--std=c++11
CXXFLAGS=--std=c++11

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-L/usr/local/boost_1_53_0/stage/lib -lboost_log_setup -lboost_log -lboost_program_options -lboost_date_time -lboost_filesystem -lboost_system -lboost_thread -lboost_graph -lpthread -lcppunit

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.cc} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places ${OBJECTFILES} ${LDLIBSOPTIONS} -static -s

${OBJECTDIR}/src/ContentElement.o: src/ContentElement.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/ContentElement.o src/ContentElement.cpp

${OBJECTDIR}/src/Flow.o: src/Flow.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/Flow.o src/Flow.cpp

${OBJECTDIR}/src/IPTVTopologyOracle.o: src/IPTVTopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/IPTVTopologyOracle.o src/IPTVTopologyOracle.cpp

${OBJECTDIR}/src/PLACeS.o: src/PLACeS.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/PLACeS.o src/PLACeS.cpp

${OBJECTDIR}/src/Scheduler.o: src/Scheduler.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/Scheduler.o src/Scheduler.cpp

${OBJECTDIR}/src/SimTimeInterval.o: src/SimTimeInterval.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/SimTimeInterval.o src/SimTimeInterval.cpp

${OBJECTDIR}/src/Topology.o: src/Topology.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/Topology.o src/Topology.cpp

${OBJECTDIR}/src/TopologyOracle.o: src/TopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/TopologyOracle.o src/TopologyOracle.cpp

${OBJECTDIR}/src/UGCPopularity.o: src/UGCPopularity.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/UGCPopularity.o src/UGCPopularity.cpp

${OBJECTDIR}/src/VoDTopologyOracle.o: src/VoDTopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/VoDTopologyOracle.o src/VoDTopologyOracle.cpp

# Subprojects
.build-subprojects:

# Build Test Targets
.build-tests-conf: .build-conf ${TESTFILES}
${TESTDIR}/TestFiles/f1: ${TESTDIR}/tests/CacheTestClass.o ${TESTDIR}/tests/CacheTestRunner.o ${OBJECTFILES:%.o=%_nomain.o}
	${MKDIR} -p ${TESTDIR}/TestFiles
	${LINK.cc}   -o ${TESTDIR}/TestFiles/f1 $^ ${LDLIBSOPTIONS} `cppunit-config --libs` `cppunit-config --libs`   


${TESTDIR}/tests/CacheTestClass.o: tests/CacheTestClass.cpp 
	${MKDIR} -p ${TESTDIR}/tests
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 `cppunit-config --cflags` -MMD -MP -MF $@.d -o ${TESTDIR}/tests/CacheTestClass.o tests/CacheTestClass.cpp


${TESTDIR}/tests/CacheTestRunner.o: tests/CacheTestRunner.cpp 
	${MKDIR} -p ${TESTDIR}/tests
	${RM} $@.d
	$(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 `cppunit-config --cflags` -MMD -MP -MF $@.d -o ${TESTDIR}/tests/CacheTestRunner.o tests/CacheTestRunner.cpp


${OBJECTDIR}/src/ContentElement_nomain.o: ${OBJECTDIR}/src/ContentElement.o src/ContentElement.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/ContentElement.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/ContentElement_nomain.o src/ContentElement.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/ContentElement.o ${OBJECTDIR}/src/ContentElement_nomain.o;\
	fi

${OBJECTDIR}/src/Flow_nomain.o: ${OBJECTDIR}/src/Flow.o src/Flow.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/Flow.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/Flow_nomain.o src/Flow.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/Flow.o ${OBJECTDIR}/src/Flow_nomain.o;\
	fi

${OBJECTDIR}/src/IPTVTopologyOracle_nomain.o: ${OBJECTDIR}/src/IPTVTopologyOracle.o src/IPTVTopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/IPTVTopologyOracle.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/IPTVTopologyOracle_nomain.o src/IPTVTopologyOracle.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/IPTVTopologyOracle.o ${OBJECTDIR}/src/IPTVTopologyOracle_nomain.o;\
	fi

${OBJECTDIR}/src/PLACeS_nomain.o: ${OBJECTDIR}/src/PLACeS.o src/PLACeS.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/PLACeS.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/PLACeS_nomain.o src/PLACeS.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/PLACeS.o ${OBJECTDIR}/src/PLACeS_nomain.o;\
	fi

${OBJECTDIR}/src/Scheduler_nomain.o: ${OBJECTDIR}/src/Scheduler.o src/Scheduler.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/Scheduler.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/Scheduler_nomain.o src/Scheduler.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/Scheduler.o ${OBJECTDIR}/src/Scheduler_nomain.o;\
	fi

${OBJECTDIR}/src/SimTimeInterval_nomain.o: ${OBJECTDIR}/src/SimTimeInterval.o src/SimTimeInterval.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/SimTimeInterval.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/SimTimeInterval_nomain.o src/SimTimeInterval.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/SimTimeInterval.o ${OBJECTDIR}/src/SimTimeInterval_nomain.o;\
	fi

${OBJECTDIR}/src/Topology_nomain.o: ${OBJECTDIR}/src/Topology.o src/Topology.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/Topology.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/Topology_nomain.o src/Topology.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/Topology.o ${OBJECTDIR}/src/Topology_nomain.o;\
	fi

${OBJECTDIR}/src/TopologyOracle_nomain.o: ${OBJECTDIR}/src/TopologyOracle.o src/TopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/TopologyOracle.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/TopologyOracle_nomain.o src/TopologyOracle.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/TopologyOracle.o ${OBJECTDIR}/src/TopologyOracle_nomain.o;\
	fi

${OBJECTDIR}/src/UGCPopularity_nomain.o: ${OBJECTDIR}/src/UGCPopularity.o src/UGCPopularity.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/UGCPopularity.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/UGCPopularity_nomain.o src/UGCPopularity.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/UGCPopularity.o ${OBJECTDIR}/src/UGCPopularity_nomain.o;\
	fi

${OBJECTDIR}/src/VoDTopologyOracle_nomain.o: ${OBJECTDIR}/src/VoDTopologyOracle.o src/VoDTopologyOracle.cpp 
	${MKDIR} -p ${OBJECTDIR}/src
	@NMOUTPUT=`${NM} ${OBJECTDIR}/src/VoDTopologyOracle.o`; \
	if (echo "$$NMOUTPUT" | ${GREP} '|main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T main$$') || \
	   (echo "$$NMOUTPUT" | ${GREP} 'T _main$$'); \
	then  \
	    ${RM} $@.d;\
	    $(COMPILE.cc) -O2 -I. -I/usr/local/boost_1_53_0 -I. --std=c++11 -Dmain=__nomain -MMD -MP -MF $@.d -o ${OBJECTDIR}/src/VoDTopologyOracle_nomain.o src/VoDTopologyOracle.cpp;\
	else  \
	    ${CP} ${OBJECTDIR}/src/VoDTopologyOracle.o ${OBJECTDIR}/src/VoDTopologyOracle_nomain.o;\
	fi

# Run Test Targets
.test-conf:
	@if [ "${TEST}" = "" ]; \
	then  \
	    ${TESTDIR}/TestFiles/f1 || true; \
	else  \
	    ./${TEST} || true; \
	fi

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/places

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
