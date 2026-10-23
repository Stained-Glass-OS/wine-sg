// wmipoly.js: WQL queries of a class give its subclasses' instances (Stained Glass OS wine-sg 6054).
function out(k, v) { WScript.Echo(k + "=" + v); }
function q(wql) {
    var r = { count: 0, classes: {} }, e, o;
    try {
        e = new Enumerator(svc.ExecQuery(wql));
        for (; !e.atEnd(); e.moveNext()) { o = e.item(); r.count++; r.classes[o.Path_.Class] = 1; }
    } catch (x) { r.error = (x.number >>> 0).toString(16); }
    return r;
}
function classes(r) { var a = [], k; for (k in r.classes) a.push(k); a.sort(); return a.join(","); }
var svc = GetObject("winmgmts:\\\\.\\root\\cimv2");
var procs = q("SELECT * FROM Win32_Processor");
var cimprocs = q("SELECT * FROM CIM_Processor");
out("cim_processor", (cimprocs.count == procs.count && procs.count > 0) + " " + classes(cimprocs));
var dev = q("SELECT * FROM CIM_LogicalDevice");
out("logical_device", (dev.classes["Win32_Processor"] ? 1 : 0) + " " + (dev.classes["Win32_LogicalDisk"] ? 1 : 0) + " " + (dev.count > procs.count ? 1 : 0));
var disk = q("SELECT * FROM CIM_LogicalDisk WHERE DeviceID = 'C:'");
out("logical_disk", disk.count + " " + classes(disk));
var setting = q("SELECT * FROM CIM_Setting");
out("setting", setting.classes["Win32_Environment"] ? 1 : 0);
var svcbase = q("SELECT * FROM Win32_BaseService");
var svcs = q("SELECT * FROM Win32_Service");
out("base_service", (svcbase.count >= svcs.count && svcs.count > 0) + " " + (svcbase.classes["Win32_Service"] ? 1 : 0));
out("leaf", classes(q("SELECT * FROM Win32_Processor")));
var cls = svc.Get("CIM_Processor");
out("class_def", cls.Path_.Class);
out("done", 1);
