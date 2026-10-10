#!/bin/sh
# dmcompos (patches/sg/2963): test/dmcontent-compos-probe.c loads chord maps (scale), puts them into chord map tracks
# (SetParam, Load, Clone, Join, extended methods), loads, saves, clones and joins signpost tracks, changes the chord
# map and scale of a segment with ChangeChordMap and creates the template object, comparing every answer with the data.
# The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmcontent-compos-gate.sh
# Mutants (dmcompos, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmcompos/*.c.
GATE_NAME=dmccompos PROBE=dmcontent-compos-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='chordmap_track_|signpost_track_(Init|InitPlay|EndPlay|Play|AddNotificationType|RemoveNotificationType|Clone|PlayEx|Join)|IPersistStreamImpl_(Load|Save)|IDirectMusicChordMapImpl_GetScale|IDirectMusicComposerImpl_ChangeChordMap|create_direct_music_template'
. "$(dirname "$0")/dmcontent-gate-common.sh"
