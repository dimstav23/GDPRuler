#!/bin/sh

# Passthrough controller + bare metal DB server
./passthrough_self_hosted_db.sh --encryption OFF --logging OFF
# ./passthrough_self_hosted_db.sh --encryption ON --logging ON

# CVM GDPRuler + bare metal DB server
./gdpr_self_hosted_db.sh --encryption OFF --logging OFF
# ./gdpr_self_hosted_db.sh --encryption ON --logging OFF

# CVM GDPRuler + VM DB server
./gdpr_cloud_hosted_db.sh --encryption OFF --logging OFF
# ./gdpr_cloud_hosted_db.sh --encryption ON --logging OFF

# CVM GDPRuler + CVM DB server
./gdpr_confidential_cloud_hosted_db.sh --encryption OFF --logging OFF
# ./gdpr_confidential_cloud_hosted_db.sh --encryption ON --logging OFF
