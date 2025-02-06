#!/bin/sh

# CVM GDPRuler + bare metal DB server
./self_hosted_db.sh --encryption OFF --logging OFF
./self_hosted_db.sh --encryption ON --logging OFF

# CVM GDPRuler + VM DB server
./cloud_hosted_db.sh --encryption OFF --logging OFF
./cloud_hosted_db.sh --encryption ON --logging OFF

# CVM GDPRuler + CVM DB server
./confidential_cloud_hosted_db.sh --encryption OFF --logging OFF
./confidential_cloud_hosted_db.sh --encryption ON --logging OFF
