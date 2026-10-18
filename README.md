# CommandPrompt-ContextMenu

Adds "Command Prompt" directly to the Windows 11 top-level modern context menu when right-clicking empty background space in folders or on the Desktop.

## Prerequisites
Windows 11 (build 22000 or newer).

## Build (GitHub Actions)
1. Go to Actions -> "Build CommandPrompt-ContextMenu Package".
2. Click "Run workflow".
3. Download CommandPrompt-ContextMenu.zip from Artifacts.

## Release (GitHub Actions)
1. Go to Actions -> "Release CommandPrompt-ContextMenu".
2. Click "Run workflow" and enter the release tag (e.g. v1.0.0).

## Install
1. Extract CommandPrompt-ContextMenu.zip.
2. Open PowerShell as Administrator in that folder:
   powershell -ExecutionPolicy Bypass -File .\install.ps1
3. Delete the extracted folder after installation.

## Uninstall
Open PowerShell as Administrator in that folder:
powershell -ExecutionPolicy Bypass -File .\uninstall.ps1
