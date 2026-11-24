' vbscript gives an object that wants a site one even when the host's site offers no services
' (patches/sg/9612), run by test/vbsaxsite-gate.sh under cscript: Windows Script Host scripts
' create MSXML documents and HTTP requests all the time; Wine's vbscript refused them
' ("ActiveX component can't create object") because WSH's site is no IServiceProvider.
Option Explicit
Dim failures, progid, o, n
failures = 0

Sub Check(c, what)
    If c Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

For Each progid In Array("MSXML2.DOMDocument", "MSXML2.DOMDocument.3.0", "MSXML2.DOMDocument.6.0", "Microsoft.XMLDOM", _
                         "MSXML2.FreeThreadedDOMDocument.6.0", "MSXML2.XMLHTTP", "MSXML2.XMLHTTP.6.0", _
                         "Microsoft.XMLHTTP", "MSXML2.ServerXMLHTTP", "MSXML2.ServerXMLHTTP.6.0")
    Set o = Nothing
    On Error Resume Next
    Set o = CreateObject(progid)
    Check Err.Number = 0 And Not o Is Nothing, "CreateObject(""" & progid & """) (Err " & Err.Number & ")"
    Err.Clear
    On Error GoTo 0
Next

On Error Resume Next
Set o = Nothing
Set o = CreateObject("MSXML2.DOMDocument.6.0")
On Error GoTo 0
If o Is Nothing Then
    Check False, "no DOMDocument.6.0 to load"
Else
    o.async = False
    Check o.loadXML("<config><item name=""a"">1</item><item name=""b"">2</item></config>"), "loadXML"
    Set n = o.selectSingleNode("//item[@name='b']")
    Check Not n Is Nothing, "selectSingleNode finds the item"
    If Not n Is Nothing Then Check n.text = "2", "the item's text is 2"
    Check o.documentElement.childNodes.length = 2, "two items"
End If

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
