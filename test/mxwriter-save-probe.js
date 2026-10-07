// mxwriter-save-probe.js (test/mxwriter-save-gate.sh, wine-sg 1480): what Delphi's
// xmldoc does to save formatted XML -- a SAX reader feeding an MXXMLWriter
var steps = [];
function step(name, f) { try { var r = f(); steps.push(name + ": ok" + (r !== undefined ? " " + r : "")); } catch (e) { steps.push(name + ": ERROR " + (e.number >>> 0).toString(16) + " " + e.description); } }
var ver = WScript.Arguments.length ? WScript.Arguments(0) : "3.0";
var r = new ActiveXObject("Msxml2.SAXXMLReader." + ver), w = new ActiveXObject("Msxml2.MXXMLWriter." + ver);
step("output = ''", function () { w.output = ""; });
step("indent", function () { w.indent = true; });
step("omitXMLDeclaration", function () { w.omitXMLDeclaration = false; });
step("contentHandler", function () { r.contentHandler = w; });
step("lexical-handler", function () { r.putProperty("http://xml.org/sax/properties/lexical-handler", w); });
step("declaration-handler", function () { r.putProperty("http://xml.org/sax/properties/declaration-handler", w); });
step("parse", function () { r.parse("<a><b>x</b><!-- c --></a>"); });
step("output", function () { return String(w.output).replace(/\r?\n/g, "\\n"); });
WScript.Echo(steps.join("\n"));
