$syms = Get-Content nm-symbols.txt | ForEach-Object {
    $t = $_.Trim()
    $p = $t -split '\s+'
    if ($p.Count -ge 3 -and $p[0] -match '^[0-9a-fA-F]{16}$') {
        [PSCustomObject]@{ Addr = [Convert]::ToUInt64($p[0],16); Name = $p[2] }
    }
} | Sort-Object Addr

$queries = @(
    'FFFFFFFF8001113E','FFFFFFFF80016546','FFFFFFFF80003004','FFFFFFFF8001150A',
    'FFFFFFFF8000581B','FFFFFFFF800087DD','FFFFFFFF80008887','FFFFFFFF80002DEE',
    'FFFFFFFF80002DCA','FFFFFFFF80002DD8','FFFFFFFF8001101F','FFFFFFFF8001102D',
    'FFFFFFFF8001107E','FFFFFFFF80011060','FFFFFFFF80011220','FFFFFFFF800111D2'
)
foreach ($q in $queries) {
    $qa = [Convert]::ToUInt64($q,16)
    $best = $null
    foreach ($s in $syms) {
        if ($s.Addr -le $qa) { $best = $s } else { break }
    }
    if ($best) {
        $off = $qa - $best.Addr
        Write-Output ("{0} -> {1} +0x{2:X}" -f $q, $best.Name, $off)
    } else {
        Write-Output ("{0} -> NOT FOUND" -f $q)
    }
}
