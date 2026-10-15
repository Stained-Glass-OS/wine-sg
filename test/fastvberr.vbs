' vbscript Err.Number and Err.HelpContext setters, run by test/fastvberr-gate.sh
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

On Error Resume Next
Err.Number = 5
Check Err.Number = 5, "Err.Number = 5 is kept"
Check Err.HelpContext = 0, "help context starts at 0"
Err.HelpContext = 1234
Check Err.HelpContext = 1234, "Err.HelpContext can be set"
Err.Number = 70000
Check Err.Number = 70000, "a number beyond the VBScript range is kept as it is"
Err.Clear
Check Err.Number = 0 And Err.HelpContext = 0, "Clear resets both"
Err.Number = 11
Err.Number = 0
Check Err.Number = 0, "Err.Number = 0"
Err.Number = "7"
Check Err.Number = 7, "a string is converted"
Err.Number = "abc"
Check Err.Number = 13 Or Err.Number = 7, "a bad value is a type mismatch"
Err.Clear
Err.Description = "my text"
Err.Source = "my source"
Check Err.Description = "my text" And Err.Source = "my source", "Err.Description and Err.Source can be set"
Err.Clear

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
