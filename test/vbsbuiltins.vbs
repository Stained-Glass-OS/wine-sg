' VBScript built-in functions and statements (patches/sg/1651), run by
' test/vbsbuiltins-gate.sh under cscript in the shell's desktop. Argument 0
' is the folder that holds test/vbsbuiltins-answer.vbs. Each of these was a
' stub (E_NOTIMPL or a FIXME) or crashed.
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
Function ErrOf()
    ErrOf = Err.Number
    Err.Clear
End Function

Dim sh, dir, v, a, n
Set sh = CreateObject("WScript.Shell")
dir = WScript.Arguments(0)
On Error Resume Next

' byte functions
Call Check(LenB("ab") = 4 And LenB(ChrB(65)) = 1, "LenB counts bytes (" & LenB("ab") & ")")
Call Check(AscB(ChrB(200)) = 200, "ChrB/AscB")
Call Check(InStrB("ABC", "B") = 3 And InStrB(2, "ABC", "C") = 5, "InStrB gives a byte position")
Call Check(MidB("ABC", 3, 2) = "B" And LeftB("ABC", 2) = "A" And RightB("ABC", 2) = "C", "MidB/LeftB/RightB")
Call Check(AscW(ChrW(8364)) = 8364 And AscW(ChrW(&HFFFF)) = -1, "AscW/ChrW (AscW is an Integer)")
Call Check(String(3, 65) = "AAA" And String(2, "xyz") = "xx", "String with a character code or a string")

' locale
Call SetLocale(1031)
Call Check(GetLocale() = 1031, "SetLocale/GetLocale")
v = SetLocale("en-us")
Call Check(v = 1031 And GetLocale() = 1033, "SetLocale returns the previous locale and takes a name")

' dates
Call Check(DateValue("2020-03-04 05:06") = #3/4/2020#, "DateValue keeps the date part")
Call Check(TimeValue("2020-03-04 05:06") = #5:06:00 AM#, "TimeValue keeps the time part")
Call Check(DateDiff("d", #1/1/2020#, #3/1/2020#) = 60, "DateDiff days")
Call Check(DateDiff("m", #1/31/2020#, #2/1/2020#) = 1 And DateDiff("yyyy", #12/31/2019#, #1/1/2020#) = 1, "DateDiff months and years count boundaries")
Call Check(DateDiff("ww", #1/4/2020#, #1/5/2020#) = 1 And DateDiff("w", #1/1/2020#, #1/15/2020#) = 2, "DateDiff weeks")
Call Check(DateDiff("h", #1/1/2020#, #1/1/2020 5:00:00 AM#) = 5 And DateDiff("s", #1/1/2020#, #1/1/2020 12:01:00 AM#) = 60, "DateDiff hours and seconds")
Call Check(DateDiff("d", #3/1/2020#, #1/1/2020#) = -60, "DateDiff backwards is negative")
Call Check(DatePart("ww", #12/31/2020#) = 53 And DatePart("w", #1/1/2020#) = 4, "DatePart week and weekday")
Call Check(DatePart("ww", #1/1/2021#, vbMonday, vbFirstFourDays) = 53, "DatePart ISO-style week")
Call Check(DatePart("q", #5/1/2020#) = 2 And DatePart("y", #3/1/2020#) = 61 And DatePart("yyyy", #3/1/2020#) = 2020, "DatePart quarter, day of year, year")

' arrays
Call Check(Join(Array("a", "b", 3), "-") = "a-b-3" And Join(Array("x", "y")) = "x y", "Join")
a = Filter(Array("apple", "pear", "pineapple"), "apple")
Call Check(UBound(a) = 1 And a(1) = "pineapple", "Filter keeps the matches")
Call Check(Join(Filter(Array("apple", "pear"), "APPLE", False, vbTextCompare)) = "pear", "Filter excludes, text compare")
Call Check(TypeName(Array()) = "Variant()" And VarType(Array()) = 8204, "TypeName/VarType of an array")
Dim fixed(2)
fixed(0) = 5 : fixed(2) = "x"
Erase fixed
Call Check(ErrOf() = 0 And UBound(fixed) = 2 And IsEmpty(fixed(0)) And IsEmpty(fixed(2)), "Erase empties a fixed array's elements")
Dim dyn()
n = UBound(dyn)
Call Check(ErrOf() = 9, "UBound of a never-sized dynamic array: error 9")
ReDim dyn(3)
dyn(1) = 7
Erase dyn
n = -1 : n = UBound(dyn)
Call Check(ErrOf() = 9 And n = -1, "Erase frees a dynamic array: UBound raises error 9")
ReDim dyn(1)
Call Check(ErrOf() = 0 And UBound(dyn) = 1, "... and it can be ReDim'd again")
Dim grid(1, 2)
grid(1, 2) = "x"
Erase grid
Call Check(UBound(grid, 2) = 2 And IsEmpty(grid(1, 2)), "Erase of a two-dimensional fixed array")
n = 3
Erase n
Call Check(ErrOf() = 13, "Erase of a scalar: type mismatch")

' escapes, errors
Call Check(Escape("a b&" & ChrW(252) & ChrW(8364)) = "a%20b%26%FC%u20AC", "Escape")
Call Check(Unescape("a%20b%26%FC%u20AC") = "a b&" & ChrW(252) & ChrW(8364), "Unescape")
v = Left("abc", -1)
Call Check(ErrOf() = 5, "Left with a negative length: error 5")
v = StrComp("a", "b", 5)
Call Check(ErrOf() = 5, "StrComp with a bad compare mode: error 5")
Call Check(LoadPicture("") Is Nothing, "LoadPicture of an empty name: Nothing")
Call Check(ErrOf() = 0, "... no error")
Set v = LoadPicture(dir & "\none.bmp")
Call Check(ErrOf() <> 0, "LoadPicture of a missing file raises an error")

' dialogs, answered by vbsbuiltins-answer.vbs
sh.Run "cscript //nologo """ & dir & "\vbsbuiltins-answer.vbs"" ""SG Question"" ""hello{ENTER}""", 0, False
v = InputBox("Your name?", "SG Question", "default")
Call Check(ErrOf() = 0 And v = "hello", "InputBox returns what was typed over the selected default (" & v & ")")
sh.Run "cscript //nologo """ & dir & "\vbsbuiltins-answer.vbs"" ""SG Question"" ""{ESC}""", 0, False
v = InputBox("Your name?", "SG Question", "default")
Call Check(ErrOf() = 0 And v = "", "InputBox cancelled: an empty string")
sh.Run "cscript //nologo """ & dir & "\vbsbuiltins-answer.vbs"" ""SG Notice"" ""{ENTER}""", 0, False
v = MsgBox("Done?", vbOKOnly, "SG Notice", "help.hlp", 10)
Call Check(ErrOf() = 0 And v = vbOK, "MsgBox with a help file and context")

If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
