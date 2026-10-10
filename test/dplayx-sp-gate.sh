#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
. "$(dirname "$0")/dplayx-gate-lib.sh"
# dplayx's interfaces for service providers that were stubs (patches/sg/2986):
# IDirectPlaySP (AddMRUEntry, EnumMRUEntries, CreateAddress,
# CreateCompoundAddress, GetPlayerFlags, SendComplete) and IDPLobbySP (the
# remote group and player operations, EnumSessionsResponse, HandleMessage,
# SendChatMessage, the name and session description calls, StartSession,
# Get/SetSPDataPointer). test/dplayx-sp-probe.c calls them the way a provider
# does, through test/dplayx-fakesp.c, and checks what DirectPlay shows of the
# calls; the log of the run must not show a FIXME of any of them.
#
#   WINE=/opt/wine-sg/bin/wine test/dplayx-sp-gate.sh
# Mutants (dplayx, -DSG_MUTANT_x): SP_MRUMAX SP_MRUDUP SP_MRUORDER SP_ADDRESS SP_FLAGS
# SP_COMPOUND SP_FIXME LSP_STATE LSP_CREATEGROUP LSP_PLAYER LSP_DESTROY LSP_ENUMRESP
# LSP_DATAPTR LSP_MSG LSP_CHAT LSP_SESSIONDESC LSP_START LSP_NAMES LSP_SENDGUARD
dplayx_gate sp dplayx-sp-probe.c 'IDirectPlaySPImpl_(AddMRUEntry|CreateAddress|EnumMRUEntries|GetPlayerFlags|CreateCompoundAddress|SendComplete|HandleMessage|QueryInterface)|IDPLobbySPImpl_[A-Za-z]+'
