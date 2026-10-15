' vbscript For Each over a non-collection, run by test/fastvbenum-gate.sh
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

Dim x, y, n
On Error Resume Next
y = False
Err.Clear
For Each x In Empty
    y = True
Next
Check Err.Number = 451, "For Each over Empty: Object not a collection (451), got " & Err.Number
Check y, "On Error Resume Next runs the body once"

Err.Clear
n = 0
For Each x In 5
    n = n + 1
Next
Check Err.Number = 451, "For Each over a number: 451, got " & Err.Number

Err.Clear
For Each x In "text"
Next
Check Err.Number = 451, "For Each over a string: 451, got " & Err.Number

Err.Clear
For Each x In Array(1, 2, 3)
    n = n + x
Next
Check Err.Number = 0 And n = 7, "an array still enumerates (n=" & n & ")"

Err.Clear
For Each x In CreateObject("Scripting.Dictionary")
Next
Check Err.Number = 0, "a Dictionary still enumerates, got " & Err.Number

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
