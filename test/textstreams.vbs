' Text streams of Scripting.FileSystemObject (patches/sg/1648), run by
' test/textstreams-gate.sh under cscript with "piped input" on its stdin.
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

Dim fso, sh, path, ts, s, ex, lines, out, inp
Set fso = CreateObject("Scripting.FileSystemObject")
Set sh = CreateObject("WScript.Shell")
path = fso.GetSpecialFolder(2).Path & "\sg-streams.txt"
On Error Resume Next

' writing: Line and Column follow
Set ts = fso.CreateTextFile(path, True)
ts.WriteLine "first"
Check ts.Line = 2 And ts.Column = 1, "writing: Line and Column after WriteLine (" & ts.Line & "," & ts.Column & ")"
ts.Write "abc"
Check ts.Line = 2 And ts.Column = 4, "... after Write (" & ts.Line & "," & ts.Column & ")"
ts.WriteBlankLines 2 : Report
Check ts.Line = 4 And ts.Column = 1, "... after WriteBlankLines 2 (" & ts.Line & "," & ts.Column & ")"
ts.WriteLine "last"
ts.Close

' reading
Set ts = fso.OpenTextFile(path)
Check ts.ReadLine = "first" And ts.Line = 2 And ts.Column = 1, "ReadLine, then Line 2, Column 1"
Check ts.AtEndOfLine = False, "AtEndOfLine before the text"
Check ts.Read(1) = "a" And ts.Column = 2, "Read(1), then Column 2"
ts.Skip 2 : Report
Check ts.AtEndOfLine = True And ts.Column = 4, "Skip 2 reaches the end of the line"
ts.SkipLine : Report
Check ts.Line = 3, "SkipLine"
ts.SkipLine
s = ts.ReadLine : Report
Check s = "last" And ts.AtEndOfStream, "the last line, and the end"
ts.Close
fso.DeleteFile path

' a program's output
Set ex = sh.Exec("cmd.exe /c echo hello& echo world& echo third")
s = ex.StdOut.ReadLine : Report
Check s = "hello", "Exec(...).StdOut.ReadLine (" & s & ")"
lines = 0
Do While Not ex.StdOut.AtEndOfStream
    s = ex.StdOut.ReadLine
    lines = lines + 1
Loop
Report
Check lines = 2 And s = "third", "... AtEndOfStream waits for the rest, and ends (" & lines & " more)"
Set ex = sh.Exec("cmd.exe /c echo all of it")
s = ex.StdOut.ReadAll : Report
Check InStr(s, "all of it") = 1, "Exec(...).StdOut.ReadAll"

' the standard streams
Set out = fso.GetStandardStream(1) : Report
out.WriteLine "standard output line" : Report
Set inp = fso.GetStandardStream(0) : Report
s = inp.ReadLine : Report
Check s = "piped input", "GetStandardStream(0).ReadLine reads the standard input (" & s & ")"

If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
WScript.Quit failures
