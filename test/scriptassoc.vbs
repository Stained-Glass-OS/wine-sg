' .js and .wsf files open with the script host (patches/sg/9622), run by test/scriptassoc-gate.sh:
' double-clicking (ShellExecute "open") a .js or .wsf runs it with wscript, as Windows does; Wine
' had no open verb for JSFile and no WSFFile at all.
Option Explicit
Dim failures, sh, fso, f, i, v
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Function Reg(key)
    Reg = ""
    On Error Resume Next
    Reg = sh.RegRead(key)
    On Error GoTo 0
End Function

Sub RunAndWait(file, marker)
    If fso.FileExists(marker) Then fso.DeleteFile marker
    On Error Resume Next
    sh.Run """" & file & """", 0, True
    v = Err.Number
    On Error GoTo 0
    For i = 1 To 100
        If fso.FileExists(marker) Then Exit For
        WScript.Sleep 100
    Next
End Sub

Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
Check Reg("HKCR\.wsf\") = "WSFFile", ".wsf is a WSFFile"
Check InStr(LCase(Reg("HKCR\WSFFile\Shell\Open\Command\")), "wscript.exe") > 0, "WSFFile opens with wscript"
Check InStr(LCase(Reg("HKCR\WSFFile\Shell\Open2\Command\")), "cscript.exe") > 0, "WSFFile open2 is cscript"
Check InStr(LCase(Reg("HKCR\JSFile\Shell\Open\Command\")), "wscript.exe") > 0, "JSFile opens with wscript"
Check InStr(LCase(Reg("HKCR\JSFile\Shell\Open2\Command\")), "cscript.exe") > 0, "JSFile open2 is cscript"
Check InStr(LCase(Reg("HKCR\VBSFile\Shell\Open\Command\")), "wscript.exe") > 0, "VBSFile still opens with wscript"

Set f = fso.CreateTextFile("C:\assoc.js", True)
f.WriteLine "var fso = new ActiveXObject(""Scripting.FileSystemObject"");"
f.WriteLine "fso.CreateTextFile(""C:\\assoc-js.done"", true).Close();"
f.Close
RunAndWait "C:\assoc.js", "C:\assoc-js.done"
Check fso.FileExists("C:\assoc-js.done"), "opening a .js runs it (Run error " & v & ")"

Set f = fso.CreateTextFile("C:\assoc.wsf", True)
f.WriteLine "<job><script language=""VBScript"">"
f.WriteLine "CreateObject(""Scripting.FileSystemObject"").CreateTextFile(""C:\assoc-wsf.done"", True).Close"
f.WriteLine "</script></job>"
f.Close
RunAndWait "C:\assoc.wsf", "C:\assoc-wsf.done"
Check fso.FileExists("C:\assoc-wsf.done"), "opening a .wsf runs it (Run error " & v & ")"

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
