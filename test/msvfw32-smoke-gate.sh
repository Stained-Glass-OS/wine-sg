#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msvfw32 (patches/sg/4030), 64- and 32-bit: MCIWNDM_NEW opens a new
# waveaudio element in an MCIWnd window, and again on the current device.
#
#   WINE=/opt/wine-sg/bin/wine test/msvfw32-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke msvfw32
