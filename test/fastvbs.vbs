' vbscript batch (patches/sg/3020), run by test/fastvbs-gate.sh
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

Set wsh = CreateObject("WScript.Shell")
Check TypeName(wsh) = "IWshShell3", "Set WSH = ... declares a variable that hides the host item"
Check wsh.ExpandEnvironmentStrings("%OS%") <> "%OS%", "the new variable works"
Check LCase(TypeName(WScript)) = "iwshscript" Or TypeName(WScript) <> "", "WScript is still there"

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
