$ErrorActionPreference = 'Stop'
$adb = (Get-Command adb).Source
$serial = 'HA28LRFH'
$remoteCfg = '/data/local/tmp/ov_cfg.txt'
$tapScript = 'F:\CodexWork\tpen-esp\outputs\tpen_esp_opd2413_matched\device_tapxy.sh'
$resultFile = 'F:\CodexWork\tpen-esp\outputs\tpen_esp_opd2413_matched\verify_all_categories.txt'

@'
#!/system/bin/sh
X="$1"
Y="$2"
RX=$((Y * 10))
RY=$((X * 10))
sendevent /dev/input/event5 3 53 "$RX"
sendevent /dev/input/event5 3 54 "$RY"
sendevent /dev/input/event5 3 57 511
sendevent /dev/input/event5 1 330 1
sendevent /dev/input/event5 0 0 0
usleep 70000
sendevent /dev/input/event5 1 330 0
sendevent /dev/input/event5 3 57 -1
sendevent /dev/input/event5 0 0 0
'@ -replace "`r`n", "`n" | Set-Content -LiteralPath $tapScript -NoNewline -Encoding utf8
& $adb -s $serial push $tapScript /data/local/tmp/.tapxy.sh | Out-Null
& $adb -s $serial shell su -c 'chmod 755 /data/local/tmp/.tapxy.sh'

$items = @(
    @{ Key='cat_n_box_player'; X=1000; Y=1029 },
    @{ Key='cat_n_box_bot'; X=1492; Y=1029 },
    @{ Key='cat_n_safebox'; X=1960; Y=1029 },
    @{ Key='cat_n_med'; X=1000; Y=1073 },
    @{ Key='cat_n_ammo'; X=1492; Y=1073 },
    @{ Key='cat_n_weapon'; X=1960; Y=1073 },
    @{ Key='cat_n_tool'; X=1000; Y=1117 },
    @{ Key='cat_n_bag'; X=1492; Y=1117 },
    @{ Key='cat_n_drawer'; X=1960; Y=1117 },
    @{ Key='cat_n_wardrobe'; X=1000; Y=1161 },
    @{ Key='cat_n_jacket'; X=1492; Y=1161 },
    @{ Key='cat_n_pc'; X=1960; Y=1161 },
    @{ Key='cat_n_file'; X=1000; Y=1205 },
    @{ Key='cat_n_cash'; X=1492; Y=1205 },
    @{ Key='cat_n_fridge'; X=1960; Y=1205 },
    @{ Key='cat_n_washer'; X=1000; Y=1249 },
    @{ Key='cat_n_case'; X=1492; Y=1249 },
    @{ Key='cat_n_crate'; X=1960; Y=1249 },
    @{ Key='cat_n_airdrop'; X=1000; Y=1293 },
    @{ Key='cat_n_locker'; X=1492; Y=1293 },
    @{ Key='cat_n_jewelry'; X=1960; Y=1293 },
    @{ Key='cat_n_unknown'; X=1000; Y=1337 }
)

function Get-CfgValue([string]$Key) {
    $line = (& $adb -s $serial shell "grep '^$Key=' $remoteCfg" 2>$null | Select-Object -Last 1).Trim()
    if ($line -match '=(0|1)$') { return [int]$Matches[1] }
    throw "missing config key: $Key, line=$line"
}

function Tap-Until([hashtable]$Item, [int]$Expected) {
    for ($attempt = 1; $attempt -le 6; $attempt++) {
        & $adb -s $serial shell su -c "/data/local/tmp/.tapxy.sh $($Item.X) $($Item.Y)" | Out-Null
        Start-Sleep -Milliseconds 550
        if ((Get-CfgValue $Item.Key) -eq $Expected) { return $attempt }
    }
    return -1
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("DEVICE=$serial")
$lines.Add("CONFIG=$remoteCfg")
$lines.Add("TEST=each category toggled once and restored to its exact starting value")
$fail = 0
foreach ($item in $items) {
    $start = Get-CfgValue $item.Key
    $changed = 1 - $start
    $offAttempts = Tap-Until $item $changed
    $afterChange = Get-CfgValue $item.Key
    $restoreAttempts = Tap-Until $item $start
    $afterRestore = Get-CfgValue $item.Key
    $ok = ($offAttempts -gt 0 -and $restoreAttempts -gt 0 -and $afterChange -eq $changed -and $afterRestore -eq $start)
    if (-not $ok) { $fail++ }
    $line = "{0}: start={1} changed={2} change_attempts={3} restored={4} restore_attempts={5} PASS={6}" -f $item.Key,$start,$afterChange,$offAttempts,$afterRestore,$restoreAttempts,$ok
    $lines.Add($line)
    Write-Host $line
}
$lines.Add("TOTAL=$($items.Count) FAIL=$fail")
$lines | Set-Content -LiteralPath $resultFile -Encoding utf8
Write-Host "RESULT_FILE=$resultFile"
if ($fail -ne 0) { exit 1 }
