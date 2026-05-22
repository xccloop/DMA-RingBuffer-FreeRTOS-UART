<#
.SYNOPSIS
    Verify the self-contained DMA-UART project is ready for Keil MDK.

.DESCRIPTION
    This project is SELF-CONTAINED — all third-party source code
    (FreeRTOS, STM32 HAL, CMSIS) is bundled in ./ThirdParty/

    This script just verifies the file structure and prints
    instructions for Keil MDK setup.

.NOTES
    No external downloads or path configuration needed.
#>

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "╔══════════════════════════════════════════╗"
Write-Host "║  DMA-UART Framework — Self-Contained    ║"
Write-Host "║  STM32F103ZET6 Keil MDK Project         ║"
Write-Host "╚══════════════════════════════════════════╝"
Write-Host ""

# ---- Verify ThirdParty files ----
$tp = Join-Path $ScriptDir "ThirdParty"
$checks = @(
    @{Path="$tp\FreeRTOS\src\tasks.c"; Name="FreeRTOS tasks.c"},
    @{Path="$tp\FreeRTOS\include\FreeRTOS.h"; Name="FreeRTOS.h"},
    @{Path="$tp\FreeRTOS\portable\RVDS\ARM_CM3\port.c"; Name="FreeRTOS port.c (Keil)"},
    @{Path="$tp\FreeRTOS\portable\MemMang\heap_4.c"; Name="FreeRTOS heap_4.c"},
    @{Path="$tp\HAL\Src\stm32f1xx_hal.c"; Name="HAL core"},
    @{Path="$tp\HAL\Src\stm32f1xx_hal_uart.c"; Name="HAL UART driver"},
    @{Path="$tp\HAL\Src\stm32f1xx_hal_dma.c"; Name="HAL DMA driver"},
    @{Path="$tp\HAL\Inc\stm32f1xx_hal.h"; Name="HAL main header"},
    @{Path="$tp\CMSIS\Device\stm32f1xx.h"; Name="CMSIS device header"},
    @{Path="$tp\CMSIS\Device\system_stm32f1xx.c"; Name="CMSIS system init"},
    @{Path="$tp\CMSIS\Include\core_cm3.h"; Name="CMSIS Cortex-M3 core"},
    @{Path="$tp\CMSIS\Include\cmsis_gcc.h"; Name="CMSIS compiler (GCC)"},
    @{Path="$tp\CMSIS\Include\cmsis_armcc.h"; Name="CMSIS compiler (ARMCC/Keil)"}
)

$all_ok = $true
foreach ($c in $checks) {
    if (Test-Path $c.Path) {
        Write-Host "  [OK]  $($c.Name)" -ForegroundColor Green
    } else {
        Write-Host "  [MISS] $($c.Name)" -ForegroundColor Red
        $all_ok = $false
    }
}

Write-Host ""
if ($all_ok) {
    Write-Host "All dependencies OK. Project is ready!" -ForegroundColor Green
} else {
    Write-Host "SOME FILES MISSING! Run: git checkout platform/" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== HOW TO USE ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Keil MDK (recommended):" -ForegroundColor White
Write-Host "  1. Install STM32F1 pack in Keil Pack Installer" -ForegroundColor Gray
Write-Host "     (Menu: Project → Manage → Pack Installer)" -ForegroundColor Gray
Write-Host "     Search 'STM32F1', install 'Keil::STM32F1xx_DFP'" -ForegroundColor Gray
Write-Host ""
Write-Host "  2. Open DMAFreeRTOS.uvprojx" -ForegroundColor Gray
Write-Host "  3. Project → Options → Debug → Select ST-Link" -ForegroundColor Gray
Write-Host "  4. F7 = Build, F8 = Download" -ForegroundColor Gray
Write-Host ""
Write-Host "  GCC (free alternative):" -ForegroundColor White
Write-Host "  make clean && make" -ForegroundColor Gray
Write-Host "  st-flash write DMAFreeRTOS.bin 0x08000000" -ForegroundColor Gray
Write-Host ""
Write-Host "  CubeMX (advanced):" -ForegroundColor White
Write-Host "  1. Generate STM32F103ZE project with FreeRTOS" -ForegroundColor Gray
Write-Host "  2. Merge framework files into generated project" -ForegroundColor Gray
Write-Host "  See README.md for details" -ForegroundColor Gray
