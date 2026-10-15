' vbscript For loop with a failing expression, run by test/fastvbfor-gate.sh
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

Dim x, y
On Error Resume Next

x = 6: y = 0
For x = 1 / 0 To 100
    Check Err.Number = 11, "from fails: Err.Number is 11 in the body, got " & Err.Number
    Check x = 6, "from fails: x untouched in the body, got " & x
    y = y + 1
Next
Check y = 1, "from fails: body ran once, y = " & y
Check x = 6, "from fails: x untouched after the loop, got " & x
Check Err.Number = 92, "from fails: Next raises 92, got " & Err.Number

Err.Clear
x = 6: y = 0
For x = 100 To 1 / 0
    Check x = 6, "to fails: the variable is assigned only after all expressions, got " & x
    y = y + 1
Next
Check y = 1, "to fails: body ran once, y = " & y
Check Err.Number = 92, "to fails: Next raises 92, got " & Err.Number
Check Err.Description = "For loop not initialized", "description: " & Err.Description

Err.Clear
y = 0
For x = 1 To 3 Step 1 / 0
    y = y + 1
Next
Check y = 1, "step fails: body ran once, y = " & y
Check Err.Number = 92, "step fails: Next raises 92, got " & Err.Number

Err.Clear
y = 0
For x = 1 To 3
    y = y + x
Next
Check y = 6 And x = 4 And Err.Number = 0, "a normal loop is unchanged: y = " & y & " x = " & x & " err = " & Err.Number

Err.Clear
y = 0
For x = 10 To 1 Step -3
    y = y + x
Next
Check y = 22 And Err.Number = 0, "negative step: y = " & y

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
