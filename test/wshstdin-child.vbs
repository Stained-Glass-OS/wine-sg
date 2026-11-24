' run by test/wshstdin.vbs through WshShell.Exec: echoes its piped input
Do Until WScript.StdIn.AtEndOfStream
    WScript.StdOut.WriteLine "got " & WScript.StdIn.ReadLine
Loop
