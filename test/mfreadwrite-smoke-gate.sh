#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mfreadwrite (patches/sg/4011), 64- and 32-bit: the source reader of a
# WAV file takes SetNativeMediaType and RemoveAllTransformsForStream.
#
#   WINE=/opt/wine-sg/bin/wine test/mfreadwrite-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mfreadwrite
