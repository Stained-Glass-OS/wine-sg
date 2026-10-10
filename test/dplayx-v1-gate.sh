#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
. "$(dirname "$0")/dplayx-gate-lib.sh"
# dplayx's IDirectPlay (version 1) interface, which was E_NOTIMPL stubs
# (patches/sg/2987): every method, on the service provider of
# test/dplayx-fakesp.c. test/dplayx-v1-probe.c checks the results and the
# values that come back (names in two buffers, the DPSESSIONDESC of an
# enumerated session, the event that CreatePlayer makes, the callbacks'
# arguments); the log of the run must not show a FIXME of any of them.
#
#   WINE=/opt/wine-sg/bin/wine test/dplayx-v1-gate.sh
# Mutants (dplayx, -DSG_MUTANT_x): V1_SESSION V1_ENUMCB V1_EVENT V1_ENABLE V1_PREVIOUS
# V1_NAMES V1_INIT V1_OPEN V1_SAVE V1_COUNT V1_FIXME
dplayx_gate v1 dplayx-v1-probe.c 'IDirectPlayImpl_[A-Za-z]+'
