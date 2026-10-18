# Must run as Administrator
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Warning "Please run PowerShell as Administrator!"
    Exit
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
if (-not $ScriptDir) { $ScriptDir = $PSScriptRoot }

Write-Host "=============================================" -ForegroundColor DarkCyan
Write-Host "     CommandPrompt-ContextMenu Installer     " -ForegroundColor White
Write-Host "=============================================" -ForegroundColor DarkCyan

Write-Host "`n1. Installing Certificate..." -ForegroundColor Cyan
$certPath = Join-Path $ScriptDir "CommandPrompt-ContextMenu.cer"
Import-Certificate -FilePath $certPath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null

Write-Host "2. Installing directly to WindowsApps..." -ForegroundColor Cyan
$msixPath = Join-Path $ScriptDir "CmdMenu.msix"
Add-AppxPackage -Path $msixPath

Write-Host "`nInstalled cleanly inside C:\Program Files\WindowsApps\CommandPrompt-ContextMenu_...!" -ForegroundColor Green
Write-Host "You can now safely delete the downloaded folder." -ForegroundColor Yellow
