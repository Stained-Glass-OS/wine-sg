#!/bin/sh
# dmcompos composer (patches/sg/2918-2919): test/dmcompose-probe.c composes templates from shapes, segments from templates
# and shapes, chords from a signpost track (IDirectMusicTrack8::Compose and IDirectMusicSegment8::Compose), transitions
# (ComposeTransition) and plays them (AutoTransition), comparing lengths, commands, signposts and chord names with golden
# strings and the error codes with the documented ones. The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmcompose-gate.sh
# Mutants (dmcompos, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmcompos/*.c.
GATE_NAME=dmcompose PROBE=dmcompose-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='.*'
. "$(dirname "$0")/dmcontent-gate-common.sh"
