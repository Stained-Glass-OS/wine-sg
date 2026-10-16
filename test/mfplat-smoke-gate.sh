#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfplat (patches/sg/4009), 64- and 32-bit: an AM media type with a format
# block mfplat does not know converts to an MF type and back unchanged.
#
#   WINE=/opt/wine-sg/bin/wine test/mfplat-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mfplat
