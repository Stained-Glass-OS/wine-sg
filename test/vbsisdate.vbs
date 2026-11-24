' vbscript IsDate of a string (patches/sg/9618), run by test/vbsisdate-gate.sh: scripts validate
' typed or read dates with IsDate("2001-02-03") before CDate; Wine answered False for every string.
Option Explicit
Dim failures, s
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

SetLocale 1033
For Each s In Array("2001-02-03", "Feb 3, 2001", "3 Feb 2001", "2/3/2001", "12:30", "2001/02/03 10:00", "  2001-02-03  ")
    Check IsDate(s), "IsDate(""" & s & """) is True"
    Check VarType(CDate(s)) = vbDate, "CDate(""" & s & """) converts"
Next
For Each s In Array("x", "4", "", "2/30/2001", "13/13/2001", "Monday")
    Check Not IsDate(s), "IsDate(""" & s & """) is False"
Next
Check Not IsDate(Null) And Not IsDate(Empty) And Not IsDate(4) And IsDate(#1/1/2000#), "Null, Empty, 4 and a date literal as before"
s = "2001-02-03"
Check IsDate(s), "a string held in a variable"

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
