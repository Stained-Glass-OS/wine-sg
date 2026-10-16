#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# amstream (patches/sg/4028), 64- and 32-bit: an audio stream sample takes
# SSUPDATE_CONTINUOUS (a stopped stream answers MS_E_NOTRUNNING, not
# E_NOTIMPL); unknown flags are still refused.
#
#   WINE=/opt/wine-sg/bin/wine test/amstream-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke amstream
