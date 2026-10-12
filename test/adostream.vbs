' msado15 stream batch (patches/sg/2058), run by test/adostream-gate.sh: ADODB.Stream text
' in its character sets (byte order marks, lines), binary data, files (Open on one,
' LoadFromFile, SaveToFile, Flush, Close writing back) and CopyTo.
Option Explicit
Dim failures, fso, dir, s, d, v, t
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Function Bytes(st)
    ' the bytes of a stream as hex text
    Dim b, i, r, was
    was = st.Type
    st.Position = 0
    st.Type = 1
    b = st.Read(-1)
    r = ""
    If Not IsNull(b) Then
        For i = 1 To LenB(b)
            r = r & Right("0" & Hex(AscB(MidB(b, i, 1))), 2)
        Next
    End If
    st.Position = 0
    st.Type = was
    Bytes = r
End Function

Function Fails(code)
    Fails = (Err.Number And 65535) = code
End Function

Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetSpecialFolder(2) & "\sgstream"
If Not fso.FolderExists(dir) Then fso.CreateFolder dir

' unicode, the default: a byte order mark, then two bytes a character
Set s = CreateObject("ADODB.Stream")
s.Open
s.WriteText "AB"
Check s.Size = 6, "Unicode text has a byte order mark: size 6"
Check s.Position = 6, "the position is after the text"
Check Bytes(s) = "FFFE41004200", "the bytes are FFFE 4100 4200"
s.Position = 0
Check s.ReadText(-1) = "AB", "ReadText skips the byte order mark"
Check s.EOS, "the end of the stream"
s.Position = 0
Check s.ReadText(1) = "A" And s.Position = 4, "ReadText(1) reads one character"
s.Close

' lines
Set s = CreateObject("ADODB.Stream")
s.Open
s.LineSeparator = -1
s.WriteText "one", 1
s.WriteText "two", 1
s.WriteText "three"
s.Position = 0
Check s.ReadText(-2) = "one", "ReadText(-2): the first line"
Check s.ReadText(-2) = "two", "the second line"
Check s.ReadText(-2) = "three", "the last line has no separator"
Check s.EOS, "the end after the last line"
s.Position = 0
s.SkipLine
Check s.ReadText(-2) = "two", "SkipLine passes a line"
s.Close
Set s = CreateObject("ADODB.Stream")
s.Charset = "us-ascii"
s.Open
s.LineSeparator = 10
s.WriteText "a", 1
s.WriteText "b", 1
Check s.Size = 4, "a line ends with the line separator: LF"
s.LineSeparator = 13
s.WriteText "c", 1
Check s.Size = 6, "a line ends with the line separator: CR"
s.Position = 0
s.LineSeparator = 10
Check s.ReadText(-2) = "a", "ReadText(-2) with LF"
s.Close

' utf-8 and the single byte sets
Set s = CreateObject("ADODB.Stream")
s.Charset = "utf-8"
s.Open
s.WriteText "h" & ChrW(233) & "llo"
Check s.Size = 9, "UTF-8 text: a byte order mark and six bytes"
Check Bytes(s) = "EFBBBF68C3A96C6C6F", "UTF-8 bytes"
s.Close
Set s = CreateObject("ADODB.Stream")
s.Charset = "iso-8859-1"
s.Open
s.WriteText "h" & ChrW(233)
Check Bytes(s) = "68E9", "ISO-8859-1: no byte order mark, one byte for e-acute"
s.Position = 0
Check s.ReadText(-1) = "h" & ChrW(233), "read back as ISO-8859-1"
s.Close
Set s = CreateObject("ADODB.Stream")
s.Charset = "us-ascii"
s.Open
s.WriteText "abc"
Check s.Size = 3, "ASCII text is a byte a character"
s.Close
Set s = CreateObject("ADODB.Stream")
s.Charset = "windows-1252"
s.Open
s.WriteText ChrW(8364)
Check Bytes(s) = "80", "windows-1252: the euro sign is 0x80"
s.Close

' errors
Set s = CreateObject("ADODB.Stream")
On Error Resume Next
s.ReadText 1
Check Fails(3704), "ReadText on a closed stream: 3704"
Err.Clear
s.Open
s.Charset = "no-such-charset"
s.WriteText "x"
Check Fails(3001), "WriteText in an unknown character set: 3001"
Err.Clear
s.Charset = "unicode"
s.WriteText "x"
s.Type = 1
Check Fails(3219), "changing the type away from the start: 3219"
Err.Clear
s.Position = 0
s.Type = 1
Err.Clear
s.WriteText "x"
Check Fails(3219), "WriteText on a binary stream: 3219"
Err.Clear
s.Close
On Error Goto 0

' files
Set t = fso.CreateTextFile(dir & "\in.txt", True)
t.Write "ABC"
t.Close
Set s = CreateObject("ADODB.Stream")
s.Type = 1
s.Open
s.LoadFromFile dir & "\in.txt"
Check s.Size = 3 And s.Position = 0, "LoadFromFile: three bytes from the start"
Check Bytes(s) = "414243", "the bytes of the file"
On Error Resume Next
s.LoadFromFile dir & "\nothing.txt"
Check Fails(3002), "LoadFromFile of a missing file: 3002"
Err.Clear
s.SaveToFile dir & "\out.bin", 2
Check Err.Number = 0 And fso.FileExists(dir & "\out.bin") And fso.GetFile(dir & "\out.bin").Size = 3, "SaveToFile creates a file"
Err.Clear
s.SaveToFile dir & "\out.bin", 1
Check Fails(3004), "SaveToFile over a file without overwrite: 3004"
Err.Clear
s.SaveToFile dir & "\out.bin", 2
Check Err.Number = 0, "SaveToFile with overwrite"
Err.Clear
s.SaveToFile dir & "\out.bin", 9
Check Fails(3001), "SaveToFile with a bad option: 3001"
Err.Clear
On Error Goto 0
s.Close

' text from a file, and a file opened for writing is written on Close
Set t = fso.CreateTextFile(dir & "\u8.txt", True)
t.Close
Set s = CreateObject("ADODB.Stream")
s.Type = 1
s.Open
s.LoadFromFile dir & "\in.txt"
s.Close
Set s = CreateObject("ADODB.Stream")
s.Type = 1
s.Open dir & "\in.txt", 3
Check s.Size = 3, "Open on a file reads it"
s.Close
Set s = CreateObject("ADODB.Stream")
s.Charset = "us-ascii"
s.Open "URL=" & dir & "\in.txt", 3
Check s.ReadText(-1) = "ABC", "Open on a URL=path as text"
s.Position = 3
s.WriteText "DEF"
s.Flush
Check fso.GetFile(dir & "\in.txt").Size = 6, "Flush writes the stream back to its file"
s.WriteText "G"
s.Close
Check fso.GetFile(dir & "\in.txt").Size = 7, "Close writes it back as well"
Set s = CreateObject("ADODB.Stream")
s.Charset = "us-ascii"
s.Open dir & "\in.txt", 1
s.Position = 0
s.WriteText "Z"
s.Close
Check fso.OpenTextFile(dir & "\in.txt").ReadAll = "ABCDEFG", "a stream opened to read only does not write back"

' CopyTo
Set s = CreateObject("ADODB.Stream")
s.Type = 1
s.Open
s.LoadFromFile dir & "\in.txt"
Set d = CreateObject("ADODB.Stream")
d.Type = 1
d.Open
s.CopyTo d, 4
Check d.Size = 4 And s.Position = 4, "CopyTo(4): four bytes, the source moved on"
s.CopyTo d
Check d.Size = 7, "CopyTo with no size copies the rest"
Check Bytes(d) = "41424344454647", "the copy is the data"
s.Close
d.Close
Set s = CreateObject("ADODB.Stream")
s.Open
s.WriteText "texttexttext"
s.Position = 0
Set d = CreateObject("ADODB.Stream")
d.Open
s.CopyTo d, 4
d.Position = 0
Check d.ReadText(-1) = "text", "CopyTo text: characters"
s.Close
d.Close

fso.DeleteFolder dir, True
WScript.Echo "RESULT: " & Verdict()

Function Verdict()
    If failures = 0 Then Verdict = "PASS" Else Verdict = "FAIL"
End Function
