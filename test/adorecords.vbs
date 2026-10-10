' msado15 batch (patches/sg/2043), run by test/adorecords-gate.sh: an in-memory ADODB.Recordset
' driven the way a script does: position, Move, Find, Sort, GetRows, GetString, Delete, paging,
' Source, Status, Update with fields.
Option Explicit
Dim failures, rs, v, s, n
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Sub Add(id, nm, score)
    rs.AddNew
    rs.Fields("id").Value = id
    rs.Fields("name").Value = nm
    rs.Fields("score").Value = score
    rs.Update
End Sub

Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "id", 3
rs.Fields.Append "name", 202, 20
rs.Fields.Append "score", 5
rs.LockType = 3
rs.Open
Check rs.LockType = 3, "LockType is kept"
Add 1, "alpha", 3.5
Add 2, "beta", 1.25
Add 3, "gamma", 9
Add 4, "alpine", 2
Check rs.RecordCount = 4, "four rows"

' position
rs.MoveFirst
Check rs.AbsolutePosition = 1, "AbsolutePosition of the first row is 1"
rs.AbsolutePosition = 3
Check rs.Fields("id").Value = 3, "setting AbsolutePosition moves there"
rs.MoveLast
Check rs.AbsolutePosition = 4, "the last row is 4"
rs.MoveNext
Check rs.EOF And rs.AbsolutePosition = -3, "past the last row is adPosEOF"
rs.MoveFirst
rs.MovePrevious
Check rs.BOF And rs.AbsolutePosition = -2, "before the first row is adPosBOF"
On Error Resume Next
Err.Clear
rs.AbsolutePosition = 9
Check Err.Number <> 0, "a position beyond the rows is an error"
On Error GoTo 0

' Move
rs.MoveFirst
rs.Move 2
Check rs.Fields("id").Value = 3, "Move 2 from the first"
rs.Move -1
Check rs.Fields("id").Value = 2, "Move -1"
rs.Move 2, 1
Check rs.Fields("id").Value = 3, "Move 2 from the first row (adBookmarkFirst)"
rs.Move -1, 2
Check rs.Fields("id").Value = 3, "Move -1 from the last row (adBookmarkLast)"
rs.Move 10
Check rs.EOF, "Move beyond the end is EOF"
rs.MovePrevious
Check rs.Fields("id").Value = 4, "the row before EOF is the last one (Move stops at EOF)"
rs.MoveFirst
rs.Move -5
Check rs.BOF, "Move before the start is BOF"
rs.MoveNext
Check rs.Fields("id").Value = 1, "the row after BOF is the first one (Move stops at BOF)"

' Find
rs.MoveFirst
rs.Find "name = 'beta'"
Check Not rs.EOF And rs.Fields("id").Value = 2, "Find a string"
rs.MoveFirst
rs.Find "score > 3", 1
Check Not rs.EOF And rs.Fields("id").Value = 3, "Find with skip 1 starts at the next row"
rs.MoveFirst
rs.Find "name LIKE 'al*'"
Check rs.Fields("name").Value = "alpha", "Find with LIKE"
rs.Find "name LIKE 'al*'", 1
Check rs.Fields("name").Value = "alpine", "the next LIKE match"
rs.Find "name = 'nobody'"
Check rs.EOF, "no match is EOF"
rs.MoveLast
rs.Find "id < 3", 0, -1
Check rs.Fields("id").Value = 2, "Find backwards"
rs.MoveLast
rs.Find "id > 99", 0, -1
Check rs.BOF, "no match backwards is BOF"
rs.MoveFirst
rs.Find "[score] <= 2"
Check rs.Fields("id").Value = 2, "Find with a number and brackets"
rs.MoveFirst
rs.Find "id <> 1"
Check rs.Fields("id").Value = 2, "Find with <>"

' Sort
rs.Sort = "score DESC"
Check rs.Sort = "score DESC", "Sort reads back"
rs.MoveFirst
Check rs.Fields("name").Value = "gamma", "highest score first"
rs.Sort = "name ASC"
rs.MoveFirst
s = ""
Do While Not rs.EOF
    s = s & rs.Fields("name").Value & ","
    rs.MoveNext
Loop
Check s = "alpha,alpine,beta,gamma,", "sorted by name"
rs.Sort = "id"
rs.MoveFirst
Check rs.Fields("id").Value = 1, "sorted back by id"

' GetRows
rs.MoveFirst
v = rs.GetRows(2)
Check UBound(v, 1) = 2 And UBound(v, 2) = 1, "GetRows(2): three columns, two rows"
Check v(0, 0) = 1 And v(1, 0) = "alpha" And v(2, 1) = 1.25, "GetRows values"
Check rs.Fields("id").Value = 3, "the current row is the next one"
v = rs.GetRows(-1)
Check UBound(v, 2) = 1 And v(1, 1) = "alpine", "GetRows(-1) takes the rest"
Check rs.EOF, "and ends at EOF"
rs.MoveFirst
v = rs.GetRows(1, , "name")
Check UBound(v, 1) = 0 And v(0, 0) = "alpha", "GetRows of one named field"
v = rs.GetRows(1, , Array("score", "id"))
Check v(0, 0) = 1.25 And v(1, 0) = 2, "GetRows of an array of fields, in that order"

' GetString
rs.MoveFirst
s = rs.GetString(2, 2, "|", ";")
Check s = "1|alpha|3.5;2|beta|1.25;", "GetString with delimiters"
Check rs.Fields("id").Value = 3, "GetString moves past what it returned"
rs.MoveFirst
s = rs.GetString(2, 1)
Check s = "1" & vbTab & "alpha" & vbTab & "3.5" & vbCr, "GetString with the default delimiters"
rs.MoveFirst
rs.Fields("name").Value = Null
rs.Update
s = rs.GetString(2, 1, "|", ";", "<null>")
Check s = "1|<null>|3.5;", "GetString writes null as asked"
rs.Fields("name").Value = "alpha"
rs.Update

' paging, source
Check rs.PageSize = 10, "the default page size is 10"
rs.PageSize = 3
Check rs.PageCount = 2, "four rows in pages of three are two pages"
rs.MoveFirst
Check rs.AbsolutePage = 1, "on page 1"
rs.MoveLast
Check rs.AbsolutePage = 2, "the last row is on page 2"
rs.AbsolutePage = 2
Check rs.Fields("id").Value = 4, "page 2 starts at the fourth row"
rs.Source = "SELECT 1"
Check rs.Source = "SELECT 1", "Source is kept"

' Update with a field, Status of a new row
rs.MoveFirst
rs.Update "name", "zulu"
Check rs.Fields("name").Value = "zulu", "Update with a field and a value"
rs.AddNew
Check rs.Status = 1, "Status of a new row is adRecNew"
rs.Fields("id").Value = 5
rs.Update
Check rs.RecordCount = 5, "five rows"

' Delete
rs.MoveFirst
rs.Delete 1
Check rs.RecordCount = 4, "Delete the current row"
rs.MoveFirst
Check rs.Fields("id").Value = 2, "the next row is first now"
rs.Delete 3
Check rs.RecordCount = 0, "Delete all"

Dim ro
Set ro = CreateObject("ADODB.Recordset")
ro.Fields.Append "id", 3
ro.Open
On Error Resume Next
ro.AddNew
ro.Fields("id").Value = 1
ro.Update
Err.Clear
ro.Delete 1
Check Err.Number <> 0, "Delete in a read-only recordset is an error"
On Error GoTo 0

WScript.Echo failures & " checks failed"
If failures = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
