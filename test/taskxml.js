// taskxml-gate.sh's script (patches/sg/1633): parts of a task definition
// that were E_NOTIMPL -- Data, the registration's SecurityDescriptor, the
// XmlText of the registration info, settings and actions, and For-Each
// over the actions and triggers.
var service = new ActiveXObject("Schedule.Service");
service.Connect();
var root = service.GetFolder("\\");
var task = service.NewTask(0);
function attempt(name, f) { try { f(); } catch (e) { WScript.Echo("error " + name + " " + (e.number >>> 0).toString(16)); } }
attempt("data", function () { task.Data = "sg-data"; });
attempt("sd", function () { task.RegistrationInfo.SecurityDescriptor = "D:(A;;FA;;;BA)"; });
task.RegistrationInfo.Description = "SG XML probe";
task.Settings.Enabled = false;
var a1 = task.Actions.Create(0); a1.Path = "C:\\windows\\system32\\cmd.exe";
var a2 = task.Actions.Create(0); a2.Path = "C:\\windows\\system32\\notepad.exe";
var t1 = task.Triggers.Create(1); t1.StartBoundary = "2030-01-01T10:00:00";
var t2 = task.Triggers.Create(9);
var n = 0;
attempt("actions enum", function () { for (var e = new Enumerator(task.Actions); !e.atEnd(); e.moveNext()) n++; });
WScript.Echo("actions " + n);
n = 0;
attempt("triggers enum", function () { for (var e = new Enumerator(task.Triggers); !e.atEnd(); e.moveNext()) n++; });
WScript.Echo("triggers " + n);
attempt("reginfo xml", function () { WScript.Echo("reginfoxml " + (task.RegistrationInfo.XmlText.indexOf("<Description>SG XML probe</Description>") >= 0)); });
attempt("settings xml", function () { task.Settings.XmlText = "<Settings><Priority>4</Priority></Settings>"; WScript.Echo("priority " + task.Settings.Priority); });
attempt("actions xml", function () {
    var x = task.Actions.XmlText;
    WScript.Echo("actionsxml " + (x.indexOf("notepad.exe") >= 0));
    task.Actions.XmlText = "<Actions Context=\"Author\"><Exec><Command>C:\\windows\\regedit.exe</Command></Exec></Actions>";
    WScript.Echo("actionsput " + task.Actions.Count + " " + task.Actions.Item(1).Path);
});
var reg = root.RegisterTaskDefinition("SG XML Probe", task, 6, null, null, 3);
var def = root.GetTask("SG XML Probe").Definition;
attempt("data back", function () { WScript.Echo("data " + def.Data); });
attempt("sd back", function () { WScript.Echo("sd " + def.RegistrationInfo.SecurityDescriptor); });
root.DeleteTask("SG XML Probe", 0);
WScript.Echo("done");
