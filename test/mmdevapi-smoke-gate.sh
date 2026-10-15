#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mmdevapi spatial audio and winealsa device position (patches/sg/4005,
# 4006), 64- and 32-bit: an unknown spatial stream IID is not available,
# IAudioClock2::GetDevicePosition answers. Skips what needs a sound device.
#
#   WINE=/opt/wine-sg/bin/wine test/mmdevapi-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke mmdevapi
