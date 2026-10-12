#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# qasf (patches/sg/4004), 64- and 32-bit: the WM ASF reader's class
# factory counts LockServer and creates the filter.
#
#   WINE=/opt/wine-sg/bin/wine test/qasf-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke qasf
