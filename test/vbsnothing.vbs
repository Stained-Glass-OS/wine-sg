' vbsnothing.vbs: using Nothing as an object is an error, for test/vbsnothing-gate.sh (Stained Glass OS wine-sg 6035)
On Error Resume Next
Set x = Nothing
y = x(0)
WScript.Echo "call=" & Err.Number : Err.Clear
y = x.Foo
WScript.Echo "member=" & Err.Number : Err.Clear
x.Foo = 1
WScript.Echo "assign=" & Err.Number : Err.Clear
Set x.Bar = Nothing
WScript.Echo "set=" & Err.Number : Err.Clear
x.Baz 1, 2
WScript.Echo "method=" & Err.Number : Err.Clear
WScript.Echo "alive=1"
