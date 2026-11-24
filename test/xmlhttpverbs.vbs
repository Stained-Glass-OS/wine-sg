' msxml3 XMLHTTP / ServerXMLHTTP send any HTTP method (patches/sg/9613), run by
' test/xmlhttpverbs-gate.sh against a local echo server: REST scripts PATCH a resource,
' WebDAV scripts MKCOL/PROPPATCH, OPTIONS asks what a server allows.
Option Explicit
Dim failures, base, progid, verb, x, ok
failures = 0
base = WScript.Arguments(0)

Sub Check(c, what)
    If c Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

For Each progid In Array("MSXML2.XMLHTTP", "MSXML2.ServerXMLHTTP")
    For Each verb In Array("PATCH", "OPTIONS", "MKCOL", "PROPPATCH", "get", "DELETE")
        Set x = CreateObject(progid)
        On Error Resume Next
        x.open verb, base & "/item/7", False
        ok = (Err.Number = 0)
        Err.Clear
        If ok Then
            x.setRequestHeader "Content-Type", "application/json"
            x.send "{""name"":""new""}"
            ok = (Err.Number = 0)
        End If
        On Error GoTo 0
        If ok Then
            If UCase(verb) = "GET" Then
                Check x.status = 200 And Left(x.responseText, 4) = "GET ", progid & " " & verb & " is sent as GET (" & x.status & " " & x.responseText & ")"
            ElseIf verb = "OPTIONS" Or verb = "DELETE" Or verb = "MKCOL" Then
                Check x.status = 200 And Left(x.responseText, Len(verb) + 1) = verb & " ", progid & " " & verb & " reaches the server (" & x.status & " " & x.responseText & ")"
            Else
                Check x.status = 200 And x.responseText = verb & " /item/7 {""name"":""new""}", progid & " " & verb & " reaches the server with its body (" & x.status & " " & x.responseText & ")"
            End If
        Else
            Check False, progid & " " & verb & " open/send failed"
        End If
    Next
    Set x = CreateObject(progid)
    On Error Resume Next
    x.open "BAD VERB", base & "/item/7", False
    Check Err.Number <> 0, progid & " a method with a space is refused"
    Err.Clear
    On Error GoTo 0
Next

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
