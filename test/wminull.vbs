' wbemdisp method calls (patches/sg/9616), run by test/wminull-gate.sh: Microsoft's own
' Win32_Process.Create samples pass Null for the current directory and a Win32_ProcessStartup
' instance; Wine refused the Null with "Type mismatch" and the instance with a WMI type mismatch.
Option Explicit
Dim failures, wmi, proc, startup, config, rc, pid, fso, nothing_dir, i
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Set fso = CreateObject("Scripting.FileSystemObject")
Set wmi = GetObject("winmgmts:{impersonationLevel=impersonate}!\\.\root\cimv2")
Set proc = wmi.Get("Win32_Process")
Set startup = wmi.Get("Win32_ProcessStartup")
Set config = startup.SpawnInstance_
config.ShowWindow = 0

On Error Resume Next
rc = proc.Create("cmd /c echo one> C:\wminull1.txt", Null, config, pid)
Check Err.Number = 0 And rc = 0 And pid > 0, "Create(cmd, Null, config, pid) (Err " & Err.Number & ", rc " & rc & ")"
Err.Clear
rc = proc.Create("cmd /c echo two> C:\wminull2.txt", "C:\", config, pid)
Check Err.Number = 0 And rc = 0 And pid > 0, "Create(cmd, dir, config, pid) (Err " & Err.Number & ", rc " & rc & ")"
Err.Clear
rc = proc.Create("cmd /c echo two> C:\wminull2.txt", Null, Null, pid)
Check Err.Number = 0 And rc = 0 And pid > 0, "Create(cmd, Null, Null, pid) (Err " & Err.Number & ", rc " & rc & ")"
Err.Clear
nothing_dir = Null
rc = proc.Create("cmd /c echo three> C:\wminull3.txt", nothing_dir, Null, pid)
Check Err.Number = 0 And rc = 0, "a Null held in a variable (Err " & Err.Number & ", rc " & rc & ")"
Err.Clear
On Error GoTo 0
For i = 1 To 50
    If fso.FileExists("C:\wminull1.txt") And fso.FileExists("C:\wminull2.txt") And fso.FileExists("C:\wminull3.txt") Then Exit For
    WScript.Sleep 100
Next
Check fso.FileExists("C:\wminull1.txt") And fso.FileExists("C:\wminull2.txt") And fso.FileExists("C:\wminull3.txt"), "the processes ran"

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
