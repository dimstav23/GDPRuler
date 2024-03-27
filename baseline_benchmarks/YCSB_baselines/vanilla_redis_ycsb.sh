#!/bin/bash

### get the script's directory
SCRIPT=$(readlink -f $0)
SCRIPT_DIR=`dirname $SCRIPT`

pushd $PWD
cd ${SCRIPT_DIR}
### get the GDPRBench framework if it does not exist
if [ ! -d "YCSB" ]; then
    git clone https://github.com/brianfrankcooper/YCSB.git
fi

# Reset the repository to its original state
cd YCSB
git reset --hard origin/master

### apply the patch file
git apply ${SCRIPT_DIR}/ycsb_workloads.patch

### build YCSB redis binding
mvn -pl site.ycsb:redis-binding -am clean package
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

### checkout the appropriate version and apply the patch file
git checkout 5.0.4
git apply ${SCRIPT_DIR}/SDS.patch

### build redis
make -j$(nproc)
popd

### set and create the traces folder if it doesn't exist
RES_DIR=${SCRIPT_DIR}/results
mkdir -p ${RES_DIR}

### go to YCSB directory
pushd $PWD
cd ${SCRIPT_DIR}/YCSB

########
# For more information on the sequence of execution phases for the following workloads
# see here: https://github.com/brianfrankcooper/YCSB/wiki/Core-Workloads#running-the-workloads
# Note: We perform a separate load phase for each workload to have autonomous workload traces
# Load A Run A, Load A Run B, Load A Run C, Load A Run F, Load A Run D, Load E Run E
########
### setup redis parameters
REDIS_SERVER=${SCRIPT_DIR}/redis/src/redis-server
REDIS_PORT=6379
LOG_DIR="/scratch/dimitrios/ycsb_dumps"
mkdir -p ${LOG_DIR}

### workload A
echo "Running workload A"
rm -f ${RES_DIR}/ycsb_workloada_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloada
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloada | grep OVERALL | tee ${RES_DIR}/ycsb_workloada_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload B
echo "Running workload B"
rm -f ${RES_DIR}/ycsb_workloadb_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloada
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloadb | grep OVERALL | tee ${RES_DIR}/ycsb_workloadb_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload C
echo "Running workload C"
rm -f ${RES_DIR}/ycsb_workloadc_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloada
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloadc | grep OVERALL | tee ${RES_DIR}/ycsb_workloadc_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload D
echo "Running workload D"
rm -f ${RES_DIR}/ycsb_workloadd_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloada
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloadd | grep OVERALL | tee ${RES_DIR}/ycsb_workloadd_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

### workload E
# echo "Running workload E"
# rm -f ${RES_DIR}/ycsb_workloade_res
# ${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
# sleep 1
# python2 ${SCRIPT_DIR}/YCSB/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloade
# python2 ${SCRIPT_DIR}/YCSB/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloade | grep OVERALL | tee ${RES_DIR}/ycsb_workloade_res
# kill $(pgrep -f redis-server)
# sleep 5
# rm -rf ${LOG_DIR}/dump.rdb

### workload F
echo "Running workload F"
rm -f ${RES_DIR}/ycsb_workloadf_res
${REDIS_SERVER} --port ${REDIS_PORT} --dir ${LOG_DIR} --protected-mode no &
sleep 1
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb load redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloada
python2 ${SCRIPT_DIR}/YCSB/bin/ycsb run redis -s -p redis.host=localhost -p redis.port=${REDIS_PORT} -P  ${SCRIPT_DIR}/YCSB/workloads/workloadf | grep OVERALL | tee ${RES_DIR}/ycsb_workloadf_res
kill $(pgrep -f redis-server)
sleep 5
rm -rf ${LOG_DIR}/dump.rdb

popd
