' WScript.Shell (patches/sg/1647), run by test/wshshell-gate.sh under cscript
' in the shell's desktop; argument: the folder with wshshell-target.exe.
Option Explicit
Dim failures : failures = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub
Sub Report()
    If Err.Number <> 0 Then WScript.Echo "  error: " & Err.Description & " (" & Hex(Err.Number) & ")"
    Err.Clear
End Sub

Dim sh, fso, dir, tmp, envs, i, env, found, n, item, sf, m, path, lnk, again, other, ex, t, keys, act, got, ts
Set sh = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
dir = WScript.Arguments(0)
tmp = fso.GetSpecialFolder(2).Path
On Error Resume Next

envs = Array("Process", "User", "Volatile", "System")
For i = 0 To 3
    Set env = sh.Environment(envs(i)) : Report
    env("SG_WSH_TEST") = "value " & i : Report
    Check env("SG_WSH_TEST") = "value " & i, envs(i) & " environment: a variable set reads back"
    found = False : n = 0
    For Each item In env
        n = n + 1
        If item = "SG_WSH_TEST=value " & i Then found = True
    Next
    Report
    Check found And n = env.Count, envs(i) & " environment: For Each lists it (" & n & " of " & env.Count & ")"
    env.Remove "SG_WSH_TEST" : Report
    Check env("SG_WSH_TEST") = "", envs(i) & " environment: Remove"
Next

Set sf = sh.SpecialFolders
Check sf.Count = 16, "SpecialFolders.Count is 16"
Check sf.Item(4) = sf.Item("Desktop") And sf.Item("Desktop") <> "", "SpecialFolders by position and by name"
Check sf("Templates") <> "" And sf("NoSuchFolder") = "", "Templates; an unknown name is empty"
m = 0
For Each item In sf : m = m + 1 : Next
Report
Check m = 16, "For Each over SpecialFolders"

path = tmp & "\sg-wsh.lnk"
Set lnk = sh.CreateShortcut(path)
lnk.TargetPath = tmp & "\target.txt"
lnk.Description = "a description"
lnk.Hotkey = "CTRL+ALT+K" : Report
lnk.Save : Report
Set again = sh.CreateShortcut(path)
Check again.Description = "a description", "CreateShortcut reads an existing shortcut: Description"
Check again.Hotkey = "ALT+CTRL+K", "Hotkey reads back (" & again.Hotkey & ")"
Check LCase(again.TargetPath) = LCase(tmp & "\target.txt"), "TargetPath reads back"
Check LCase(again.FullName) = LCase(path), "FullName"
Set other = sh.CreateShortcut(tmp & "\other.lnk")
other.Load path : Report
Check other.Description = "a description", "Load"
fso.DeleteFile path

sh.RegWrite "HKCU\Software\SGWshTest\Sub\", ""
sh.RegWrite "HKCU\Software\SGWshTest\Value", 42, "REG_DWORD"
Check sh.RegRead("HKCU\Software\SGWshTest\Value") = 42, "RegWrite and RegRead"
sh.RegDelete "HKCU\Software\SGWshTest\Value" : Report
Err.Clear : t = sh.RegRead("HKCU\Software\SGWshTest\Value")
Check Err.Number <> 0, "RegDelete a value" : Err.Clear
sh.RegDelete "HKCU\Software\SGWshTest\Sub\" : Report
sh.RegDelete "hkcu\Software\SGWshTest\" : Report
Err.Clear : t = sh.RegRead("HKCU\Software\SGWshTest\")
Check Err.Number <> 0, "RegDelete keys (and lower-case root names)" : Err.Clear

Check sh.LogEvent(4, "SG WSH test event") = True, "LogEvent" : Report
Set ex = sh.Exec("cmd.exe /c exit 7")
t = 0
Do While ex.Status = 0 And t < 100 : WScript.Sleep 100 : t = t + 1 : Loop
Check ex.ExitCode = 7, "Exec(...).ExitCode" : Report

keys = tmp & "\sg-keys.txt"
If fso.FileExists(keys) Then fso.DeleteFile keys
sh.Run """" & dir & "\wshshell-target.exe"" """ & keys & """", 1, False
WScript.Sleep 2500
act = False : t = 0
Do While Not act And t < 20
    act = sh.AppActivate("SG Keys Target") : Report
    If Not act Then WScript.Sleep 500
    t = t + 1
Loop
Check act = True, "AppActivate finds the window by the start of its title"
Check sh.AppActivate("No Such Window Title") = False, "AppActivate of no window: false"
WScript.Sleep 500
sh.SendKeys "ab+c{TAB}~{F5}{+}^a+(de)", True : Report
WScript.Sleep 1000
' Alt+F4 closes it, and it closes its file
sh.SendKeys "%{F4}", True : Report
t = 0
Do While t < 50
    If sh.AppActivate("SG Keys Target") = False Then Exit Do
    WScript.Sleep 200 : t = t + 1
Loop
WScript.Sleep 500
got = ""
If fso.FileExists(keys) Then
    Set ts = fso.OpenTextFile(keys)
    If Not ts.AtEndOfStream Then got = ts.ReadAll
    ts.Close
End If
WScript.Echo "  typed: " & got
Check got = "c61;c62;c43;c09;c0d;F5;c2b;c01;c44;c45;", "SendKeys: characters, Shift, Tab, Enter, F5, {+}, Ctrl+A, a group, then Alt+F4"

If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
WScript.Quit failures
