param(
    [string]$GamePath = "C:\Program Files (x86)\Steam\steamapps\common\Chipmatic"
)

$ErrorActionPreference = "Stop"

$statsPath = Join-Path $GamePath "www\buildingsStats.json"

if (-not (Test-Path -LiteralPath $statsPath)) {
    Write-Error "File non trovato: $statsPath"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupPath = "$statsPath.backup-$timestamp"
Copy-Item -LiteralPath $statsPath -Destination $backupPath

$json = Get-Content -LiteralPath $statsPath -Raw | ConvertFrom-Json

function Get-ReducedCost {
    param([double]$Value)
    return [Math]::Max(1, [Math]::Ceiling($Value / 2))
}

function Reduce-Costs {
    param($Node)

    if ($null -eq $Node) {
        return
    }

    if ($Node -is [System.Array]) {
        foreach ($item in $Node) {
            Reduce-Costs -Node $item
        }
        return
    }

    if ($Node -isnot [psobject]) {
        return
    }

    foreach ($property in @($Node.PSObject.Properties)) {
        $name = $property.Name
        $value = $property.Value

        if ($name -eq "cost" -and $value -is [int]) {
            $property.Value = Get-ReducedCost -Value $value
        }
        elseif (($name -eq "cost" -or $name -eq "price") -and $value -is [System.Array]) {
            foreach ($entry in $value) {
                foreach ($entryProperty in @($entry.PSObject.Properties)) {
                    $entryName = $entryProperty.Name
                    $entryValue = $entryProperty.Value

                    if (
                        $entryValue -is [int] -and
                        $entryName -notin @("value", "capacity", "energyPerCharge", "genTime", "EnergyPerSecond")
                    ) {
                        $entryProperty.Value = Get-ReducedCost -Value $entryValue
                    }
                }
            }
        }

        Reduce-Costs -Node $property.Value
    }
}

$stations = @(
    "battery",
    "furnace",
    "garage",
    "laboratory",
    "refiner",
    "solar",
    "crystal",
    "atom"
)

foreach ($station in $stations) {
    if ($json.PSObject.Properties.Name -contains $station) {
        Reduce-Costs -Node $json.$station
    }
}

if ($json.PSObject.Properties.Name -contains "drill") {
    $json.drill.speedMultiplier = 3
    $json.drill.capacity = 80

    foreach ($itemProperty in @($json.drill.items.PSObject.Properties)) {
        $item = $itemProperty.Value

        if ($item.PSObject.Properties.Name -contains "amount") {
            $item.amount = $item.amount * 3
        }

        if ($item.PSObject.Properties.Name -contains "time") {
            $item.time = [Math]::Max(10, [Math]::Ceiling($item.time / 2))
        }

        if ($item.PSObject.Properties.Name -contains "energy") {
            $item.energy = [Math]::Max(1, [Math]::Ceiling($item.energy / 2))
        }
    }
}

$json | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $statsPath -Encoding UTF8

Write-Host "MOD applicata correttamente."
Write-Host "Backup creato: $backupPath"

