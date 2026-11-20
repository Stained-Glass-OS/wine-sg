#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ICorDebug object values over Wine Mono's soft debugger agent (wine-sg 8200, 8201: NET-DBG): what a .NET debugger
# shows of a stopped program's data. A debugger (cordbgobj-probe.exe, raw ICorDebug) and a managed debuggee
# (cordbgobj-app.cs):
#   domains   EnumerateAppDomains (one), its assemblies (mscorlib and App.exe by file name), App's module, its
#             name, its assembly, its metadata (IMetaDataImport), the module found again from that metadata
#   functions App.Hold found in the module's metadata, the module's function for its token, the function's
#             module and class; classes from tokens
#   objects   stopped at a breakpoint in Hold: an object argument dereferenced (ICorDebugObjectValue), its class,
#             its fields by metadata token (int, string, double, a base class's, an object's field, a struct
#             field's fields), a static field (ICorDebugClass::GetStaticFieldValue); a token that is not a field
#             of it refused
#   arrays    int[] (element type, rank, count, elements by index and position, out of range refused),
#             string[] with a null, int[,] (rank, dimensions, an element)
#   structs   a struct argument's fields, a struct's static, an enum's value (ICorDebugGenericValue)
#   eval      (8201) at the breakpoint: App.Twice(21) called (an argument made with CreateValue): 42, the
#             process stopped again; NewString; NewObject with a constructor; a call that throws (EvalException)
#   setting   a field, an array element and an argument set (ICorDebugGenericValue::SetValue): continued, the
#             program writes what it then has: "500 111 1234"
#
# Needs Wine Mono with the dt_unix transport (SG_MONO_RUNTIME=DIR names one) and mingw. SKIP without.
#   WINE=/opt/wine-sg/bin/wine test/cordbgobj-gate.sh
#   mutants: SG_MUTANT_CDB_DEREF_STRINGS_ONLY, SG_MUTANT_CDB_STRUCT_NONE, SG_MUTANT_CDB_FIELD_INDEX,
#            SG_MUTANT_CDB_SET_NOOP, SG_MUTANT_CDB_NO_ASSEMBLIES, SG_MUTANT_CDB_EVAL_NO_RESULT (mscoree cordebug.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
MCS=""; RUNTIME=""
for d in "${SG_MONO_RUNTIME:-}" "${SG_MONO_DIR:-}" "$(dirname "$WINE")/../share/wine/mono" /opt/wine-sg/share/wine/mono /usr/share/wine/mono; do
    [ -n "$d" ] || continue
    for m in "$d" "$d"/wine-mono-*; do
        [ -f "$m/lib/mono/4.5/mcs.exe" ] || continue
        [ -n "$MCS" ] || MCS="$m/lib/mono/4.5/mcs.exe"
        [ -z "$RUNTIME" ] && grep -q mono_sg_debugger_unix_transport "$m/bin/libmono-2.0-x86_64.dll" 2>/dev/null && RUNTIME=$m
    done
done
[ -n "$MCS" ] || { echo "SKIP: Wine Mono is not installed"; exit 77; }
[ -n "$RUNTIME" ] || { echo "SKIP: no Wine Mono with the dt_unix transport (SG_MONO_RUNTIME)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-cordbgobj.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="winemenubuilder.exe=d" DISPLAY=
unset WINE_MONO_DEBUGGER_AGENT
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
"$MINGW" -O1 -o "$T/probe.exe" "$HERE/cordbgobj-probe.c" -lole32 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Mono' /v RuntimePath /d "Z:$(printf '%s' "$RUNTIME" | tr '/' '\\')" /f >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/cordbgobj-app.cs" "$T/app.cs"
(cd "$T" && timeout -s KILL 300 "$WINE" "$MCS" -debug -out:App.exe app.cs >/dev/null 2>&1 </dev/null)
[ -f "$T/App.exe" ] || { fail "mcs did not build the program"; exit 1; }
cp "$T/App.exe" "$T/probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 300 "$WINE" probe.exe 'C:\App.exe' 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Launch 0 "CreateProcess"
expect Domains "0 1 1" "one application domain"
expect Assemblies "0 1 1" "its assemblies: more than one, App.exe among them"
expect Modules "0 1" "App's module"
expect Module.Name 1 "its name: the file"
expect Module.Assembly 1 "its assembly: the same object"
expect Module.MetaData 0 "its metadata (IMetaDataImport)"
expect Module.FromMetaData "0 1" "the module found again from its metadata"
expect MetaData.Tokens 1 "the program's types, method and fields in the metadata"
expect Function "0 1" "the module's function for App.Hold's token"
expect Function.Module 1 "the function's module"
expect Function.Class 1 "the function's class (App)"
expect Classes 1 "classes from tokens"
expect Hold "0 1" "a breakpoint in App.Hold is hit"
expect Object "0x12 1" "an object argument dereferenced: an object value"
expect Object.Class "1 1" "its class (Derived), not a value class"
expect Field.Id 41 "its field Id (int): 41"
expect Set.Field 0 "the field set to 500"
expect Field.IdAgain 500 "read again: 500"
expect Field.Name first "its field Name (a string): first"
expect Field.Weight 1.50 "its field Weight (double): 1.5"
expect Field.Extra 9000000000 "a field of the derived class: 9000000000"
expect Field.ExtraNoClass 9000000000 "the same with no class given"
expect Field.NextId 42 "the object in its field Next: Id 42"
expect Field.At "0x11 1 3 4" "a struct in a field: a value class, X 3, Y 4"
expect Static.Kind "0 sg-node" "a static field (GetStaticFieldValue): sg-node"
expect Field.Bad 1 "a token that is not a field: refused"
expect Array "0x1d 0x8 1 4" "int[]: elements int, rank 1, count 4"
expect Array.2 30 "element [2]: 30"
expect Array.At3 40 "element at position 3: 40"
expect Array.Out 1 "position 4: E_INVALIDARG"
expect Set.Element 0 "element [0] set to 111"
expect Words.1 1 "string[] element [1]: null"
expect Words.2 gamma "string[] element [2]: gamma"
expect Grid "0x14 2 2 3 99" "int[,]: rank 2, dimensions 2x3, element [1,2]: 99"
expect Struct "0x11 -5 6" "a struct argument: X -5, Y 6"
expect Static.Made "0 7" "the struct's static: 7"
expect Enum "0x11 4" "an enum argument: Color.Blue, 4"
expect Round 0x8 "an int argument"
expect Set.Argument 0 "the argument set to 1234"
expect Eval.Create 0 "a thread's evaluation (CreateEval)"
expect Eval.Value "0 21" "a value of the debugger's (CreateValue), set to 21"
expect Eval.NotYet 1 "no result before it ran: CORDBG_E_FUNC_EVAL_NOT_COMPLETE"
expect Eval.Call "0 1 1 42" "App.Twice(21) called: EvalComplete, stopped again, result 42"
expect Eval.String "0 1 sg-eval" "NewString: a string sg-eval"
expect Eval.NewObject "0 1 0x12 0.25" "NewObject with Node's constructor: an object, its constructor ran (Weight 0.25)"
expect Eval.Throws "0 1 0x12" "App.Fail(): EvalException, the exception object as the result"
expect Seen "500 111 1234" "continued, the program has the values set"
expect done 1 "the probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
