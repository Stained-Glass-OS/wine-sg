#!/bin/sh
# dmstyle style object (patches/sg/2962): test/dmcontent-style-probe.c loads a style with two bands, grooves, fills and
# two motifs from RIFF data and compares EnumBand, GetDefaultBand, EnumMotif, EnumPattern, GetEmbellishmentLength,
# GetMotif (segment length, repeats, start and loop points, motif and band tracks) and the chord map methods with it.
# The log is also checked: none of the old stubs may log a FIXME.
#
#   WINE=/opt/wine-sg/bin/wine test/dmcontent-style-gate.sh
# Mutants (dmstyle, -DSG_MUTANT_x): see the #ifdef SG_MUTANT_ lines in dlls/dmstyle/style.c.
GATE_NAME=dmcstyle PROBE=dmcontent-style-probe.c EXTRA_LIBS="-ldxguid -lole32 -luuid -luser32"
FIXME_RE='style_(EnumBand|GetDefaultBand|EnumMotif|GetMotif|GetDefaultChordMap|EnumChordMap|GetChordMap|GetEmbellishmentLength|EnumPattern)|IPersistStreamImpl_Load'
. "$(dirname "$0")/dmcontent-gate-common.sh"
