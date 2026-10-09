' Eval, Execute, ExecuteGlobal and GetRef (patches/sg/1652), run by
' test/vbsdyncode-gate.sh under cscript. These were E_NOTIMPL stubs.
Dim failures : failures = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub
Function ErrOf()
    ErrOf = Err.Number
    Err.Clear
End Function

Dim g : g = 10
Call Check(Eval("g * 2 + 1") = 21, "Eval of an expression with a global")
Call Check(Eval("g = 10") = True, "Eval: = compares")

Dim inF
Function F(a)
    Dim loc : loc = 3
    Execute "loc = loc + a : madeHere = 7 : Dim d2 : d2 = 4 : F = loc * 100"
    inF = loc & " " & madeHere & " " & d2 & " " & Eval("a + loc")
End Function
Call Check(F(5) = 800, "Execute in a function sets its return value")
Call Check(inF = "8 7 4 13", "Execute and Eval see and make the function's own variables (" & inF & ")")
Call Check(IsEmpty(madeHere), "... which are not global")

Execute "Dim gx : gx = 42"
Call Check(gx = 42, "Execute at the global level declares globals")
ExecuteGlobal "Function Twice(x) : Twice = x * 2 : End Function"
Call Check(Twice(21) = 42, "ExecuteGlobal declares a function")

Dim arrInfo
Sub Local2
    ExecuteGlobal "gy = 9"
    Execute "Dim arr(2) : arr(1) = ""z"""
    arrInfo = UBound(arr) & arr(1)
End Sub
Local2
Call Check(gy = 9, "ExecuteGlobal from a procedure makes a global")
Call Check(arrInfo = "2z", "Execute declares a fixed array in a procedure")

Class Counter
    Public v
    Function Bump
        Execute "v = v + 1"
        Bump = Eval("Me.v")
    End Function
End Class
Dim o : Set o = New Counter : o.v = 1
Call Check(o.Bump() = 2, "Execute and Eval in a class method see its members and Me")

Sub Nest
    Dim depth : depth = 1
    Execute "Execute ""depth = depth + 1"""
    Call Check(depth = 2, "Execute inside Execute runs in the same procedure")
End Sub
Nest

On Error Resume Next
Execute "x = (1"
Call Check(ErrOf() = 1002, "a syntax error in Execute is error 1002, raised where On Error handles it")
Dim n : n = Eval("1/0")
Call Check(ErrOf() = 11, "an error inside Eval is raised in the caller (division by zero, 11)")
Execute "Dim gx"
Call Check(ErrOf() <> 0, "redeclaring a global by Execute: Name redefined")
On Error GoTo 0

Dim r : Set r = GetRef("Twice")
Call Check(r(8) = 16, "GetRef of a function: calling the object calls it")
Dim said
Sub Hello(who) : said = "hello " & who : End Sub
Set r = GetRef("Hello")
r "world"
Call Check(said = "hello world", "GetRef of a Sub")
On Error Resume Next
Set r = GetRef("NoSuchProcedure")
Call Check(ErrOf() = 5, "GetRef of no procedure: invalid procedure call")
On Error GoTo 0

If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
