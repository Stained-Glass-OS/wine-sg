#!/bin/sh
# dmscript script object and script track (patches/sg/2805, 2806): test/dmscript-probe.c loads VBScript and
# JScript scripts from RIFF data and from files, runs routines, reads and writes variables of every kind,
# enumerates routines and variables, checks the error information, and plays a script track loaded from a
# segment file whose events call routines of a script. The log is also checked: none of the old stubs may
# log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmscript-gate.sh
# Mutants (dmscript, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmscript/*.c.
GATE_NAME=dmscript PROBE=dmscript-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -loleaut32 -luser32"
FIXME_RE='IDirectMusicScriptImpl_|script_track_|IPersistStreamImpl_|script_IPersistStream_'
. "$(dirname "$0")/dmscript-gate-common.sh"
