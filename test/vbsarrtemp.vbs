' vbscript: an element of an array a function returns, taken at once (patches/sg/9615), run by
' test/vbsarrtemp-gate.sh: Split(s, ",")(1) is how scripts pick a field out of a line; Wine
' handed back a reference into the array it had just freed (Empty or garbage).
Option Explicit
Dim failures, v, line, parts
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Function Pair(a, b)
    Pair = Array(a, b)
End Function

v = Split("a,b,c", ",")(1)
Check v = "b", "v = Split(""a,b,c"", "","")(1) is ""b"" (got """ & v & """)"
Check Split("a,b,c", ",")(2) = "c", "Split(...)(2) in an expression is ""c"""
Check UCase(Split("x.txt", ".")(1)) = "TXT", "an extension: UCase(Split(name, ""."")(1))"
Check Array(7, 8)(1) = 8, "Array(7, 8)(1) is 8"
Check Pair("k", "v")(0) & "=" & Pair("k", "v")(1) = "k=v", "a script function's array result indexed"
line = "user;42;x"
Check CLng(Split(line, ";")(1)) + 1 = 43, "CLng(Split(line, "";"")(1)) + 1 is 43"
parts = Split(line, ";")
parts(0) = "changed"
Check parts(0) = "changed" And Split(line, ";")(0) = "user", "a variable's array keeps working by reference"
Check Join(Array(Split("p q", " ")(1), Split("p q", " ")(0)), "") = "qp", "elements of two temporary arrays at once"

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
