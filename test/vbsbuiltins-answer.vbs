' Answers a dialog for test/vbsbuiltins.vbs: waits for a window whose title
' starts with argument 0, brings it to the front and types argument 1.
Dim sh, i
Set sh = CreateObject("WScript.Shell")
For i = 1 To 150
    If sh.AppActivate(WScript.Arguments(0)) Then
        WScript.Sleep 300
        sh.SendKeys WScript.Arguments(1), True
        WScript.Quit 0
    End If
    WScript.Sleep 100
Next
WScript.Quit 1
