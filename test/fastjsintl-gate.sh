#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# jscript: Intl.Collator / NumberFormat / DateTimeFormat in the IE11 (ES6) language version.
#
#   WINE=/path/obj/wine test/fastjsintl-gate.sh   (mutants SG_MUTANT_JS_NOINTL, _NOSENS, _NOSEP, _NOOWNBUILTIN, _NOARGSCLASS)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
OBJ="$(dirname "$WINE")"
TEST="${JSCRIPT_TEST:-$OBJ/dlls/jscript/tests/x86_64-windows/jscript_test.exe}"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$TEST" ] || { echo "SKIP: no jscript_test.exe at $TEST"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastjsintl.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
cat > "$T/intl.js" <<'JS'
var AACUTE = String.fromCharCode(225), d = Date.UTC(2014, 0, 2, 15, 4, 5), c, nf, r;
ok(typeof Intl === "object" && typeof Intl.Collator === "function", "Intl exists");
c = new Intl.Collator("en-US", {sensitivity: "base"});
ok(c.compare("a", "A") === 0 && c.compare("a", AACUTE) === 0 && c.compare("a", "b") < 0, "base sensitivity");
c = new Intl.Collator("en-US", {sensitivity: "accent"});
ok(c.compare("a", "A") === 0 && c.compare("a", AACUTE) !== 0, "accent sensitivity");
ok(new Intl.Collator("en-US", {numeric: true}).compare("a2", "a10") < 0, "numeric collation");
ok("a2".localeCompare("a10", "en-US", {numeric: true}) === -1, "localeCompare options");
nf = new Intl.NumberFormat("en-US");
ok(nf.format(1234567.891) === "1,234,567.891", "grouping " + nf.format(1234567.891));
ok(new Intl.NumberFormat("de-DE").format(1234567.891) === "1.234.567,891", "de-DE");
ok(new Intl.NumberFormat("en-US", {style: "currency", currency: "USD"}).format(1234.5) === "$1,234.50", "currency");
ok(new Intl.NumberFormat("en-US", {style: "percent"}).format(0.256) === "26%", "percent");
ok(new Intl.NumberFormat("en-US", {minimumIntegerDigits: 3}).format(5) === "005", "min integer digits");
ok(new Intl.NumberFormat("en-US", {maximumSignificantDigits: 3}).format(123456) === "123,000", "significant digits");
function fmt(o, l) { o.timeZone = "UTC"; return new Intl.DateTimeFormat(l || "en-US", o).format(d); }
ok(fmt({}) === "1/2/2014", "default date " + fmt({}));
ok(fmt({year: "numeric", month: "long", day: "numeric"}) === "January 2, 2014", "long date " + fmt({year: "numeric", month: "long", day: "numeric"}));
ok(fmt({month: "long"}) === "January", "month only " + fmt({month: "long"}));
ok(fmt({weekday: "long", year: "numeric", month: "long", day: "numeric"}) === "Thursday, January 2, 2014", "weekday date");
ok(fmt({hour: "numeric", minute: "2-digit", second: "2-digit"}) === "3:04:05 PM", "time " + fmt({hour: "numeric", minute: "2-digit", second: "2-digit"}));
ok(fmt({hour: "numeric", hour12: true}) === "3 PM", "hour only " + fmt({hour: "numeric", hour12: true}));
ok(fmt({year: "numeric", month: "numeric", day: "numeric"}, "de-DE") === "2.1.2014", "de-DE date");
ok(Intl.NumberFormat.supportedLocalesOf(["en-US", "de-de", "zz-ZZ-nonsense"]).join() === "en-US,de-DE", "supportedLocalesOf");
try { new Intl.NumberFormat("en_US"); ok(false, "bad tag accepted"); } catch(e) { ok(e instanceof RangeError, "RangeError for a bad tag"); }
ok(Object.getOwnPropertyNames([]).join() === "length", "getOwnPropertyNames([]) lists length");
ok(Object.getOwnPropertyNames(Array.prototype).indexOf("push") !== -1, "getOwnPropertyNames(Array.prototype) lists push");
(function() { ok(Object.prototype.toString.call(arguments) === "[object Arguments]", "arguments class"); })();
ok(Object.prototype.toString.call(JSON) === "[object JSON]" && Object.prototype.toString.call(new Map()) === "[object Object]", "class names");
ok(Object.prototype.toString.call(new Intl.Collator()) === "[object Object]", "Intl class name");
try { Object.defineProperty({}, "x", {get: function() {}, value: 1}); ok(false, "no throw"); } catch(e) { ok((e.number >>> 0) === 0x800a01bd && e instanceof TypeError, "descriptor error number"); }
trace("INTL-GATE-DONE");
JS
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
JSTEST_ES6=1 timeout -s KILL 300 "$WINE" "$TEST" run "$(env WINEDEBUG=-all "$WINE" winepath -w "$T/intl.js" 2>/dev/null | tr -d '\r')" >"$T/o" 2>&1 </dev/null
tr -d '\r' < "$T/o" > "$T/o2"
if ! grep -q "INTL-GATE-DONE" "$T/o2"; then
    echo "FAIL  the Intl script did not run to the end:"; tail -5 "$T/o2"; echo "RESULT: FAIL"; exit 1
fi
n=$(grep -c "Test failed" "$T/o2")
if [ "$n" != 0 ]; then
    echo "FAIL  $n failures:"; grep "Test failed" "$T/o2" | head -8; echo "RESULT: FAIL"; exit 1
fi
echo "PASS  Collator, NumberFormat, DateTimeFormat and the locale methods behave like IE11"
echo "RESULT: PASS"; exit 0
