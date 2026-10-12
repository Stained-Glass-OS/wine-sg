#!/bin/sh
# dmime music value conversions, segment Compose, PlaySegmentEx / Stop / GetResolvedTime flags
# (patches/sg/2913-2917): test/dmime-conv-probe.c, table-driven value checks. The log is also
# checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmime-conv-gate.sh
# Mutants (dmime, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmime/*.c.
GATE_NAME=dmimeconv PROBE=dmime-conv-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='performance_(MIDIToMusic|MusicToMIDI|Stop|StopEx|GetResolvedTime|PlaySegmentEx)|segment_Compose|music values|play mode|flags|transition|from'
. "$(dirname "$0")/dmime-gate-common.sh"
