#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectSound effect DMOs (patches/sg/4000), 64- and 32-bit: the gargle
# DMO takes a parameter through IMediaParams, refuses one out of range,
# and modulates PCM in place through IMediaObjectInPlace::Process.
#
#   WINE=/opt/wine-sg/bin/wine test/dsdmo-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke dsdmo
