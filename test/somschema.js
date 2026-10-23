// somschema.js: the Schema Object Model over an XSD in a schema cache (Stained Glass OS wine-sg 6053),
// through script (IDispatch). Prints name=value lines; see test/somschema-gate.sh.
function out(k, v) { WScript.Echo(k + "=" + v); }
function tryout(k, f) { try { out(k, f()); } catch (e) { out(k, "error " + (e.number >>> 0).toString(16) + " " + e.description); } }
var xsd = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns="urn:sg-som" targetNamespace="urn:sg-som"' +
    ' elementFormDefault="qualified" version="1.2">' +
    '<xs:simpleType name="Color"><xs:restriction base="xs:string"><xs:enumeration value="red"/>' +
    '<xs:enumeration value="green"/><xs:maxLength value="5"/></xs:restriction></xs:simpleType>' +
    '<xs:complexType name="ItemType"><xs:sequence><xs:element name="title" type="xs:string"/>' +
    '<xs:element name="qty" type="xs:int" minOccurs="0" maxOccurs="unbounded"/><xs:element name="color" type="Color"/>' +
    '</xs:sequence><xs:attribute name="id" type="xs:ID" use="required"/>' +
    '<xs:attribute name="note" type="xs:string" default="none"/></xs:complexType>' +
    '<xs:element name="order"><xs:complexType><xs:sequence><xs:element name="item" type="ItemType" maxOccurs="unbounded"/>' +
    '</xs:sequence></xs:complexType></xs:element>' +
    '<xs:attribute name="lang" type="xs:language"/>' +
    '</xs:schema>';
var doc = new ActiveXObject("Msxml2.DOMDocument.6.0");
doc.async = false;
doc.loadXML(xsd);
out("xsd_parse", doc.parseError.errorCode);
var cache = new ActiveXObject("Msxml2.XMLSchemaCache.6.0");
cache.add("urn:sg-som", doc);
var s;
tryout("getschema", function () { s = cache.getSchema("urn:sg-som"); return s ? 1 : 0; });
if (!s) { out("done", 1); WScript.Quit(0); }
tryout("schema", function () { return s.itemType.toString(16) + " " + s.targetNamespace + " " + s.version; });
tryout("elements", function () { return s.elements.length + " " + s.elements.item(0).name; });
tryout("types", function () { return s.types.length + " " + s.types.itemByName("ItemType").itemType.toString(16); });
var color = s.types.itemByName("Color");
tryout("color", function () {
    return color.itemType.toString(16) + " " + color.baseTypes.item(0).name + " " + color.enumeration.length + " " +
        color.enumeration.item(1) + " " + color.maxLength;
});
tryout("valid", function () { return color.isValid("red") + " " + color.isValid("blue") + " " + color.isValid("greenish"); });
var it = s.types.itemByName("ItemType");
var model;
tryout("model", function () { model = it.contentModel; return model.itemType.toString(16) + " " + model.particles.length; });
var qty;
tryout("qty", function () {
    qty = model.particles.item(1);
    return qty.name + " " + qty.type.name + " " + qty.minOccurs + " " + qty.maxOccurs + " " + qty.type.itemType.toString(16);
});
tryout("attrs", function () {
    return it.attributes.length + " " + it.attributes.item(0).name + " " + it.attributes.item(0).use + " " +
        it.attributes.item(1).defaultValue;
});
tryout("scope", function () { return qty.scope.name + " " + qty.namespaceURI; });
tryout("anon", function () {
    var order = s.elements.itemByName("order");
    return order.type.itemType.toString(16) + " " + order.type.contentType;
});
tryout("global_attr", function () { return s.attributes.item(0).name + " " + s.attributes.item(0).type.name; });
var inst = new ActiveXObject("Msxml2.DOMDocument.6.0");
inst.async = false;
inst.loadXML('<order xmlns="urn:sg-som"><item id="a1"><title>x</title><color>red</color></item></order>');
tryout("decl", function () {
    var d = cache.getDeclaration(inst.documentElement.firstChild.firstChild);
    return d.name + " " + d.scope.name + " " + d.type.name;
});
tryout("adecl", function () {
    var d = cache.getDeclaration(inst.documentElement.firstChild.attributes.getNamedItem("id"));
    return d.name + " " + d.type.name + " " + d.use;
});
tryout("nosuch", function () { return cache.getSchema("urn:none") ? "found" : "null"; });
out("done", 1);
