#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mciavi32 (patches/sg/4023), 64- and 32-bit: an AVI takes setaudio
# volume, setvideo off, realize, cue to a frame and signal.
#
#   WINE=/opt/wine-sg/bin/wine test/mciavi32-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mciavi32
