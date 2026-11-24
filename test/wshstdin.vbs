' WScript.StdIn read in a loop (patches/sg/9623), run by test/wshstdin-gate.sh with its input
' redirected from a file: the filter loop scripts use,
'   Do Until WScript.StdIn.AtEndOfStream : line = WScript.StdIn.ReadLine : ... : Loop
' read nothing under Wine (each WScript.StdIn was a new stream, the first one's buffer lost).
Option Explicit
Dim lines, n, sh, x, out
n = 0 : lines = ""
Do Until WScript.StdIn.AtEndOfStream
    lines = lines & "[" & WScript.StdIn.ReadLine & "]"
    n = n + 1
Loop
If n = 3 And lines = "[one][two][three]" Then
    WScript.Echo "PASS  the redirected input is read line by line"
Else
    WScript.Echo "FAIL  the redirected input: " & n & " lines " & lines
End If
If WScript.StdIn Is WScript.StdIn Then
    WScript.Echo "PASS  WScript.StdIn is one object"
Else
    WScript.Echo "FAIL  WScript.StdIn is a new object each time"
End If

' a child script fed through WshExec.StdIn
Set sh = CreateObject("WScript.Shell")
Set x = sh.Exec("cscript //nologo C:\wshstdin-child.vbs")
x.StdIn.WriteLine "alpha"
x.StdIn.WriteLine "beta"
x.StdIn.Close
Do While x.Status = 0 : WScript.Sleep 50 : Loop
out = ""
If Not x.StdOut.AtEndOfStream Then out = x.StdOut.ReadAll
If Replace(out, vbCrLf, "|") = "got alpha|got beta|" Then
    WScript.Echo "PASS  a child reads its piped input"
Else
    WScript.Echo "FAIL  the child printed [" & Replace(out, vbCrLf, "|") & "]"
End If
