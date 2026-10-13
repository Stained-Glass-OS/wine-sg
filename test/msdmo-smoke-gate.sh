#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msdmo (patches/sg/4018), 64- and 32-bit: DMOGuidToStr A/W and
# DMOStrToGuidW round trip a GUID in registry key form.
#
#   WINE=/opt/wine-sg/bin/wine test/msdmo-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke msdmo
