#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.gaming.ui.gamebar (patches/sg/4007), 64- and 32-bit: the
# GameBar activation factory names its class and trust level.
#
#   WINE=/opt/wine-sg/bin/wine test/gamebar-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke gamebar
