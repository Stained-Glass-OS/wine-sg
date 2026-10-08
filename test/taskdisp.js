// taskdisp-gate.sh's script (patches/sg/1631): the Task Scheduler from a
// script, through IDispatch -- every call here failed with E_NOTIMPL.
var service = new ActiveXObject("Schedule.Service");
service.Connect();
var root = service.GetFolder("\\");
WScript.Echo("folder " + root.Path);
var task = service.NewTask(0);
task.RegistrationInfo.Description = "SG dispatch probe";
task.Settings.Enabled = false;
var trigger = task.Triggers.Create(1);          // TASK_TRIGGER_TIME
trigger.StartBoundary = "2030-01-01T10:00:00";
var action = task.Actions.Create(0);            // TASK_ACTION_EXEC
action.Path = "C:\\windows\\system32\\cmd.exe";
var reg = root.RegisterTaskDefinition("SG Dispatch Probe", task, 6, null, null, 3);
WScript.Echo("registered " + reg.Name);
WScript.Echo("description " + reg.Definition.RegistrationInfo.Description);
WScript.Echo("trigger " + reg.Definition.Triggers.Item(1).StartBoundary);
root.DeleteTask("SG Dispatch Probe", 0);
WScript.Echo("done");
