# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Configures, builds and tests one workflow preset in a developer command prompt for MSVC.
#
#   ci/windows/build.ps1 -MsvcVersion 14.52 -Preset msvc-debug
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^14\.\d{2}$')]
    [string] $MsvcVersion,

    [Parameter(Mandatory)]
    [string] $Preset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

& (Join-Path $PSScriptRoot 'enter-msvc.ps1') -Version $MsvcVersion
# The machine's clang-format and clang-tidy are older than Irama's floor, and configuring stops on
# any it finds. The MSVC presets run neither, so the folders holding them leave the PATH.
$env:PATH = (
    $env:PATH.Split([IO.Path]::PathSeparator) | Where-Object {
        $folder = $_
        $llvm = @('clang-format.exe', 'clang-tidy.exe') |
            Where-Object { $folder -and (Test-Path -LiteralPath (Join-Path $folder $_)) }
        if ($llvm) {
            Write-Host "build: leaving $folder off the PATH, for its $($llvm -join ' and ')"
        }
        -not $llvm
    }
) -join [IO.Path]::PathSeparator
# Ahead of the CMake and Ninja that Visual Studio's own tools put on the PATH.
$env:PATH = "$(Join-Path $env:CI_TOOLS 'Scripts');$env:PATH"
cmake --workflow --preset $Preset
