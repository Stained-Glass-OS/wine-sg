#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# avifil32 (patches/sg/4029, 4031), 64- and 32-bit: IAVIStream::SetInfo of
# a stream of an AVI opened for writing changes its name and language and
# counts the edit, refuses a short structure and a read-only file; the file
# (written without header room) is saved with the longer header by moving
# its frames, which still read back, and a frame appended to it lands after
# them. Run twice on the same file by the 64- then 32-bit probe.
#
#   WINE=/opt/wine-sg/bin/wine test/avifil32-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke avifil32
