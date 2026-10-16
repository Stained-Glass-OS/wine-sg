#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# qdvd (patches/sg/4016), 64- and 32-bit: IDvdInfo2::GetDiscID of a
# DVD-Video folder is non-zero and stable, a missing disc fails.
#
#   WINE=/opt/wine-sg/bin/wine test/qdvd-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke qdvd
