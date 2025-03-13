#!/bin/sh

# direct execution
./native_direct.sh --encryption OFF --logging OFF

# native controller execution
./passthrough.sh --encryption OFF --logging OFF
# ./passthrough.sh --encryption OFF --logging ON
# ./passthrough.sh --encryption ON --logging OFF
# ./passthrough.sh --encryption ON --logging ON

# gdpr controller execution
./gdpr_ctl.sh --encryption OFF --logging OFF
# ./gdpr_ctl.sh --encryption OFF --logging ON
# ./gdpr_ctl.sh --encryption ON --logging OFF
# ./gdpr_ctl.sh --encryption ON --logging ON
