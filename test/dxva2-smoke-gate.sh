#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dxva2 monitor configuration (patches/sg/4002), 64- and 32-bit: physical
# monitors of the primary monitor are handed out and destroyed; DDC/CI
# calls on them and on a bad handle fail with an error code.
#
#   WINE=/opt/wine-sg/bin/wine test/dxva2-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke dxva2
