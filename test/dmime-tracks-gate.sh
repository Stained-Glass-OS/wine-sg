#!/bin/sh
# dmime track objects (patches/sg/2950): test/dmime-tracks-probe.c loads the sysex, lyrics, tempo,
# time signature, marker, sequence, parameter control and segment trigger tracks and a tool graph from
# RIFF data, plays and clones them, and checks the messages and parameters against that data.
# The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmime-tracks-gate.sh
# Mutants (dmime, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmime/*track.c, graph.c, dmime_main.c.
GATE_NAME=dmimetracks PROBE=dmime-tracks-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='wave_track_|sysex_track_|sequence_track_|lyrics_track_|segment_track_|paramcontrol_track_|tempo_track_|IDirectMusicTrackImpl_|marker_IPersist|sys_IPersist|param_IPersist|graph_IPersist'
. "$(dirname "$0")/dmime-gate-common.sh"
