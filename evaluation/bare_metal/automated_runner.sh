#!/bin/sh

# direct execution
./direct.sh --encryption OFF --logging OFF

# native controller execution
./passthrough.sh --encryption OFF --logging OFF
# ./passthrough.sh --encryption OFF --logging ON
# ./passthrough.sh --encryption ON --logging OFF
# ./passthrough.sh --encryption ON --logging ON

# gdpr controller execution
./gdpr.sh --encryption OFF --logging OFF
# ./gdpr.sh --encryption OFF --logging ON
# ./gdpr.sh --encryption ON --logging OFF
# ./gdpr.sh --encryption ON --logging ON
