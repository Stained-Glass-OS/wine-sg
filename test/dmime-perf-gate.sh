#!/bin/sh
# dmime performance / segment / segment state / audio path members (patches/sg/2952, 2953):
# test/dmime-perf-probe.c plays segments into a performance whose tool graph holds a collecting tool and
# checks IsPlaying, StopEx, Invalidate, SetParam / GetParam / GetParamEx, the time and rhythm methods,
# PlaySegmentEx start times, the port / PChannel bookkeeping, the segment's track configuration and
# audio path configuration, and (when audio starts) the audio path's GetObjectInPath / SetVolume /
# ConvertPChannel / Activate. The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmime-perf-gate.sh
# Mutants (dmime, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmime/*.c.
GATE_NAME=dmimeperf PROBE=dmime-perf-probe.c PROBE_ARGS=perf EXTRA_LIBS="-ldsound -ldxguid -lole32 -luuid -luser32"
FIXME_RE='performance_(Stop|IsPlaying|AddNotificationType|RemoveNotificationType|AddPort|RemovePort|AssignPChannelBlock|AssignPChannel|Invalidate|SetParam|GetQueueTime|AdjustTime|CloseDown|GetResolvedTime|TimeToRhythm|RhythmToTime|PlaySegmentEx|StopEx|CreateAudioPath|CreateStandardAudioPath|SetDefaultAudioPath|GetDefaultAudioPath|GetParamEx|SetGraph|tool_Flush|tool_ProcessPMsg)|segment_(InitPlay|GetGraph|SetGraph|AddNotificationType|RemoveNotificationType|SetPChannelsUsed|SetTrackConfig|GetAudioPathConfig)|segment_state_(GetSeek|SetTrackConfig|GetObjectInPath)'
. "$(dirname "$0")/dmime-gate-common.sh"
