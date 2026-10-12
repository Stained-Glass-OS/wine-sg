#!/bin/sh
# dmstyle track playback (patches/sg/2954): test/dmcontent-play-probe.c builds a style (parts, groove, fill, motif patterns) as
# RIFF data and plays the style, motif and audition tracks into a performance; the note and curve messages are compared with a
# table (times, pitches by play mode and chord, durations, positions, channels, variation choice and locks, the command and
# chord tracks of a segment state). The log is also checked: no Play may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmcontent-play-gate.sh
# Mutants (dmstyle, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmstyle/style.c.
GATE_NAME=dmcplay PROBE=dmcontent-play-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='style_track_Play|motif_track_Play|audition_track_Play'
. "$(dirname "$0")/dmcontent-gate-common.sh"
