#!/bin/sh
# dmloader's stream methods and CollectGarbage (patches/sg/2951): test/dmloader-streams-probe.c.
# A probe object captures the stream the loader gives IPersistStream::Load; Stat / Write / SetSize /
# CopyTo / Commit / Revert / LockRegion / UnlockRegion are checked on it and on its clone, then
# CollectGarbage is checked. The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmloader-streams-gate.sh
# Mutants (dmloader, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmloader/*.c.
GATE_NAME=dmloaderstreams PROBE=dmloader-streams-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='loader_stream_|file_stream_|loader_CollectGarbage'
. "$(dirname "$0")/dmime-gate-common.sh"
