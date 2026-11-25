#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HKCU\Keyboard Layout\Preload is the person's (patches/sg/9157): Windows writes it when the profile is made and
# never again; Settings > Time & Language > Language writes the person's keyboards there (German first for an
# English user: "1"="d0010409", Substitutes d0010409 = 00000407). Wine wrote "1" = the locale's layout at every
# process start, so the person's first keyboard was US English again as soon as any program ran.
#   1. a new prefix has Preload "1" (the locale's layout, as Windows' setup writes it)
#   2. German first, US second (as Settings writes them): two programs later, still so
#   WINE=/opt/wine-sg/bin/wine test/kbdpreload-gate.sh
# Mutant: SG_MUTANT_KBD_PRELOAD_REWRITE (win32u/sysparams.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdpreload.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset DISPLAY
rc=0
pre() { "$WINE" reg query 'HKCU\Keyboard Layout\Preload' /v "$1" 2>/dev/null | tr -d '\r' | sed -n "s/^ *$1 *REG_SZ *//p"; }
expect() { if [ "$2" = "$3" ]; then echo "PASS  $1"; else echo "FAIL  $1: got '$2', want '$3'"; rc=1; fi; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
expect "a new profile has Preload 1, the locale's layout" "$(pre 1)" 00000409
"$WINE" reg add 'HKCU\Keyboard Layout\Substitutes' /v d0010409 /d 00000407 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Keyboard Layout\Preload' /v 1 /d d0010409 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Keyboard Layout\Preload' /v 2 /d 00000409 /f >/dev/null 2>&1
timeout -s KILL 60 "$WINE" cmd /c exit >/dev/null 2>&1
timeout -s KILL 60 "$WINE" cmd /c exit >/dev/null 2>&1
"$WINESERVER" -w
expect "German first stays first after programs ran (Preload 1)" "$(pre 1)" d0010409
expect "...and US second (Preload 2)" "$(pre 2)" 00000409
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
