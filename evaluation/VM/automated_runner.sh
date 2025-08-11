#!/bin/sh

repeats=5

for i in $(seq 1 "$repeats"); do
  echo "=== Iteration $i / $repeats ==="

  # -------------------------------
  # DB server in CVM
  # -------------------------------
  ./direct.sh --encryption OFF --logging OFF --server_connection TCP

  # -------------------------------
  # passthrough controller + DB server in CVM
  # -------------------------------
  # ./passthrough.sh --encryption OFF --logging OFF --server_connection UNIX
  # ./passthrough.sh --encryption ON --logging OFF --server_connection UNIX
  # ./passthrough.sh --encryption ON --logging ON

  # -------------------------------
  # GDPRuler + DB server in CVM
  # -------------------------------
  ./gdpr.sh --encryption OFF --logging OFF --server_connection UNIX
  ./gdpr.sh --encryption ON --logging OFF --server_connection UNIX
  # ./gdpr.sh --encryption ON --logging ON

  echo
done
