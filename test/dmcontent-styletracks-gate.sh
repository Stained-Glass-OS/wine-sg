#!/bin/sh
# dmstyle tracks (patches/sg/2960): test/dmcontent-styletracks-probe.c loads the command, chord and mute tracks from
# RIFF data, fills the style, motif and audition tracks, and checks parameters, Clone, Join, Save, notifications and
# the extended methods against that data. The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmcontent-styletracks-gate.sh
# Mutants (dmstyle, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmstyle/*track.c and dmutils.c.
GATE_NAME=dmcstyletracks PROBE=dmcontent-styletracks-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='command_track_|chord_track_|mute_track_|style_track_(Init|InitPlay|EndPlay|AddNotificationType|RemoveNotificationType|Clone|PlayEx|GetParam|GetParamEx|SetParam|SetParamEx|Join)|motif_track_(Init|InitPlay|EndPlay|AddNotificationType|RemoveNotificationType|Clone|PlayEx|GetParam|GetParamEx|SetParam|SetParamEx)|audition_track_(Init|InitPlay|EndPlay|AddNotificationType|RemoveNotificationType|Clone|PlayEx|GetParam|GetParamEx|SetParam|SetParamEx)|IPersistStreamImpl_Save'
. "$(dirname "$0")/dmcontent-gate-common.sh"
