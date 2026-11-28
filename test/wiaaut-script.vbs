' wiaaut-script.vbs: WIA Automation as scripts use it (late bound, through IDispatch).
' Writes PASS/FAIL lines to the file named by the first argument.
Option Explicit
Dim fso, out, failures
Set fso = CreateObject("Scripting.FileSystemObject")
Set out = fso.CreateTextFile(WScript.Arguments(0), True)
failures = 0

Sub Check(ok, what)
    If ok Then
        out.WriteLine "PASS  " & what
    Else
        out.WriteLine "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Const wiaFormatBMP = "{B96B3CAB-0728-11D3-9D7B-0000F81EF32E}"
Const wiaFormatPNG = "{B96B3CAF-0728-11D3-9D7B-0000F81EF32E}"
Const wiaFormatJPEG = "{B96B3CAE-0728-11D3-9D7B-0000F81EF32E}"

On Error Resume Next
Dim dm, info, dev, item, img, ip, png, v, cd, img2, p, n, folder
Set dm = CreateObject("WIA.DeviceManager")
Check Err.Number = 0, "WIA.DeviceManager is created"
Check dm.DeviceInfos.Count = 1, "DeviceInfos lists the scanner"
Set info = dm.DeviceInfos(1)
Check Left(info.DeviceID, 39) = "{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}\", "a DeviceInfo has the device's ID"
Check info.Type = 1, "the device is a scanner (ScannerDeviceType)"
Check Len(info.Properties("Name").Value) > 0, "the device's Name property"
Set dev = info.Connect
Check Err.Number = 0 And dev.Items.Count = 1, "Connect gives the device and its item"
Set item = dev.Items(1)
n = 0
For Each p In item.Properties
    n = n + 1
Next
Check n > 30, "the item's properties enumerate (" & n & ")"
Check item.Properties("Horizontal Resolution").SubType = 1, "the resolution has a range (RangeSubType)"
Check item.Properties("Horizontal Resolution").SubTypeMax = 1200, "the range's maximum is the device's"
item.Properties("Horizontal Resolution").Value = 100
item.Properties("6148").Value = 100
item.Properties("Horizontal Extent").Value = 300
item.Properties("Vertical Extent").Value = 200
Check Err.Number = 0, "properties are set by name and by number"
Check item.Properties("Pixels Per Line").Value = 300, "a dependent property follows (Pixels Per Line)"
item.Properties("Pixels Per Line").Value = 5
Check Err.Number <> 0, "a read-only property refuses a value"
Err.Clear
n = 0
For Each v In item.Formats
    If v = wiaFormatJPEG Then n = 1
Next
Check n = 1, "the item's Formats include JPEG"
Set img = item.Transfer(wiaFormatJPEG)
Check Err.Number = 0, "Transfer scans"
Check img.Width = 300 And img.Height = 200, "the ImageFile is 300x200 (" & img.Width & "x" & img.Height & ")"
Check img.FormatID = wiaFormatJPEG And LCase(img.FileExtension) = "jpg", "the ImageFile is a JPEG"
Check img.HorizontalResolution = 100, "with the item's resolution"
img.SaveFile WScript.Arguments(1) & "\scan.jpg"
Check fso.FileExists(WScript.Arguments(1) & "\scan.jpg"), "SaveFile writes the file"
img.SaveFile WScript.Arguments(1) & "\scan.jpg"
Check Err.Number <> 0, "SaveFile does not replace a file"
Err.Clear

Set ip = CreateObject("WIA.ImageProcess")
ip.Filters.Add ip.FilterInfos("RotateFlip").FilterID
ip.Filters(1).Properties("RotationAngle").Value = 90
ip.Filters.Add ip.FilterInfos("Convert").FilterID
ip.Filters(2).Properties("FormatID").Value = wiaFormatPNG
Set png = ip.Apply(img)
Check Err.Number = 0, "ImageProcess applies RotateFlip and Convert"
Check png.Width = 200 And png.Height = 300 And png.FormatID = wiaFormatPNG, "the result is a turned PNG"
Set v = png.FileData
Check v.Count > 100 And v(1) = 137 And v(2) = 80, "FileData holds the PNG's bytes"
Set v = png.ARGBData
Check v.Count = 60000, "ARGBData has a value per pixel"

Set img2 = Nothing
Set cd = CreateObject("WIA.CommonDialog")
Set img2 = cd.ShowAcquireImage(1, 2, 131072, wiaFormatBMP, False, False, False)
Check Err.Number = 0 And Not (img2 Is Nothing), "ShowAcquireImage without its dialog scans the only scanner"
Check img2.FormatID = wiaFormatBMP And img2.PixelDepth = 8, "a grayscale intent gives an 8-bit BMP"
Set dev = cd.ShowSelectDevice(1, False, False)
Check Err.Number = 0 And dev.Items.Count = 1, "ShowSelectDevice gives the only scanner"
Set v = CreateObject("WIA.Vector")
v.Add 5
v.Add "x"
Check v.Count = 2 And v(1) = 5, "WIA.Vector"
If failures = 0 Then out.WriteLine "RESULT: PASS" Else out.WriteLine "RESULT: FAIL"
out.Close
