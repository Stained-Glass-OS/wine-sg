' msado15 batch (patches/sg/2044), run by test/adoconn-gate.sh: the stored properties of an
' ADODB.Connection (ConnectionTimeout, Version, DefaultDatabase, IsolationLevel, Attributes)
' and transactions on a connection that is not open.
Option Explicit
Dim failures, cn
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Set cn = CreateObject("ADODB.Connection")
Check cn.ConnectionTimeout = 15, "ConnectionTimeout defaults to 15"
cn.ConnectionTimeout = 45
Check cn.ConnectionTimeout = 45, "ConnectionTimeout is kept"
Check cn.CommandTimeout = 30, "CommandTimeout is separate"
Check cn.Version = "6.1", "Version"
Check cn.DefaultDatabase = "", "DefaultDatabase is empty"
cn.DefaultDatabase = "sales"
Check cn.DefaultDatabase = "sales", "DefaultDatabase is kept"
Check cn.IsolationLevel = 4096, "IsolationLevel defaults to cursor stability"
cn.IsolationLevel = 1048576
Check cn.IsolationLevel = 1048576, "IsolationLevel is kept"
Check cn.Attributes = 0, "Attributes default to 0"
cn.Attributes = 131072
Check cn.Attributes = 131072, "Attributes are kept"
cn.Attributes = 131072 + 262144
Check cn.Attributes = 393216, "both retaining flags"

On Error Resume Next
Err.Clear
cn.IsolationLevel = 12345
Check Err.Number <> 0, "an unknown isolation level is an error"
Check cn.IsolationLevel = 1048576, "and leaves the level"
Err.Clear
cn.Attributes = 1
Check Err.Number <> 0, "an unknown attribute is an error"
Err.Clear
cn.ConnectionTimeout = -1
Check Err.Number <> 0, "a negative timeout is an error"
Err.Clear
cn.BeginTrans
Check Err.Number <> 0, "BeginTrans needs an open connection"
Err.Clear
cn.CommitTrans
Check Err.Number <> 0, "CommitTrans needs an open connection"
Err.Clear
cn.RollbackTrans
Check Err.Number <> 0, "RollbackTrans needs an open connection"
On Error GoTo 0

WScript.Echo failures & " checks failed"
If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
