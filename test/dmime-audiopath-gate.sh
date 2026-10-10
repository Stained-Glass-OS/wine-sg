#!/bin/sh
# dmime audio path members (patches/sg/2953): test/dmime-perf-probe.c (audiopath part) starts audio,
# and checks the audio path's GetObjectInPath for every stage, Activate, SetVolume with and without
# ramp, ConvertPChannel, the segment state's GetObjectInPath and StopEx on an audio path.
# SKIPs (77) without audio. The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmime-audiopath-gate.sh
# Mutants (dmime, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmime/audiopath.c.
GATE_NAME=dmimeaudiopath PROBE=dmime-perf-probe.c PROBE_ARGS=audiopath EXTRA_LIBS="-ldsound -ldxguid -lole32 -luuid -luser32"
FIXME_RE='IDirectMusicAudioPathImpl_(GetObjectInPath|Activate|SetVolume|ConvertPChannel)|performance_(CreateAudioPath|CreateStandardAudioPath|SetDefaultAudioPath|GetDefaultAudioPath)'
. "$(dirname "$0")/dmime-gate-common.sh"
