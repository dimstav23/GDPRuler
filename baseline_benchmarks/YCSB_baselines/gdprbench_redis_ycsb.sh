#!/bin/bash

### get the script's directory
SCRIPT=$(readlink -f $0)
SCRIPT_DIR=`dirname $SCRIPT`

pushd $PWD
cd ${SCRIPT_DIR}
### get the GDPRbench framework if it does not exist
if [ ! -d "GDPRbench" ]; then
    git clone https://github.com/GDPRbench/GDPRbench.git
fi

# Reset the repository to its original state
cd GDPRbench
git reset --hard origin/master

### apply the patch file
git apply ${SCRIPT_DIR}/gdprbench_workloads.patch

### build GDPRbench
cd src
mvn clean package
popd

pushd $PWD
cd ${SCRIPT_DIR}
### get the redis gdpr compliance patch for GDPRbench
if [ ! -f "gdpr-compliance.diff" ]; then
    wget https://raw.githubusercontent.com/GDPRbench/redis-gdpr/master/src/gdpr-compliance.diff
fi
popd

pushd $PWD
cd ${SCRIPT_DIR}
### get redis and checkout the appopriate (5.0.4) version
if [ ! -d "redis" ]; then
    git clone https://github.com/redis/redis.git
fi

# Reset the repository to its original state
cd redis
git reset --hard origin/unstable

### checkout the appropriate version and apply the patch files
git checkout 5.0.4
git apply ${SCRIPT_DIR}/SDS.patch
git apply ${SCRIPT_DIR}/gdpr-compliance.diff

### build redis
make -j$(nproc)
popd

### set and create the traces folder if it doesn't exist
RES_DIR=${SCRIPT_DIR}/results
mkdir -p ${RES_DIR}

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

### setup redis parameters
REDIS_SERVER=${SCRIPT_DIR}/redis/src/redis-server
REDIS_PORT=6379
LOG_DIR="/scratch/dimitrios/gdprbench_dumps"
mkdir -p ${LOG_DIR}

### workload A
echo "Running workload A"
rm -f ${RES_DIR}/gdprbench_workloada_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloada | grep OVERALL | tee ${RES_DIR}/gdprbench_workloada_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload B
echo "Running workload B"
rm -f ${RES_DIR}/gdprbench_workloadb_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloadb | grep OVERALL | tee ${RES_DIR}/gdprbench_workloadb_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload C
echo "Running workload C"
rm -f ${RES_DIR}/gdprbench_workloadc_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloadc | grep OVERALL | tee ${RES_DIR}/gdprbench_workloadc_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload D
echo "Running workload D"
rm -f ${RES_DIR}/gdprbench_workloadd_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloadd | grep OVERALL | tee ${RES_DIR}/gdprbench_workloadd_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload E
# echo "Running workload E"
# rm -f ${RES_DIR}/gdprbench_workloade_res
# ${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
# sleep 1
# python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloade
# python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloade | grep OVERALL | tee ${RES_DIR}/gdprbench_workloade_res
# kill $(pgrep -f redis-server)
# sleep 5
# rm -rf ${LOG_DIR}/dump.rdb

### workload F
echo "Running workload F"
rm -f ${RES_DIR}/gdprbench_workloadf_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloada
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/workloadf | grep OVERALL | tee ${RES_DIR}/gdprbench_workloadf_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload gdpr_controller
echo "Running workload gdpr_controller"
rm -f ${RES_DIR}/gdprbench_gdpr_controller_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/gdpr_controller
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/gdpr_controller | grep OVERALL | tee ${RES_DIR}/gdprbench_gdpr_controller_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload gdpr_customer
echo "Running workload gdpr_customer"
rm -f ${RES_DIR}/gdprbench_gdpr_customer_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/gdpr_customer
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/gdpr_customer | grep OVERALL | tee ${RES_DIR}/gdprbench_gdpr_customer_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload gdpr_processor
echo "Running workload gdpr_processor"
rm -f ${RES_DIR}/gdprbench_gdpr_processor_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/gdpr_processor
python2 ${SCRIPT_DIR}/GDPRbench/src/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/GDPRbench/src/workloads/gdpr_processor | grep OVERALL | tee ${RES_DIR}/gdprbench_gdpr_processor_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

popd
