#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# kernel32 Transactional NTFS calls (6061): the *Transacted file functions did their work outside the
# transaction ("semi-stub"), so a rollback kept the changes. Now:
#  - the transaction handle is checked first: an event, NULL or a bogus handle gives ERROR_INVALID_HANDLE;
#  - with a real transaction, each call fails as on a volume without transaction support:
#    ERROR_RM_NOT_ACTIVE (6801), and nothing on disk changes (CreateFile/A, DeleteFile, MoveFile, CopyFile,
#    CreateDirectory, RemoveDirectory, Set/GetFileAttributes, FindFirstFile, GetFullPathName,
#    GetLongPathName, GetCompressedFileSize, CreateHardLink, CreateSymbolicLink);
#  - a transaction with no work commits (once) and another rolls back.
#   WINE=/opt/wine-sg/bin/wine test/txfail-gate.sh   (mutants SG_MUTANT_TXF_NO_HANDLE_CHECK, SG_MUTANT_TXF_RUN_PLAIN)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-txfail.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/txfail-probe.exe" "$HERE/txfail-probe.c" || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/txfail-probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" txfail-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect CreateTransaction 1 "CreateTransaction makes a transaction"
expect CreateFileEvent "1 6" "an event is not a transaction: ERROR_INVALID_HANDLE"
expect CreateFileNull "1 6" "nor is NULL"
expect DeleteBogus "0 6 1" "nor a bogus handle (and the file stays)"
expect CreateFile "1 6801 0" "CreateFileTransactedW: ERROR_RM_NOT_ACTIVE, no file made"
expect CreateFileA "1 6801 0" "CreateFileTransactedA too"
expect Delete "0 6801 1" "DeleteFileTransacted fails and the file stays"
expect Move "0 6801 1 0" "MoveFileTransacted fails and nothing moves"
expect Copy "0 6801 0" "CopyFileTransacted fails and nothing is copied"
expect CreateDirectory "0 6801 0" "CreateDirectoryTransacted"
expect RemoveDirectory "0 6801 1" "RemoveDirectoryTransacted"
expect SetAttributes "0 6801 1" "SetFileAttributesTransacted"
expect GetAttributes "0 6801" "GetFileAttributesTransacted"
expect FindFirst "1 6801" "FindFirstFileTransacted"
expect FullPath "0 6801" "GetFullPathNameTransacted"
expect LongPath "0 6801" "GetLongPathNameTransacted"
expect CompressedSize "1 6801" "GetCompressedFileSizeTransacted"
expect HardLink "0 6801 0" "CreateHardLinkTransacted"
expect SymLink "0 6801 0" "CreateSymbolicLinkTransacted"
expect Commit 1 "a transaction with no work commits"
expect CommitAgain "0 6705" "once (ERROR_TRANSACTION_ALREADY_COMMITTED)"
expect Rollback 1 "another rolls back"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
