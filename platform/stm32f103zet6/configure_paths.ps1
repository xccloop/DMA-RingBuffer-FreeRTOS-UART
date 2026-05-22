#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Auto-configure STM32CubeF1 and FreeRTOS paths for the DMA-UART Framework.

.DESCRIPTION
    Scans common installation locations for STM32CubeF1 and FreeRTOS,
    then updates Makefile and Keil project paths accordingly.

    For Keil users: after running this script, the .uvprojx/.uvoptx files
    will have correct paths. Open the project in Keil and it will find
    all HAL and FreeRTOS source files.

.PARAMETER CubePath
    Optional: Manually specify STM32CubeF1 repository path

.PARAMETER FreeRtosPath
    Optional: Manually specify FreeRTOS source path

.EXAMPLE
    .\configure_paths.ps1

.EXAMPLE
    .\configure_paths.ps1 -CubePath "D:\STM32Cube_FW_F1_V1.8.5" -FreeRtosPath "D:\FreeRTOSv202212.01\FreeRTOS"
#>

param(
    [string]$CubePath = "",
    [string]$FreeRtosPath = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " DMA-UART Framework Path Configurator" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ---- Find STM32CubeF1 ----
if ($CubePath -eq "") {
    $searchPaths = @(
        "C:\Users\$env:USERNAME\STM32Cube\Repository\STM32Cube_FW_F1_V*",
        "D:\STM32Cube\Repository\STM32Cube_FW_F1_V*",
        "C:\STM32Cube_FW_F1_V*",
        "D:\STM32Cube_FW_F1_V*",
        "$ScriptDir\..\..\STM32Cube_FW_F1_V*"
    )

    foreach ($pattern in $searchPaths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($found) {
            $CubePath = $found.FullName
            break
        }
    }
}

if ($CubePath -and (Test-Path $CubePath)) {
    Write-Host "[OK] STM32CubeF1: $CubePath" -ForegroundColor Green
} else {
    Write-Host "[!!] STM32CubeF1 NOT FOUND!" -ForegroundColor Yellow
    Write-Host "     Download from: https://github.com/STMicroelectronics/STM32CubeF1" -ForegroundColor Gray
    Write-Host "     Or use: -CubePath to specify location" -ForegroundColor Gray
    Write-Host "     Without it, Keil/Makefile will have broken paths." -ForegroundColor Gray
    $CubePath = ""
}

# ---- Find FreeRTOS ----
if ($FreeRtosPath -eq "") {
    $searchPaths = @(
        "C:\Users\$env:USERNAME\FreeRTOS\FreeRTOSv*\FreeRTOS",
        "C:\FreeRTOS\FreeRTOSv*\FreeRTOS",
        "D:\FreeRTOS\FreeRTOSv*\FreeRTOS",
        "$ScriptDir\..\..\FreeRTOS\FreeRTOSv*\FreeRTOS"
    )

    foreach ($pattern in $searchPaths) {
        $found = Get-Item $pattern -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
        if ($found) {
            $FreeRtosPath = $found.FullName
            break
        }
    }
}

if ($FreeRtosPath -and (Test-Path $FreeRtosPath)) {
    Write-Host "[OK] FreeRTOS: $FreeRtosPath" -ForegroundColor Green
} else {
    Write-Host "[!!] FreeRTOS NOT FOUND!" -ForegroundColor Yellow
    Write-Host "     Download from: https://github.com/FreeRTOS/FreeRTOS" -ForegroundColor Gray
    Write-Host "     Or use: -FreeRtosPath to specify location" -ForegroundColor Gray
    $FreeRtosPath = ""
}

# ---- Update Makefile ----
$makefilePath = Join-Path $ScriptDir "Makefile"
if (Test-Path $makefilePath) {
    $makefileContent = Get-Content $makefilePath -Raw

    if ($CubePath) {
        $cubePathNormalized = $CubePath -replace '\\','/'
        $makefileContent = $makefileContent -replace 'STM32CUBE = .*', "STM32CUBE = $cubePathNormalized"
    }
    if ($FreeRtosPath) {
        $freeRtosPathNormalized = $FreeRtosPath -replace '\\','/'
        $makefileContent = $makefileContent -replace 'FREERTOS = .*', "FREERTOS = $freeRtosPathNormalized"
    }

    Set-Content $makefilePath -Value $makefileContent
    Write-Host "[OK] Makefile updated" -ForegroundColor Green
}

# ---- Update Keil project files ----
$cubePathNormalized = if ($CubePath) { $CubePath -replace '\\','/' } else { "__CUBE__" }
$freeRtosPathNormalized = if ($FreeRtosPath) { $FreeRtosPath -replace '\\','/' } else { "__FREERTOS__" }

foreach ($ext in @(".uvprojx", ".uvoptx")) {
    $projPath = Join-Path $ScriptDir "DMAFreeRTOS$ext"
    if (Test-Path $projPath) {
        $content = Get-Content $projPath -Raw
        $content = $content -replace '__CUBE__', $cubePathNormalized
        $content = $content -replace '__FREERTOS__', $freeRtosPathNormalized
        Set-Content $projPath -Value $content
        Write-Host "[OK] DMAFreeRTOS$ext updated" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Configuration complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (-not $CubePath -or -not $FreeRtosPath) {
    Write-Host ""
    Write-Host "NEXT STEPS:" -ForegroundColor Yellow
    Write-Host " 1. Install missing dependencies" -ForegroundColor White
    Write-Host " 2. Re-run this script with correct paths:" -ForegroundColor White
    Write-Host "    .\configure_paths.ps1 -CubePath '<path>' -FreeRtosPath '<path>'" -ForegroundColor White
    Write-Host ""
    Write-Host " 3. Open DMAFreeRTOS.uvprojx in Keil MDK" -ForegroundColor White
    Write-Host "    - Set 'Options for Target' → Debug → ST-Link" -ForegroundColor White
    Write-Host "    - Click 'Build' (F7)" -ForegroundColor White
    Write-Host "    - Click 'Download' (F8) to flash" -ForegroundColor White
} else {
    Write-Host ""
    Write-Host "READY! You can now:" -ForegroundColor Cyan
    Write-Host "  1. Open DMAFreeRTOS.uvprojx in Keil MDK and build" -ForegroundColor White
    Write-Host "  2. Or run: make clean && make" -ForegroundColor White
}
