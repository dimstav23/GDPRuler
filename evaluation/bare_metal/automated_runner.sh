#!/bin/sh

repeats=5   # how many times to run each test

for i in $(seq 1 "$repeats"); do
  echo "=== Iteration $i / $repeats ==="

  # -------------------------------
  # direct execution
  # -------------------------------
  ./direct.sh --encryption OFF --logging OFF --server_connection TCP

  # -------------------------------
  # passthrough controller execution
  # -------------------------------
  # ./passthrough.sh --encryption OFF --logging OFF --server_connection UNIX
  # ./passthrough.sh --encryption OFF --logging OFF --server_connection TCP
  # ./passthrough.sh --encryption OFF --logging ON
  # ./passthrough.sh --encryption ON  --logging OFF --server_connection UNIX
  # ./passthrough.sh --encryption ON  --logging OFF --server_connection TCP
  # ./passthrough.sh --encryption ON  --logging ON

  # -------------------------------
  # GDPR controller execution
  # -------------------------------
  ./gdpr.sh --encryption OFF --logging OFF --server_connection UNIX
  # ./gdpr.sh --encryption OFF --logging OFF --server_connection TCP
  # ./gdpr.sh --encryption OFF --logging ON
  ./gdpr.sh --encryption ON  --logging OFF --server_connection UNIX
  # ./gdpr.sh --encryption ON  --logging OFF --server_connection TCP
  # ./gdpr.sh --encryption ON  --logging ON

  echo
done
