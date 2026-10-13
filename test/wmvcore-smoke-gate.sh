#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wmvcore (patches/sg/4017), 64- and 32-bit: WMValidateData accepts an ASF
# header and refuses junk, WMIsAvailableOffline reports a local file.
#
#   WINE=/opt/wine-sg/bin/wine test/wmvcore-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke wmvcore
