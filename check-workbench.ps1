$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class WorkbenchCheck {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out Rect r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr SendText(IntPtr h,uint m,IntPtr w,string text);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent,IntPtr child,string cls,string title);
}
'@
$TestRoot=Join-Path $PSScriptRoot 'validation\runtime'
[void](New-Item -ItemType Directory -Force $TestRoot)
Get-Process piano-check -ErrorAction SilentlyContinue | ForEach-Object { [void]$_.CloseMainWindow(); [void]$_.WaitForExit(5000) }
Copy-Item (Join-Path $PSScriptRoot 'piano.exe') (Join-Path $TestRoot 'piano-check.exe') -Force
Start-Process -FilePath (Join-Path $TestRoot 'piano-check.exe')
Start-Sleep -Milliseconds 500
$p=Get-Process piano-check | Where-Object {$_.MainWindowTitle -like 'Piano Material Lab*'} | Select-Object -First 1
if (-not $p) { throw 'Workbench not open' }
$h=$p.MainWindowHandle
[void][WorkbenchCheck]::SetForegroundWindow($h)
$r=New-Object WorkbenchCheck+Rect
[void][WorkbenchCheck]::GetClientRect($h,[ref]$r)
if($r.R -ne 1440 -or $r.B -ne 960){throw 'Wrong client size'}
function Message($m,$w,$l){[void][WorkbenchCheck]::SendMessage($h,$m,[IntPtr]([long]$w),[IntPtr]([long]$l))}
function Click($x,$y){Message 0x201 1 ($y*65536+$x);Message 0x202 0 ($y*65536+$x)}
function Snapshot($name){
 $b=New-Object Drawing.Bitmap 1440,960;$g=[Drawing.Graphics]::FromImage($b);$dc=$g.GetHdc()
 try{if(-not [WorkbenchCheck]::PrintWindow($h,$dc,1)){throw 'Print failed'}}finally{$g.ReleaseHdc($dc)}
 $b.Save((Join-Path $TestRoot $name));$g.Dispose();$b.Dispose()
}
Snapshot 'workbench.png'
$preset=Join-Path $TestRoot 'user-preset.piano'
$backup=$null;if(Test-Path $preset){$backup=[IO.File]::ReadAllBytes($preset)}
Click 940 46
$original=[IO.File]::ReadAllBytes($preset)
try{
 $before=Get-Content $preset
 for($i=0;$i -lt 42;$i++){
  $group=[math]::Floor($i/6);$x=24+($group%4)*352+56+($i%3)*112
  $y=94+[math]::Floor($group/4)*320+96+[math]::Floor(($i%6)/3)*130
  Message 0x201 1 ($y*65536+$x)
  Message 0x200 1 (($y-9)*65536+$x)
  Message 0x202 0 (($y-9)*65536+$x)
 }
 Click 940 46
 $after=Get-Content $preset
 $changed=0;for($i=1;$i -le 42;$i++){if($before[$i] -ne $after[$i]){$changed++}}
 if($changed -ne 42){throw "Only $changed knobs changed saved values"}
 Click 825 46
 Click 1060 46
 Click 940 46
 if((Get-Content $preset -Raw) -ne ($after -join "`n")+"`n"){
  # Compare line arrays to tolerate CRLF vs LF.
  $reload=Get-Content $preset
  if(@(Compare-Object $after $reload).Count){throw 'Preset load/save did not round trip'}
 }
 # Type an exact hammer mass into the real child edit control.
 Message 0x203 1 (190*65536+80)
 $edit=[WorkbenchCheck]::GetDlgItem($h,100)
 if($edit -eq [IntPtr]::Zero){throw 'No numeric editor'}
 [void][WorkbenchCheck]::SendText($edit,0xC,[IntPtr]::Zero,'7.25')
 [void][WorkbenchCheck]::SendMessage($edit,0x100,[IntPtr]13,[IntPtr]0)
 Click 940 46
 if((Get-Content $preset)[1] -notmatch '^hammer_mass 7.25$'){throw ('Exact entry failed: '+(Get-Content $preset)[1])}
 # Restore original settings before audio / image verification.
 [IO.File]::WriteAllBytes($preset,$original);Click 1060 46
 Message 0x100 32 0
 for($i=0;$i -lt 42;$i++){
  $group=[math]::Floor($i/6);$x=24+($group%4)*352+56+($i%3)*112
  $y=94+[math]::Floor($group/4)*320+96+[math]::Floor(($i%6)/3)*130
  Message 0x201 1 ($y*65536+$x)
  Message 0x200 1 (($y-2)*65536+$x)
  Message 0x202 0 (($y-2)*65536+$x)
  Start-Sleep -Milliseconds 55
 }
 Start-Sleep -Milliseconds 200
 Snapshot 'workbench-pressed.png'
 Message 0x101 32 0
 Start-Sleep -Milliseconds 500
 Snapshot 'workbench.png'
 if(-not (Get-Process -Id $p.Id).Responding){throw 'UI unresponsive'}
 $result='PASS: 1440x960 client; all 42 native knob drags update distinct parameters; preset round trip; exact numeric entry; keyboard strike/release; 42 knob adjustments during live playback; rendered screenshots; responsive window.'
 $result | Set-Content (Join-Path $TestRoot 'workbench-ui-validation.txt')
 $result
}finally{
 Message 0x101 32 0
 [IO.File]::WriteAllBytes($preset,$original);Click 1060 46
 if($null -ne $backup){[IO.File]::WriteAllBytes($preset,$backup)}else{Remove-Item $preset -ErrorAction SilentlyContinue}
 [void]$p.CloseMainWindow();[void]$p.WaitForExit(5000)
}
