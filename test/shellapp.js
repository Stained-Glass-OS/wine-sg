// Shell.Application's folders, items, shortcuts and verbs (patches/sg/1645),
// run by test/shellapp-gate.sh under cscript.
var failures = 0;
function check(ok, what) { WScript.Echo((ok ? "PASS  " : "FAIL  ") + what); if (!ok) failures++; }
function lower(x) { return typeof x == "string" ? x.toLowerCase() : "(" + x + ")"; }
function attempt(f) { try { return f(); } catch (e) { WScript.Echo("  error: " + e.message + " (" + (e.number >>> 0).toString(16) + ")"); return undefined; } }

var fso = new ActiveXObject("Scripting.FileSystemObject");
var wsh = new ActiveXObject("WScript.Shell");
var shell = new ActiveXObject("Shell.Application");
var base = fso.GetSpecialFolder(2).Path + "\\sg-shellapp";
if (fso.FolderExists(base)) fso.DeleteFolder(base, true);
fso.CreateFolder(base);
var f = shell.NameSpace(base);

attempt(function () { f.NewFolder("sub"); });
check(fso.FolderExists(base + "\\sub"), "Folder.NewFolder makes a folder");
var sub = shell.NameSpace(base + "\\sub");

var t = fso.CreateTextFile(base + "\\a.txt"); t.Write("0123456789"); t.Close();
t = fso.CreateTextFile(base + "\\m.txt"); t.Write("m"); t.Close();
attempt(function () { sub.CopyHere(base + "\\a.txt", 16 /* FOF_NOCONFIRMATION */); });
check(fso.FileExists(base + "\\sub\\a.txt") && fso.FileExists(base + "\\a.txt"), "Folder.CopyHere copies a file");
attempt(function () { sub.MoveHere(f.ParseName("m.txt"), 16); });
check(fso.FileExists(base + "\\sub\\m.txt") && !fso.FileExists(base + "\\m.txt"), "Folder.MoveHere moves a FolderItem");

var items = sub.Items();
var names = [];
attempt(function () { for (var e = new Enumerator(items); !e.atEnd(); e.moveNext()) names.push(e.item().Name); });
check(names.length == 2, "For Each over FolderItems (" + names.join(",") + ")");

var item = sub.ParseName("a.txt");
check(attempt(function () { return item.Size; }) == 10, "FolderItem.Size");
var type = attempt(function () { return item.Type; });
check(typeof type == "string" && type.length > 0, "FolderItem.Type: " + type);
var when = attempt(function () { return new Date(item.ModifyDate); });
check(when && Math.abs(when.getTime() - new Date().getTime()) < 600000, "FolderItem.ModifyDate is about now");
attempt(function () { item.ModifyDate = 36893.1278; /* 2001-01-02 03:04 as an OLE date */ });
when = new Date(fso.GetFile(base + "\\sub\\a.txt").DateLastModified);
check(when.getFullYear() == 2001 && when.getMonth() == 0 && when.getDate() == 2, "setting ModifyDate changes the file's time");
check(attempt(function () { return item.ExtendedProperty("System.Size"); }) == 10, "ExtendedProperty(System.Size)");
check(attempt(function () { return sub.GetDetailsOf(null, 0); }) == "Name", "GetDetailsOf(null, 0): the column's name");
check(lower(attempt(function () { return sub.GetDetailsOf(item, 0); })).indexOf("a") == 0, "GetDetailsOf(item, 0): its name");
attempt(function () { item.Name = "b.txt"; });
check(fso.FileExists(base + "\\sub\\b.txt") && !fso.FileExists(base + "\\sub\\a.txt"), "setting FolderItem.Name renames it");

var verbs = attempt(function () { return item.Verbs(); });
var verbnames = [];
attempt(function () { for (var e = new Enumerator(verbs); !e.atEnd(); e.moveNext()) verbnames.push(e.item().Name); });
check(verbs && verbs.Count > 0 && verbnames.length == verbs.Count, "For Each over FolderItemVerbs (" + verbnames.length + ")");

items = sub.Items();
attempt(function () { items.Filter(0x40 /* SHCONTF_NONFOLDERS */, "m*"); });
check(items.Count == 1 && items.Item(0).Name.indexOf("m") == 0, "FolderItems.Filter keeps the matching items");

check(lower(attempt(function () { return sub.ParentFolder.Self.Path; })) == base.toLowerCase(), "Folder.ParentFolder");
check(lower(attempt(function () { return f.ParseName("sub").GetFolder.Self.Path; })) == (base + "\\sub").toLowerCase(), "FolderItem.GetFolder");

// a shortcut
t = fso.CreateTextFile(base + "\\run.cmd"); t.WriteLine("echo ran> \"" + base + "\\ran.txt\""); t.Close();
var lnk = wsh.CreateShortcut(base + "\\s.lnk");
lnk.TargetPath = base + "\\run.cmd"; lnk.Save();
var link = attempt(function () { return f.ParseName("s.lnk").GetLink; });
attempt(function () { link.Description = "a description"; link.Arguments = "/x"; link.WorkingDirectory = base; link.Save(); });
var again = attempt(function () { return f.ParseName("s.lnk").GetLink; });
check(again && attempt(function () { return again.Description == "a description" && again.Arguments == "/x"; }),
      "ShellLinkObject: set and Save(), read again from the file");
check(attempt(function () { return link.Description; }) == "a description", "ShellLinkObject.Description reads back");
check(lower(attempt(function () { return link.Target.Path; })) == (base + "\\run.cmd").toLowerCase(), "ShellLinkObject.Target");

// a verb runs: "open" on a .cmd
attempt(function () { f.ParseName("run.cmd").InvokeVerb("open"); });
for (var i = 0; i < 100 && !fso.FileExists(base + "\\ran.txt"); i++) WScript.Sleep(100);
check(fso.FileExists(base + "\\ran.txt"), "FolderItem.InvokeVerb(\"open\") runs it");
WScript.Sleep(500);
try { fso.DeleteFolder(base, true); } catch (e) {}


WScript.Echo("RESULT: " + (failures ? "FAIL" : "PASS"));
WScript.Quit(failures ? 1 : 0);
