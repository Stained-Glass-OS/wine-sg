#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Small multimedia fixes (patches/sg/4012-4015, 4019-4021, 4024-4026),
# 64- and 32-bit: the DLLs load, the new exports are there (xinputuap,
# mfmediaengine, dpwsockx), and mciwave saves an opened file under a new
# name with wait, keeping the original.
#
#   WINE=/opt/wine-sg/bin/wine test/media-misc-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke misc
