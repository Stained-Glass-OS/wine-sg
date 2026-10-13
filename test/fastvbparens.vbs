' vbscript parentheses in a call statement, run by test/fastvbparens-gate.sh
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
Sub Two(a, b)
    Check a = 1 And b = 2, "Two got its arguments"
End Sub

On Error Resume Next
Execute "Two(1, 2)"
Check Err.Number = 1044, "Two(1, 2) as a statement: error 1044, got " & Err.Number
Check Err.Description = "Cannot use parentheses when calling a Sub", "description: " & Err.Description
Err.Clear
Execute "Call Two(1, 2)"
Check Err.Number = 0, "Call Two(1, 2) works, got " & Err.Number
Err.Clear
Execute "Two 1, 2"
Check Err.Number = 0, "Two 1, 2 works, got " & Err.Number
Err.Clear
Execute "x = 1 +"
Check Err.Number = 1002, "other syntax errors stay 1002, got " & Err.Number
Err.Clear
Execute "WScript.Echo(""single argument in parentheses"")"
Check Err.Number = 0, "one argument in parentheses is fine, got " & Err.Number

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
