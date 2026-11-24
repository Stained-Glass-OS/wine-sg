' vbscript: a call statement whose first argument starts with a bracketed expression
' (patches/sg/9617), run by test/vbsparenarg-gate.sh: WScript.Echo (a) & " items" and
' MsgBox (n) + 1 are valid VBScript; Wine stopped the whole script with a syntax error.
' Each case is compiled with Execute so one failure does not hide the others.
Option Explicit
Dim failures, got, n
failures = 0
n = 3

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Sub Keep(v)
    got = v
End Sub

Sub Two(a, b)
    got = a & "|" & b
End Sub

Sub Try(code, want)
    got = Empty
    On Error Resume Next
    Execute code
    If Err.Number <> 0 Then
        Check False, code & " (error " & Err.Number & ")"
    Else
        Check got = want, code & " gives """ & got & """"
    End If
    Err.Clear
End Sub

Try "Keep (1) & ""x""", "1x"
Try "Keep (""Total: "") & n", "Total: 3"
Try "Keep (n) + 1", 4
Try "Keep (n) * (n)", 9
Try "Keep (n) Mod 2", 1
Try "Keep (n) And 1", 1
Try "Keep (n) - 1", 2
Try "Keep (""a"") & (""b"")", "ab"
Try "Keep ((n + 1) * 2) & ""!""", "8!"
Try "Keep (""(a)"") & "")""", "(a))"
Try "If n = 3 Then Keep (n) & "" then"" Else Keep (n) & "" else""", "3 then"
Try "Two (1), 2", "1|2"
Try "Keep (5)", 5
Try "Keep(6)", 6
Try "Call Keep(7)", 7
Try "Keep 8", 8

'' an expression (Eval) is no call statement: a bracket after a name is its argument list
Check Eval("IsEmpty(got) & "","" & Len(""ab"")") = "False,2", "Eval(""IsEmpty(got) & "","" & Len(""ab"")"")"

' the parentheses rule of call statements is unchanged
On Error Resume Next
Execute "Two (1, 2)"
Check Err.Number = 1044, "Two (1, 2) is still error 1044 (got " & Err.Number & ")"
Err.Clear
On Error GoTo 0

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
