#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mf (patches/sg/4010), 64- and 32-bit: transcode profile attributes,
# MFCreatePMPMediaSession, MFCreateWMAEncoderActivate.
#
#   WINE=/opt/wine-sg/bin/wine test/mf-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mf
