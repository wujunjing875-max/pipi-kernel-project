$ErrorActionPreference = 'Stop'
$adb = (Get-Command adb).Source
$serial = 'HA28LRFH'
$cfg = '/data/local/tmp/ov_cfg.txt'
$out = 'F:\CodexWork\tpen-esp\outputs\tpen_esp_opd2413_matched\verify_core_controls_final.txt'
$items = @(
  @{Key='k0';X=1000;Y=620}, @{Key='k1';X=1700;Y=620},
  @{Key='k2';X=1000;Y=664}, @{Key='bone';X=1700;Y=664},
  @{Key='lab';X=1000;Y=708}, @{Key='k3';X=1700;Y=708},
  @{Key='k4';X=1000;Y=752}, @{Key='abar';X=1700;Y=752},
  @{Key='dec';X=1000;Y=796;File='dec'}, @{Key='dec2';X=1700;Y=796;File='dec2'},
  @{Key='dec3';X=1000;Y=840;File='dec3'}, @{Key='dec4';X=1700;Y=840;File='dec4'}
)
function Val([string]$k) {
  $line=(& $adb -s $serial shell "grep '^$k=' $cfg" | Select-Object -Last 1).Trim()
  if($line -notmatch '=(0|1)$'){throw "bad $k=$line"}; [int]$Matches[1]
}
function Tap([int]$x,[int]$y){
  & $adb -s $serial shell su -c "/data/local/tmp/.tapxy.sh $x $y" | Out-Null
  if($LASTEXITCODE -ne 0){throw "tap adb exit $LASTEXITCODE"}
  Start-Sleep -Milliseconds 650
}
$lines=[System.Collections.Generic.List[string]]::new(); $fail=0
$lines.Add("DEVICE=$serial")
foreach($i in $items){
  $start=Val $i.Key; Tap $i.X $i.Y; $changed=Val $i.Key
  $f1='-'; if($i.File){$f1=(& $adb -s $serial shell "cat /data/local/tmp/ov_$($i.File).txt").Trim()}
  Tap $i.X $i.Y; $restored=Val $i.Key
  $f2='-'; if($i.File){$f2=(& $adb -s $serial shell "cat /data/local/tmp/ov_$($i.File).txt").Trim()}
  $ok=($changed -eq (1-$start) -and $restored -eq $start -and (!$i.File -or ([int]$f1 -eq $changed -and [int]$f2 -eq $restored)))
  if(!$ok){$fail++}
  $line="$($i.Key): start=$start changed=$changed file_changed=$f1 restored=$restored file_restored=$f2 PASS=$ok"
  $lines.Add($line); Write-Host $line
}
$lines.Add("TOTAL=$($items.Count) FAIL=$fail")
$lines | Set-Content -LiteralPath $out -Encoding utf8
Write-Host "RESULT_FILE=$out"
if($fail){exit 1}
