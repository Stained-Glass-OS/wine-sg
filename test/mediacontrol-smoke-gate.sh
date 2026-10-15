#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.media.mediacontrol (patches/sg/4008), 64- and 32-bit: the
# display updater of a window's transport controls gives video properties
# that keep their title.
#
#   WINE=/opt/wine-sg/bin/wine test/mediacontrol-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mediacontrol
