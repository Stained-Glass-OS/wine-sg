#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
. "$(dirname "$0")/dplayx-gate-lib.sh"
# dplayx's DirectPlayLobby stubs (patches/sg/2988): RegisterApplication,
# UnregisterApplication, EnumLocalApplications (Unicode), RunApplication
# (Unicode and the guards), Send/ReceiveLobbyMessage, SetLobbyMessageEvent
# and ConnectEx. test/dplayx-lobby-probe.c checks every result and value, the
# registry the applications are kept in, and the messages between a lobby
# client and the application that it starts (the probe starts itself as that
# application); the log of the run must not show a FIXME of any of them.
#
#   WINE=/opt/wine-sg/bin/wine test/dplayx-lobby-gate.sh
# Mutants (dplayx, -DSG_MUTANT_x): L_REGGUID L_UNREG L_ENUMW L_RUN L_MSGORDER L_MSGKEEP
# L_MSGSIZE L_EVENT L_SIDE L_CONNECT L_CONNECTFAIL L_CONNECTFLAGS L_FIXME
dplayx_gate lobby dplayx-lobby-probe.c 'IDirectPlayLobby3A?Impl_(EnumLocalApplications|ReceiveLobbyMessage|RunApplication|SendLobbyMessage|SetLobbyMessageEvent|RegisterApplication|UnregisterApplication)|DPL_ConnectEx|IClassFactoryImpl_LockServer'
