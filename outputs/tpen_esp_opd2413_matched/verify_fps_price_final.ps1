$ErrorActionPreference='Stop'
$adb=(Get-Command adb).Source; $s='HA28LRFH'; $cfg='/data/local/tmp/ov_cfg.txt'
$out='F:\CodexWork\tpen-esp\outputs\tpen_esp_opd2413_matched\verify_fps_price_final.txt'
function Tap($x,$y){& $adb -s $s shell su -c "/data/local/tmp/.tapxy.sh $x $y" | Out-Null; if($LASTEXITCODE){throw "tap exit $LASTEXITCODE"}; Start-Sleep -Milliseconds 650}
function GetVal($k){((& $adb -s $s shell "grep '^$k=' $cfg" | Select-Object -Last 1).Trim() -split '=')[1]}
$lines=[System.Collections.Generic.List[string]]::new(); $fail=0
$fps=@(@{V=30;X=942},@{V=60;X=1034},@{V=90;X=1126},@{V=120;X=1218},@{V=144;X=1310},@{V=60;X=1034})
$price=@(@{V=2000;X=955},@{V=5000;X=1073},@{V=10000;X=1191},@{V=20000;X=1309},@{V=25000;X=1427},@{V=2000;X=955})
foreach($i in $fps){Tap $i.X 886; $got=[int](GetVal 'fps'); $ok=$got -eq $i.V; if(!$ok){$fail++}; $z="fps selected=$($i.V) saved=$got PASS=$ok"; $lines.Add($z); Write-Host $z}
foreach($i in $price){Tap $i.X 930; $got=[int](GetVal 'price'); $ok=$got -eq $i.V; if(!$ok){$fail++}; $z="price selected=$($i.V) saved=$got PASS=$ok"; $lines.Add($z); Write-Host $z}
$lines.Add("FINAL fps=$(GetVal 'fps') price=$(GetVal 'price') FAIL=$fail")
$lines | Set-Content -LiteralPath $out -Encoding utf8
Write-Host "RESULT_FILE=$out"; if($fail){exit 1}
