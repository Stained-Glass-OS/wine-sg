' Scripting.FileSystemObject's File, Folder and Drive objects (patches/sg/1649),
' run by test/fsoitems-gate.sh under cscript.
Option Explicit
Dim failures : failures = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub
Sub Report()
    If Err.Number <> 0 Then WScript.Echo "  error: " & Err.Description & " (" & Hex(Err.Number) & ")"
    Err.Clear
End Sub
Sub WriteFile(path, text)
    Dim t : Set t = fso.CreateTextFile(path, True) : t.Write text : t.Close
End Sub

Dim fso, base, f, sub1, fi, d, s, n
Set fso = CreateObject("Scripting.FileSystemObject")
base = fso.GetSpecialFolder(2).Path & "\sg-fso"
On Error Resume Next
If fso.FolderExists(base) Then fso.DeleteFolder base, True
fso.CreateFolder base
fso.CreateFolder base & "\inner"
WriteFile base & "\a.txt", "0123456789"
WriteFile base & "\b.dat", "01234567890123456789"
WriteFile base & "\inner\c.txt", "01234"

Set f = fso.GetFolder(base)
s = f.Size : Report
Check s = 35, "Folder.Size counts everything in it (" & s & ")"
Check f.Type = "File folder", "Folder.Type (" & f.Type & ")"
Check Year(f.DateCreated) = Year(Now) And Year(f.DateLastModified) = Year(Now) And Year(f.DateLastAccessed) = Year(Now), "Folder dates" : Report
Call Check((f.Attributes And 16) = 16, "Folder.Attributes has Directory") : Report
f.Attributes = 2 : Report
Call Check((fso.GetFolder(base).Attributes And 2) = 2, "setting Folder.Attributes (hidden)")
f.Attributes = 0
Check f.IsRootFolder = False And fso.GetFolder("C:\").IsRootFolder = True, "IsRootFolder" : Report
Check LCase(f.ParentFolder.Path) = LCase(fso.GetSpecialFolder(2).Path), "Folder.ParentFolder" : Report
Check f.Drive.DriveLetter = "C" And f.Drive.Path = "C:", "Folder.Drive" : Report
Check Len(f.ShortPath) > 0 And Len(f.ShortName) > 0, "ShortPath and ShortName (" & f.ShortName & ")" : Report
Set sub1 = f.SubFolders.Add("added") : Report
Check fso.FolderExists(base & "\added") And sub1.Name = "added", "SubFolders.Add"
Check f.SubFolders.Item("inner").Name = "inner", "SubFolders.Item by name" : Report
Check f.Files.Item("b.dat").Size = 20, "Files.Item by name" : Report

Set fi = fso.GetFile(base & "\a.txt")
Check Len(fi.Type) > 0, "File.Type (" & fi.Type & ")" : Report
Check LCase(fi.ParentFolder.Path) = LCase(base), "File.ParentFolder" : Report
Check fi.Drive.Path = "C:", "File.Drive" : Report
Check Year(fi.DateLastAccessed) = Year(Now), "File.DateLastAccessed" : Report
Check Len(fi.ShortName) > 0 And InStr(fi.ShortPath, "\") > 0, "File.ShortName and ShortPath" : Report
fi.Name = "renamed.txt" : Report
Check fso.FileExists(base & "\renamed.txt") And Not fso.FileExists(base & "\a.txt"), "setting File.Name renames it"
fi.Copy base & "\copy.txt" : Report
Check fso.FileExists(base & "\copy.txt"), "File.Copy"
fi.Move base & "\inner\" : Report
Check fso.FileExists(base & "\inner\renamed.txt") And LCase(fi.Path) = LCase(base & "\inner\renamed.txt"), "File.Move into a folder, and its Path follows"
fso.GetFile(base & "\copy.txt").Delete : Report
Check Not fso.FileExists(base & "\copy.txt"), "File.Delete"

sub1.Name = "added2" : Report
Check fso.FolderExists(base & "\added2"), "setting Folder.Name renames it"
sub1.Copy base & "\added3" : Report
Check fso.FolderExists(base & "\added3"), "Folder.Copy"
fso.GetFolder(base & "\added3").Move base & "\inner\" : Report
Check fso.FolderExists(base & "\inner\added3"), "Folder.Move into a folder"
sub1.Delete : Report
Check Not fso.FolderExists(base & "\added2"), "Folder.Delete"

WriteFile base & "\x.log", "x" : WriteFile base & "\y.log", "y"
fso.CreateFolder base & "\logs"
fso.MoveFile base & "\*.log", base & "\logs\" : Report
Check fso.FileExists(base & "\logs\x.log") And fso.FileExists(base & "\logs\y.log"), "MoveFile with wildcards into a folder"
fso.MoveFolder base & "\logs", base & "\logs2" : Report
Check fso.FolderExists(base & "\logs2") And Not fso.FolderExists(base & "\logs"), "MoveFolder"

Set d = fso.Drives.Item("C") : Report
Check d.Path = "C:" And d.RootFolder.Path = "C:\" And d.ShareName = "", "Drives.Item, Path, RootFolder, ShareName" : Report
Set d = fso.GetDrive("c:")
Check d.IsReady And d.TotalSize > 0, "GetDrive" : Report

fso.DeleteFolder base, True
If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
WScript.Quit failures
