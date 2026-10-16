#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mciqtz32 (patches/sg/4022), 64- and 32-bit: a WAV opened as mpegvideo
# answers tracks and media present, takes speed, seek to end, door,
# audio volume and info.
#
#   WINE=/opt/wine-sg/bin/wine test/mciqtz32-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mciqtz32
