Option Explicit
Dim shell,fs,base,env,candidates,version,location,python,command,result
Set shell=CreateObject("WScript.Shell")
Set fs=CreateObject("Scripting.FileSystemObject")
base=fs.GetParentFolderName(WScript.ScriptFullName)
If Not fs.FolderExists(base & "\Temp") Then fs.CreateFolder(base & "\Temp")
Set env=shell.Environment("PROCESS")
env("TEMP")=base & "\Temp"
env("TMP")=base & "\Temp"
env("PYTHONDONTWRITEBYTECODE")="1"
python=""
On Error Resume Next
For Each location In Array("HKCU", "HKLM")
 For Each version In Array("3.14", "3.13", "3.12", "3.11")
  Err.Clear
  candidates=shell.RegRead(location & "\Software\Python\PythonCore\" & version & "\InstallPath\")
  If Err.Number=0 Then
   If fs.FileExists(candidates & "pythonw.exe") Then python=Chr(34) & candidates & "pythonw.exe" & Chr(34)
  End If
  If python<>"" Then Exit For
 Next
 If python<>"" Then Exit For
Next
On Error GoTo 0
If python="" Then
 If fs.FileExists(shell.ExpandEnvironmentStrings("%WINDIR%") & "\pyw.exe") Then
  python=Chr(34) & shell.ExpandEnvironmentStrings("%WINDIR%") & "\pyw.exe" & Chr(34) & " -3"
 Else
  python="pythonw.exe"
 End If
End If
command=python & " " & Chr(34) & base & "\Launcher\Launch-PCVR.pyw" & Chr(34)
On Error Resume Next
result=shell.Run(command,0,False)
If Err.Number<>0 Then MsgBox "Python3 with Tk could not be found. Install Python3.11+ and open Launcher\Launch-PCVR.pyw, or add pythonw.exe to PATH.",48,"Portal VR launcher"
On Error GoTo 0
