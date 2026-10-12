#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# qcap (patches/sg/4001), 64- and 32-bit: ICaptureGraphBuilder2::
# SetOutputFileName builds an AVI mux and file writer; IConfigAviMux keeps
# the master stream and the old index setting.
#
#   WINE=/opt/wine-sg/bin/wine test/qcap-smoke-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
. "$(dirname "$0")/media-smoke.sh"
media_smoke qcap
