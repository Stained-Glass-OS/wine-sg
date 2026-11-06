#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml markup on a program's own types (patches/sg/6262): a member a program's metadata gives
# on a base type is set through that type (WinUI 2's RevealBackgroundBrush.TargetTheme and Color are
# RevealBrush's: Windows Terminal's 74 reveal brushes had neither), and a program's enum member given as
# digits, as compiled markup gives it, or by name reaches it as IReference<the enum>. A framework base
# type whose metadata has no members (E_NOTIMPL, as WinUI's provider throws) ends the walk with the
# member not found, which compiled markup passes over (its E_NOTIMPL stopped Windows Terminal's). A value of the program's own type
# (WinUI 2's CornerRadiusFilterKind) is made by that type's CreateFromString.
# test/xamlappbase-probe.c (its application is its own IXamlMetadataProvider).
#   WINE=/opt/wine-sg/bin/wine test/xamlappbase-gate.sh
# Mutants (dlls/windows.ui.xaml/markup.c): SG_MUTANT_APP_MEMBER_NO_BASE, SG_MUTANT_ENUM_NO_DIGITS,
# SG_MUTANT_APP_NO_CREATE_FROM_STRING, SG_MUTANT_APP_BASE_FAILS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlappbase.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlappbase-probe.exe" "$HERE/xamlappbase-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/xamlappbase-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlappbase-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^app=1$' "the program's application"
chk '^digits_load=00000000 root=1$' "markup on the program's type loads"
chk '^digits theme=1 ' "a base type's enum member, as digits, reaches the program as IReference<ApplicationTheme> (Dark)"
chk ' count=7 ' "a base type's Int32 member"
chk ' label=1$' "the type's own member"
chk '^kind_load=00000000 root=1$' "markup with an enum of the program's loads"
chk '^kind kind=1 count=3$' "an enum of the program's is made by its type (CreateFromString)"
chk '^missing_load=80070057 root=0$' "a member of none of the types is not found (E_INVALIDARG), not the framework metadata's E_NOTIMPL"
chk '^name theme=0$' "the enum member by its name (Light)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
