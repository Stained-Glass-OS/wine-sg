#!/bin/sh
# dmband band track (patches/sg/2961): test/dmcontent-band-probe.c loads bands with instruments, puts them into
# band tracks (Load, SetParam), plays, clones, joins and asks them (also through the extended methods) and compares
# the patch messages and parameters with the data. The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmcontent-band-gate.sh
# Mutants (dmband, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmband/bandtrack.c.
GATE_NAME=dmcband PROBE=dmcontent-band-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='band_track_(Init|InitPlay|EndPlay|Play|Clone|PlayEx|GetParamEx|SetParamEx|Join)'
. "$(dirname "$0")/dmcontent-gate-common.sh"
