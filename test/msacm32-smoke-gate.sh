#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msacm32 (patches/sg/4027), 64- and 32-bit: no hardware drivers counted,
# 16 to 24 bit PCM converts, acmStreamMessage refuses system messages and
# hands driver messages to the driver.
#
#   WINE=/opt/wine-sg/bin/wine test/msacm32-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke msacm32
