' Windows Script Host argument collections (patches/sg/9619), run by test/wshargs-gate.sh as
'   cscript //nologo wshargs.vbs first /Path:C:\x\y /v second "with space" /n:
' For Each over WScript.Arguments, WScript.Arguments.Named and .Unnamed (WSH 5.6) are how scripts
' read their command line; Wine had none of them (errors 451 and 438).
Option Explicit
Dim failures, checks, a, x, s, n, e
failures = 0
checks = 0

Sub Check(ok, what)
    checks = checks + 1
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Set a = WScript.Arguments
Check a.Count = 6 And a.length = 6, "six arguments (" & a.Count & ")"
s = ""
On Error Resume Next
For Each x In a : s = s & "[" & x & "]" : Next
e = Err.Number
Err.Clear
Check e = 0 And s = "[first][/Path:C:\x\y][/v][second][with space][/n:]", "For Each over WScript.Arguments (Err " & e & ": " & s & ")"
n = -1 : n = a.Named.Count
Check n = 3, "Named.Count is 3 (" & n & ")"
n = -1 : n = a.Unnamed.Count
Check n = 3, "Unnamed.Count is 3 (" & n & ")"
Check a.Named("path") = "C:\x\y", "Named(""path"") ignores case: C:\x\y"
Check a.Named.Item("PATH") = "C:\x\y", "Named.Item(""PATH"")"
Check a.Named.Exists("V") And a.Named.Exists("n") And Not a.Named.Exists("first"), "Named.Exists"
Check IsEmpty(a.Named("v")), "a switch without a value reads Empty"
Check a.Named("n") = "", "/n: reads an empty string"
Check IsEmpty(a.Named("missing")), "a name not given reads Empty"
Check a.Unnamed(0) = "first" And a.Unnamed.Item(1) = "second" And a.Unnamed(2) = "with space", "Unnamed(i)"
s = ""
For Each x In a.Named : s = s & x & ";" : Next
Check s = "Path;v;n;", "For Each over Named gives the names (" & s & ")"
s = ""
For Each x In a.Unnamed : s = s & x & ";" : Next
Check s = "first;second;with space;", "For Each over Unnamed (" & s & ")"
x = a.Unnamed(3)
Check Err.Number <> 0, "Unnamed(3) is out of range (Err " & Err.Number & ")"
Err.Clear
Check a(1) = "/Path:C:\x\y" And a.Item(0) = "first", "WScript.Arguments(i) unchanged"
On Error GoTo 0
' a check whose expression raised an error was skipped by On Error Resume Next
If checks <> 15 Then
    WScript.Echo "FAIL  only " & checks & " of 15 checks ran (an error skipped the others)"
    failures = failures + 1
End If

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
