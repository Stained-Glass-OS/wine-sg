#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32 IShellLibrary (6081): CLSID_ShellLibrary did not exist ("class not registered"); foobar2000 asks it for
# the Music library's folders at every start. A library is a set of folders kept in a .library-ms file (the
# documented library description schema); the standard Documents/Music/Pictures/Videos libraries come from their
# known folders until a file exists. The probe loads the Music library, builds, saves, reloads, changes and
# commits a library of its own, reads a description written the way Windows writes them (knownfolder: URL, the
# default save location on the second folder) and refuses a file that is no library.
#   WINE=/opt/wine-sg/bin/wine test/shelllibrary-gate.sh   (mutants SG_MUTANT_SHLIB_NO_ESCAPE, SHLIB_FIRST_DEFAULT)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shelllibrary.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/shelllibrary-probe.c" -lole32 -lshell32 -luuid 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect created 1 "CoCreateInstance(CLSID_ShellLibrary) gives an IShellLibrary"
expect music_load 0 "the Music library loads from its known folder"
expect music_first 1 "its first folder is the user's Music folder"
expect music_save Music "and that is where it saves"
expect music_type 1 "its folder type is Music"
expect music_pinned 1 "it is pinned to the navigation pane"
expect music_commit_ro 0x80030005 "a library opened read-only does not commit: STG_E_ACCESSDENIED"
expect docs_folder_load 0x80070057 "a known folder that is no library is refused"
expect empty_folders "(none)" "a new library has no folders (an empty array, not an error)"
expect empty_save 0x80070490 "nor a save location: ERROR_NOT_FOUND"
expect empty_commit 0x8000ffff "and nothing to commit to: E_UNEXPECTED"
expect add_a 0 "AddFolder"
expect add_a_again 0 "adding a folder twice keeps one"
expect new_folders "alpha|beta &amp; co" "GetFolders lists them in order"
expect new_save alpha "the first folder is the default save location"
expect new_save_b "beta &amp; co" "SetDefaultSaveFolder moves it"
expect set_options_bad 0x80070057 "unknown option bits are refused"
expect remove_c 0x80070490 "removing a folder not in the library: ERROR_NOT_FOUND"
expect save 0 "Save writes the library"
expect save_name 1 "as <name>.library-ms"
expect save_again 0x800700b7 "LSF_FAILIFTHERE: ERROR_ALREADY_EXISTS"
expect save_unique "Probe (2).library-ms" "LSF_MAKEUNIQUENAME picks a new name"
expect reload 0 "the saved file loads"
expect reload_folders "alpha|beta &amp; co" "with its folders (a name that looks like an entity, 'beta &amp; co', survives)"
expect reload_save "beta &amp; co" "its default save location"
expect reload_pinned 1 "its options"
expect reload_icon "imageres.dll,-1001" "its icon"
expect reload_type 1 "and its folder type"
expect remove_b 0 "RemoveFolder"
expect after_remove_save alpha "removing the save location moves it to the first folder"
expect commit 0 "Commit writes back to the file it came from"
expect reload2_folders "alpha|gamma" "and the change is there on the next load"
expect resolve_a 0 "ResolveFolder finds a folder that is there"
expect resolve_gone 0x80070002 "and reports one that is gone"
expect commit_ro 0x80030005 "read-only again refuses Commit"
expect junk 1 "a file that is no library description is refused"
expect win_load 0 "a description as Windows writes it loads"
expect win_folders "Music|alpha" "knownfolder: URLs and plain paths both become folders"
expect win_save alpha "isDefaultSaveLocation on the second folder is honoured"
expect win_type 1 "its folderType"
expect win_pinned 0 "and isLibraryPinned=false"
expect done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
