#!/bin/bash

### get the script's directory
SCRIPT=$(readlink -f $0)
SCRIPT_DIR=`dirname $SCRIPT`

### build the project
pushd $PWD
cd ${SCRIPT_DIR}/GDPRbench/src
mvn clean package
popd

### set and create the traces folder if it doesn't exist
TRACE_FOLDER=${SCRIPT_DIR}/../workload_traces
mkdir -p ${TRACE_FOLDER}
echo "Trace file directory: ${TRACE_FOLDER}" 

### go to YCSB directory
pushd $PWD
cd ${SCRIPT_DIR}/GDPRbench/src

########
# For more information on the sequence of execution phases for the following workloads
# see here: https://github.com/brianfrankcooper/YCSB/wiki/Core-Workloads#running-the-workloads
# Note: We perform a separate load phase for each workload to have autonomous workload traces
# Load A Run A, Load A Run B, Load A Run C, Load A Run F, Load A Run D, Load E Run E
# For GDPR workloads we load and run the workloads as described
# here: https://github.com/GDPRbench/GDPRbench#benchmarking
########

### workload monitor 0%
echo "Generating trace for workload workload_monitor_0 in ${TRACE_FOLDER}/workload_monitor_0"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workload_monitor_0
# set the trace path in the workload config: workload_monitor_0 for load, workload_monitor_0 for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_0|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_0 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_0
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_0|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_0 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_0

### workload monitor 10%
echo "Generating trace for workload workload_monitor_10 in ${TRACE_FOLDER}/workload_monitor_10"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workload_monitor_10
# set the trace path in the workload config: workload_monitor_10 for load, workload_monitor_10 for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_10|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_10 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_10
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_10|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_10 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_10

### workload monitor 20%
echo "Generating trace for workload workload_monitor_20 in ${TRACE_FOLDER}/workload_monitor_20"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workload_monitor_20
# set the trace path in the workload config: workload_monitor_20 for load, workload_monitor_20 for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_20|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_20 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_20
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_20|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_20 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_20

### workload monitor 50%
echo "Generating trace for workload workload_monitor_50 in ${TRACE_FOLDER}/workload_monitor_50"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workload_monitor_50
# set the trace path in the workload config: workload_monitor_50 for load, workload_monitor_50 for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_50|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_50 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_50
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_50|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_50 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_50

### workload monitor 100%
echo "Generating trace for workload workload_monitor_100 in ${TRACE_FOLDER}/workload_monitor_100"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workload_monitor_100
# set the trace path in the workload config: workload_monitor_100 for load, workload_monitor_100 for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_100|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_100 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_100
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_100|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_100 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_100

### workload monitor vanilla (has the same configs with monitor workloads but does not contain any metadata)
echo "Generating trace for workload workload_monitor_vanilla in ${TRACE_FOLDER}/workload_monitor_vanilla"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workload_monitor_vanilla
# set the trace path in the workload config: workload_monitor_vanilla for load, workload_monitor_vanilla for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_vanilla|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_vanilla 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_vanilla
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workload_monitor_vanilla|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_vanilla 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workload_monitor_vanilla

### workload A
echo "Generating trace for workload A in ${TRACE_FOLDER}/workloada"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloada
# set the trace path in the workload config: workloada for load, workloada for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada

### workload B
echo "Generating trace for workload B in ${TRACE_FOLDER}/workloadb"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadb
# set the trace path in the workload config: workloada for load, workloadb for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadb|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadb|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadb 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadb

### workload C
echo "Generating trace for workload C in ${TRACE_FOLDER}/workloadc"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadc
# set the trace path in the workload config: workloada for load, workloadc for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadc|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadc|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadc
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadc

### workload F
echo "Generating trace for workload F in ${TRACE_FOLDER}/workloadf"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadf
# set the trace path in the workload config: workloada for load, workloadf for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadf|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadf|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadf 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadf

### workload D
echo "Generating trace for workload D in ${TRACE_FOLDER}/workloadd"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadd
# set the trace path in the workload config: workloada for load, workloadd for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadd|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadd|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadd
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadd

### workload E
echo "Generating trace for workload E in ${TRACE_FOLDER}/workloade"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloade
# set the trace path in the workload config: workloade for load, workloade for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloade|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloade|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade

### workload GDPR Controller
echo "Generating trace for workload GDPR Controller in ${TRACE_FOLDER}/gdpr_controller"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/gdpr_controller
# set the trace path in the workload config: gdpr_controller for load, gdpr_controller for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/gdpr_controller|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_controller 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_controller
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/gdpr_controller|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_controller 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_controller

### workload GDPR Processor
echo "Generating trace for workload GDPR Processor in ${TRACE_FOLDER}/gdpr_processor"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/gdpr_processor
# set the trace path in the workload config: gdpr_processor for load, gdpr_processor for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/gdpr_processor|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_processor 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_processor
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/gdpr_processor|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_processor 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_processor

### workload GDPR Customer
echo "Generating trace for workload GDPR Customer in ${TRACE_FOLDER}/gdpr_customer"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/gdpr_customer
# set the trace path in the workload config: gdpr_customer for load, gdpr_customer for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/gdpr_customer|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_customer 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_customer
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/gdpr_customer|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_customer 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/gdpr_customer 

# revert tracefile for workloada to avoid confusion
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada 

### 1M ops workloads for reasonable running times ###

### workload A 1M
echo "Generating trace for 1M workload A in ${TRACE_FOLDER}/workloada_1M"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloada_1M
# set the trace path in the workload config: workloada_1M for load, workloada_1M for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M

### workload B test
echo "Generating trace for 1M workload B in ${TRACE_FOLDER}/workloadb_1M"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadb_1M
# set the trace path in the workload config: workloada_1M for load, workloadb_1M for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadb_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadb_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadb_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadb_1M

### workload C test
echo "Generating trace for 1M workload C in ${TRACE_FOLDER}/workloadc_1M"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadc_1M
# set the trace path in the workload config: workloada_1M for load, workloadc_1M for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadc_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadc_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadc_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadc_1M

### workload F test
echo "Generating trace for 1M workload F in ${TRACE_FOLDER}/workloadf_1M"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadf_1M
# set the trace path in the workload config: workloada_1M for load, workloadf_1M for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadf_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadf_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadf_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadf_1M

### workload D test
echo "Generating trace for 1M workload D in ${TRACE_FOLDER}/workloadd_1M"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadd_1M
# set the trace path in the workload config: workloada_1M for load, workloadd_1M for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadd_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadd_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadd_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadd_1M

# ### workload E test
echo "Generating trace for 1M workload E in ${TRACE_FOLDER}/workloade_1M"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloade_1M
# set the trace path in the workload config: workloade_1M for load, workloade_1M for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloade_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_1M
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloade_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_1M
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_1M

# revert tracefile for workloada_1M to avoid confusion
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada_1M|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_1M

### TEST workloads ###
# Minimal workloads, mainly used for testing

### workload A test
echo "Generating trace for TEST workload A in ${TRACE_FOLDER}/workloada_test"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloada_test
# set the trace path in the workload config: workloada_test for load, workloada_test for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test

### workload B test
echo "Generating trace for TEST workload B in ${TRACE_FOLDER}/workloadb_test"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadb_test
# set the trace path in the workload config: workloada_test for load, workloadb_test for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadb_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadb_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadb_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadb_test

### workload C test
echo "Generating trace for TEST workload C in ${TRACE_FOLDER}/workloadc_test"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadc_test
# set the trace path in the workload config: workloada_test for load, workloadc_test for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadc_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadc_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadc_test
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadc_test

### workload F test
echo "Generating trace for TEST workload F in ${TRACE_FOLDER}/workloadf_test"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadf_test
# set the trace path in the workload config: workloada_test for load, workloadf_test for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadf_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadf_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadf_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadf_test

### workload D test
echo "Generating trace for TEST workload D in ${TRACE_FOLDER}/workloadd_test"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloadd_test
# set the trace path in the workload config: workloada_test for load, workloadd_test for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadd_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloadd_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadd_test
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloadd_test

# ### workload E test
echo "Generating trace for TEST workload E in ${TRACE_FOLDER}/workloade_test"
# remove trace file, if it exists
rm -f ${TRACE_FOLDER}/workloade_test
# set the trace path in the workload config: workloade_test for load, workloade_test for run
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloade_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_test 
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_test
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloade_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_test
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run tracer -s -P  ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloade_test

# revert tracefile for workloada_test to avoid confusion
sed -i "s|^tracer.file=.*|tracer.file=${TRACE_FOLDER}/workloada_test|g" ${SCRIPT_DIR}/GDPRbench/src/tracer_workloads/workloada_test

popd


#######
# Notes:
# 1) in YCSB core workloads, keys are prefixed with the string "user" (https://github.com/GDPRbench/GDPRbench/blob/94d399c099de9819b493fd5e4b553b54376dcead/src/core/src/main/java/com/yahoo/ycsb/workloads/CoreWorkload.java#L526)
# 2) in GDPR core workloads, keys are prefixed with the string "key" (https://github.com/GDPRbench/GDPRbench/blob/94d399c099de9819b493fd5e4b553b54376dcead/src/core/src/main/java/com/yahoo/ycsb/workloads/GDPRWorkload.java#L647)
# 3) the Core workload values are the fields generated concatenated into a String (default: 10 fields, 100 bytes each -> 1000bytes)
#    while the GDPR workload values are set via the `fieldlength` parameter in the workload file
# 4) for the Update operation in the Core workloads, we set the `writeallfields` to update all the fields of the value 
# 5) Removed DEC and ACL metadata from GDPRBench since they are implied in the rest (respective count variables are removed from the workload files)
# 6) Converted CAT to LOG metadata for monitoring with true/false values (respective count variables are removed from the workload files)
# 7) Reduced the default number of fieldcounts to 8 from 10.
# 8) `mockvalues` parameter in the workload configuration instructs the tracer not to log the random values but place a `VAL` placeholder
# 9) verifyTTL query removed from GDPR workload / should be done after the development of the controller to verify the correctness
# 10) getLogs() query specifies the amount of records that need to be audited / will be enhanced in our framework, just keep it to know when to call it
# 11) UPDATEMETAPURPOSE PUTM requests, should also be accompanied with the USR ID that requests the modification to see where he has access to (when the query filter is the purpose)
# 12) SHR field contains the userID with whom the data is shared
# 13) OBJ field contains the objected purposes of use for this specific KV pair
#######

######
# GDPRuler metadata associated with GDPRBench metadata fields:
# TTL -> TTL
# PUR -> PUR
# ORIG -> SRC
# SHARE -> SHR
# OBJECTIONS -> OBJ
# MONITOR -> CAT(modified)
# USR = OWNER -> USR

# DEC Unused -> 
# ACL Unused -> implied by the SHR and USR
# CAT Modified -> Converted to LOG (monitor)

# ORIG -> Immutable (cannot be updated in PUTM)
# USR -> Immutable (cannot be updated in PUTM)
######
