#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
. "$(dirname "$0")/dplayx-gate-lib.sh"
# dplayx's DirectPlay 4 methods that were stubs (patches/sg/2985): GetPlayerFlags,
# GetPlayerAccount, GetPlayerAddress, GetGroupFlags, Get/SetGroupOwner,
# Get/SetGroupConnectionSettings, StartSession, SendChatMessage, CancelMessage,
# CancelPriority, the system messages of DestroyPlayer and DestroyGroup, the
# state checks of the enumerations and CreateGroup, SecureOpen parameters.
# test/dplayx-dp4-probe.c runs them on a service provider that needs no
# network (test/dplayx-fakesp.c) and checks every result and value; the log
# of the run must not show a FIXME of any of them.
#
#   WINE=/opt/wine-sg/bin/wine test/dplayx-dp4-gate.sh
# Mutants (dplayx, -DSG_MUTANT_x): DP4_FLAGS DP4_ADDRESS DP4_ACCOUNT DP4_LOBBY DP4_GROUPFLAGS
# DP4_CHAT DP4_CANCEL DP4_DESTROYMSG DP4_CURRENT DP4_SPFLAGS DP4_ENUM DP4_SENDCOMPLETE
# DP4_CREATEGROUP DP4_SECURE DP4_REMOTEID DP4_FIXME
dplayx_gate dp4 dplayx-dp4-probe.c 'IDirectPlay4A?Impl_(GetPlayerAddress|GetGroupConnectionSettings|SendChatMessage|SetGroupConnectionSettings|StartSession|GetGroupFlags|GetPlayerAccount|GetPlayerFlags|GetGroupOwner|SetGroupOwner)|DP_IF_(DestroyGroup|DestroyPlayer|EnumGroupPlayers|EnumGroupsInGroup)|DP_GetRemoteNextObjectId|DP_SecureOpen|dplay_cancelmsg|DP_MSG_ForwardPlayerCreation'
