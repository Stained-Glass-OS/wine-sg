' Compressed (zipped) folders through Shell.Application (patches/sg/1658),
' run by test/zipfolder-gate.sh. Argument 0: the folder with src.zip.
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

Dim sh, fso, d, ns, it, names, sub1, t, nz, out, before
Set sh = CreateObject("Shell.Application")
Set fso = CreateObject("Scripting.FileSystemObject")
d = WScript.Arguments(0)
On Error Resume Next

Set ns = sh.NameSpace(d & "\src.zip")
Call Check(Not ns Is Nothing, "NameSpace of a .zip file is a folder")
If ns Is Nothing Then WScript.Echo "RESULT: FAIL" : WScript.Quit 1
names = ""
For Each it In ns.Items()
    names = names & it.Name & "(" & it.IsFolder & ") "
Next
WScript.Echo "  items: " & names
Call Check(ns.Items().Count = 5, "its items: the top-level files and folders (" & ns.Items().Count & ")")
Call Check(InStr(names, "docs(True)") > 0 And InStr(names, "readme.txt(False)") > 0 And InStr(names, "big64.txt(False)") > 0, "names and folders")
Call Check(InStr(names, "evil") = 0, "an entry climbing out with .. is not offered")
Set it = ns.ParseName("readme.txt")
Call Check(Not it Is Nothing, "ParseName finds a file")
Call Check(it.Path = d & "\src.zip\readme.txt", "an item's path goes through the archive (" & it.Path & ")")
Call Check(ns.GetDetailsOf(it, 4) <> "" And ns.GetDetailsOf(Nothing, 2) = "Compressed size", "details: size, the column names")
Set sub1 = sh.NameSpace(d & "\src.zip\docs\inner")
Call Check(Not sub1 Is Nothing, "a path into the archive's folders parses")
If Not sub1 Is Nothing Then Call Check(sub1.Items().Count = 1 And sub1.Items().Item(0).Name = "deep.txt", "the inner folder's item")

' extract everything, as install scripts do
Set out = sh.NameSpace(d & "\out")
out.CopyHere ns.Items(), 4 + 16
Call Check(Err.Number = 0, "CopyHere of the archive's items: no error")
Err.Clear
Call Check(fso.FileExists(d & "\out\readme.txt") And fso.FileExists(d & "\out\docs\inner\deep.txt") And _
           fso.FileExists(d & "\out\stored.bin") And fso.FileExists(d & "\out\big64.txt"), "the files and folders extracted")
Call Check(Not fso.FileExists(d & "\evil.txt") And Not fso.FileExists(d & "\out\evil.txt"), "the climbing entry is not written")

' one file out of a folder of the archive
sh.NameSpace(d & "\one").CopyHere d & "\src.zip\docs\inner\deep.txt"
Call Check(fso.FileExists(d & "\one\deep.txt"), "CopyHere of one item by its path")

' a ZIP64 archive
sh.NameSpace(d & "\z64").CopyHere sh.NameSpace(d & "\z64.zip").Items(), 16
Call Check(fso.FileExists(d & "\z64\z64.txt"), "a ZIP64 archive's entry")

' a new archive, from the empty one scripts write
Set t = fso.CreateTextFile(d & "\new.zip", True)
t.Write Chr(80) & Chr(75) & Chr(5) & Chr(6) & String(18, Chr(0))
t.Close
Set nz = sh.NameSpace(d & "\new.zip")
nz.CopyHere d & "\out\readme.txt"
nz.CopyHere d & "\out\docs"
Call Check(Err.Number = 0 And nz.Items().Count = 2, "CopyHere adds a file and a folder to a new archive")
Err.Clear
sh.NameSpace(d & "\new.zip\docs").CopyHere d & "\out\stored.bin"
Call Check(Err.Number = 0 And sh.NameSpace(d & "\new.zip\docs").Items().Count = 2, "CopyHere into a folder of the archive keeps what was there")
Err.Clear

' rename and delete inside the archive
Set it = nz.ParseName("readme.txt")
it.Name = "renamed.txt"
Call Check(Not nz.ParseName("renamed.txt") Is Nothing And nz.ParseName("readme.txt") Is Nothing, "renaming an item in the archive")
Err.Clear
nz.ParseName("renamed.txt").InvokeVerb "delete"
Call Check(nz.Items().Count = 1, "deleting an item from the archive")

If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
