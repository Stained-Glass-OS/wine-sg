On Error Resume Next
Dim fails
Sub Check(name, cond)
  If cond Then
    WScript.Echo "PASS " & name
  Else
    WScript.Echo "FAIL " & name & " (err " & Err.Number & " " & Err.Description & ")"
    fails = fails + 1
  End If
  Err.Clear
End Sub

Set sh = CreateObject("WScript.Shell")
Set o = CreateObject("Sg.ScrobjTest")
Check "create", Err.Number = 0 And Not o Is Nothing
Check "variable property get", o.plain = 5
o.plain = 9
Check "variable property put", o.plain = 9
Check "getter with internalName", o.ro = 42
Check "method with internalName", o.add(3, 4) = 7
Check "getResource", o.res() = "Hello <world>"
Check "createComponent", o.comp() = 1
Check "object element", o.fsoName() = "b"

Dim hits : hits = 0
Sub EV_onx()
  hits = hits + 1
End Sub
Set e1 = WScript.CreateObject("Sg.ScrobjTest", "EV_")
Check "a component made with a prefix", Err.Number = 0 And Not e1 Is Nothing
Dim r : r = e1.fire()
Check "fireEvent reaches the handler once", r = 1 And hits = 1
Set e2 = CreateObject("Sg.ScrobjTest")
WScript.ConnectObject e2, "EV_"
Check "ConnectObject", Err.Number = 0
r = e2.fire()
Check "a second component's event", hits = 2
WScript.DisconnectObject e2
r = e2.fire()
Check "after DisconnectObject nothing is heard", hits = 2
r = o.fire()
Check "an unconnected component fires into nothing", r = 1 And hits = 2

Set g = CreateObject("Scriptlet.TypeLib")
Check "typelib defaults", g.Name = "" And g.Path = ""
g.AddURL "C:\fastscrobj.wsc"
g.Path = "C:\fastscrobj.tlb"
g.Name = "SgScrobj"
g.Doc = "SG scrobj typelib"
g.MajorVersion = 2
g.MinorVersion = 3
g.GUID = "{1e1d9d0c-3b33-4c1a-93c2-8f4f9b2a0e11}"
Check "typelib properties", g.Path = "C:\fastscrobj.tlb" And g.Name = "SgScrobj" And g.Doc = "SG scrobj typelib" And g.GUID = "{1E1D9D0C-3B33-4C1A-93C2-8F4F9B2A0E11}"
g.Write
Check "typelib Write", Err.Number = 0
Set fso = CreateObject("Scripting.FileSystemObject")
Check "typelib file", fso.FileExists("C:\fastscrobj.tlb") And fso.GetFile("C:\fastscrobj.tlb").Size > 100
g.Reset
Check "typelib Reset", g.Path = "" And g.Name = ""
If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
