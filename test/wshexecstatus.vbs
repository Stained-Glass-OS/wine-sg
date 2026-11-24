' WshExec.Status (patches/sg/9010), run by test/wshexecstatus-gate.sh under cscript.
' Windows: WshRunning (0) while the child runs, WshFinished (1) once it has ended -- whatever its exit
' code (ExitCode tells that). Wine said WshFailed (2) for a nonzero exit, so the usual
'   Do Until oExec.Status = 1 : WScript.Sleep 100 : Loop
' never ended.
Option Explicit
Dim failures, checks, sh, ex, n, sawRunning, st
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

Function WaitDone(e)
    Dim k
    k = 0
    Do While e.Status = 0 And k < 300
        WScript.Sleep 50
        k = k + 1
    Loop
    WaitDone = e.Status
End Function

Set sh = CreateObject("WScript.Shell")

' a child that exits 3 at once
Set ex = sh.Exec("cmd.exe /c exit 3")
st = WaitDone(ex)
Check st = 1, "Status of a child that exited 3 is WshFinished (" & st & ")"
Check ex.ExitCode = 3, "ExitCode is 3 (" & ex.ExitCode & ")"

' a child that exits 0
Set ex = sh.Exec("cmd.exe /c exit 0")
st = WaitDone(ex)
Check st = 1, "Status of a child that exited 0 is WshFinished (" & st & ")"
Check ex.ExitCode = 0, "ExitCode is 0 (" & ex.ExitCode & ")"

' a script that runs a while, then exits 3: Running first, then Finished; the Windows loop idiom ends
Set ex = sh.Exec("cscript.exe //nologo C:\wshexec-child.vbs")
sawRunning = (ex.Status = 0)
Check sawRunning, "Status is WshRunning while the child runs (" & ex.Status & ")"
n = 0
Do Until ex.Status = 1 Or n >= 300
    WScript.Sleep 50
    n = n + 1
Loop
Check n < 300 And ex.Status = 1, "Do Until oExec.Status = 1 ends (" & ex.Status & ", " & n & " waits)"
Check ex.ExitCode = 3, "the long child's ExitCode is 3 (" & ex.ExitCode & ")"
Check InStr(ex.StdOut.ReadAll, "child done") > 0, "its output can still be read"

' a big exit code (negative as a Long)
Set ex = sh.Exec("cmd.exe /c exit -1")
st = WaitDone(ex)
Check st = 1, "Status of a child that exited -1 is WshFinished (" & st & ")"

' a terminated child is finished too
Set ex = sh.Exec("cscript.exe //nologo C:\wshexec-child.vbs")
ex.Terminate
st = WaitDone(ex)
Check st = 1, "Status after Terminate is WshFinished (" & st & ")"

If failures = 0 Then
    WScript.Echo "RESULT: PASS (" & checks & " checks)"
Else
    WScript.Echo "RESULT: FAIL (" & failures & " of " & checks & ")"
End If
