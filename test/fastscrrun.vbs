' scrrun batch (patches/sg/3009), run by test/fastscrrun-gate.sh
Dim fails
fails = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        fails = fails + 1
    End If
End Sub

Set fso = CreateObject("Scripting.FileSystemObject")
base = fso.GetSpecialFolder(2).Path & "\fastscrrun"
If fso.FolderExists(base) Then fso.DeleteFolder base, True
fso.CreateFolder base

' text stream position tracking
Set ts = fso.CreateTextFile(base & "\a.txt", True)
v = (ts.Line = 1 And ts.Column = 1)
Check v, "new write stream is at 1,1"
ts.Write "abc"
Check ts.Column = 4, "column after write"
ts.WriteLine "def"
v = (ts.Line = 2 And ts.Column = 1)
Check v, "line after WriteLine"
ts.WriteBlankLines 2
Check ts.Line = 4, "line after WriteBlankLines"
ts.WriteLine "last"
ts.Close

Set ts = fso.OpenTextFile(base & "\a.txt", 1)
Check ts.AtEndOfLine = False, "not at end of line at start"
ts.Skip 2
v = (ts.Column = 3)
Check v, "column after Skip"
v = (ts.Read(1) = "c")
Check v, "Read after Skip"
v = (ts.Read(3) = "def")
Check v, "Read to line end"
v = (ts.AtEndOfLine = True)
Check v, "at end of line"
ts.SkipLine
v = (ts.Line = 2 And ts.Column = 1)
Check v, "line after SkipLine"
Check ts.ReadLine = "", "blank line"
ts.SkipLine
Check ts.ReadLine = "last", "last line"
Check ts.AtEndOfStream, "at end of stream"
ts.Close

' folders
Set f = fso.GetFolder(base)
Check f.IsRootFolder = False, "not a root folder"
Check f.Type = "File Folder", "folder type"
v = (f.Drive.DriveLetter = UCase(Left(base, 1)))
Check v, "folder drive"
v = (f.ParentFolder.Path = fso.GetParentFolderName(base))
Check v, "parent folder"
Check f.Size > 0, "folder size counts its files"
v = ((f.Attributes And 16) = 16)
Check v, "directory attribute"
v = (f.DateCreated > 0 And f.DateLastModified > 0 And f.DateLastAccessed > 0)
Check v, "folder dates"
v = (Len(f.ShortPath) > 0 And Len(f.ShortName) > 0)
Check v, "short names"
v = (fso.GetFolder(Left(base, 3)).IsRootFolder)
Check v, "root folder"
v = (fso.GetFolder(Left(base, 3)).ParentFolder Is Nothing)
Check v, "root has no parent"

Set sub1 = f.SubFolders.Add("child")
v = (fso.FolderExists(base & "\child"))
Check v, "SubFolders.Add creates"
v = (f.SubFolders.Item("child").Name = "child")
Check v, "SubFolders.Item"

' files
Set fl = fso.GetFile(base & "\a.txt")
WScript.Echo "# file type: " & fl.Type
v = (fl.Type = "TXT File")
Check v, "file type"
Check fl.DateLastAccessed > 0, "file last accessed"
v = (fl.Drive.DriveLetter = UCase(Left(base, 1)))
Check v, "file drive"
Check fl.ParentFolder.Path = base, "file parent"
fl.Copy base & "\child\"
v = (fso.FileExists(base & "\child\a.txt"))
Check v, "File.Copy to folder"
fl.Name = "b.txt"
v = (fso.FileExists(base & "\b.txt") And Not fso.FileExists(base & "\a.txt"))
Check v, "File.Name rename"
Check fl.Path = base & "\b.txt", "path follows the rename"
fl.Move base & "\child\"
v = (fso.FileExists(base & "\child\b.txt"))
Check v, "File.Move to folder"
fl.Delete
v = (Not fso.FileExists(base & "\child\b.txt"))
Check v, "File.Delete"

' folder move/copy/delete
sub1.Name = "kid"
v = (fso.FolderExists(base & "\kid"))
Check v, "Folder.Name rename"
sub1.Copy base & "\kid2"
v = (fso.FileExists(base & "\kid2\a.txt"))
Check v, "Folder.Copy copies the tree"
sub1.Move base & "\kid3"
v = (fso.FolderExists(base & "\kid3") And Not fso.FolderExists(base & "\kid"))
Check v, "Folder.Move"
Set k2 = fso.GetFolder(base & "\kid2")
k2.Delete
v = (Not fso.FolderExists(base & "\kid2"))
Check v, "Folder.Delete"
fso.MoveFolder base & "\kid3", base & "\kid4"
v = (fso.FolderExists(base & "\kid4"))
Check v, "FileSystemObject.MoveFolder"

' drives
Set d = fso.GetDrive(Left(base, 1))
v = (d.Path = UCase(Left(base, 2)))
Check v, "drive path"
Check d.RootFolder.IsRootFolder, "drive root folder"
v = (fso.Drives.Item(Left(base, 1)).DriveLetter = UCase(Left(base, 1)))
Check v, "Drives.Item"

fso.DeleteFolder base, True
If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
