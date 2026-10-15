On Error Resume Next
Dim fails
Sub Check(name, cond)
  If cond Then
    WScript.Echo "PASS " & name
  Else
    WScript.Echo "FAIL " & name & " (err " & Err.Number & " " & Err.Description & ")"
    fails = fails + 1
  End If
  Err.Clear
End Sub

Dim ready, completed, lastHres, lastTag, lastClass
Sub SINK_OnObjectReady(obj, ctx)
  ready = ready + 1
  lastClass = obj.Path_.Class
End Sub
Sub SINK_OnCompleted(hres, errObj, ctx)
  completed = completed + 1
  lastHres = hres
  lastTag = ""
  lastTag = ctx.Item("tag").Value
End Sub

Set svc = GetObject("winmgmts:\\.\root\cimv2")
Set sink = WScript.CreateObject("WbemScripting.SWbemSink", "SINK_")
Check "a sink made with a prefix", Err.Number = 0 And Not sink Is Nothing

Set ctx = CreateObject("WbemScripting.SWbemNamedValueSet")
ctx.Add "tag", "abc"

ready = 0 : completed = 0
svc.GetAsync sink, "Win32_Process", 0, Nothing, ctx
Check "GetAsync: SINK_OnObjectReady called with the object", ready = 1 And lastClass = "Win32_Process"
Check "GetAsync: SINK_OnCompleted called, success, with the context", completed = 1 And lastHres = 0 And lastTag = "abc"

n = 0
For Each o In svc.ExecQuery("SELECT * FROM Win32_Process")
  n = n + 1
Next
ready = 0 : completed = 0
svc.ExecQueryAsync sink, "SELECT * FROM Win32_Process", "WQL", 0, Nothing, ctx
Check "ExecQueryAsync: an event for each process", n > 0 And ready = n And completed = 1

WScript.DisconnectObject sink
ready = 0 : completed = 0
svc.GetAsync sink, "Win32_Process", 0, Nothing, ctx
Check "after DisconnectObject the script hears nothing", ready = 0 And completed = 0

Set sink2 = CreateObject("WbemScripting.SWbemSink")
WScript.ConnectObject sink2, "SINK_"
Check "ConnectObject", Err.Number = 0
ready = 0 : completed = 0
svc.GetAsync sink2, "Win32_Process", 0, Nothing, ctx
Check "ConnectObject: events arrive", ready = 1 And completed = 1

Set d = WScript.CreateObject("Scripting.Dictionary", "DICT_")
Check "an object without events with a prefix is fine", Err.Number = 0 And Not d Is Nothing
WScript.Echo "RESULT: " & FinalResult()
Function FinalResult()
  If fails = 0 Then FinalResult = "PASS" Else FinalResult = "FAIL"
End Function
