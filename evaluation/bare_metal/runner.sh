#!/bin/sh
./run_query_mgmt.sh --encryption OFF --logging OFF
./run_query_mgmt.sh --encryption OFF --logging ON
./run_query_mgmt.sh --encryption ON --logging OFF
./run_query_mgmt.sh --encryption ON --logging ON
