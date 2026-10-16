#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# devenum (patches/sg/4003), 64- and 32-bit: a filter moniker is not
# dirty, has a size, saves and loads itself, Enum gives no sub-monikers,
# and the moniker enumerator clones.
#
#   WINE=/opt/wine-sg/bin/wine test/devenum-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke devenum
