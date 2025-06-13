#!/bin/sh

# DB server in CVM
./direct.sh

# passthrough controller + DB server in CVM
./passthrough.sh --encryption OFF --logging OFF
# ./passthrough.sh --encryption ON --logging OFF
# ./passthrough.sh --encryption ON --logging ON

# GDPRuler + DB server in CVM
./gdpr.sh --encryption OFF --logging OFF
# ./gdpr.sh --encryption ON --logging OFF
# ./gdpr.sh --encryption ON --logging ON