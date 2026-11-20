# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Checks that a Windows machine keeps the promises CI relies on, once the job has set it up: the
# MSVC toolset the job names, building a C++23 program that runs, and the CMake and Ninja the
# presets call. It reports every broken promise, not just the first.
param(
    # The MSVC toolset the job builds with, such as 14.52.
    [Parameter(Mandatory)]
    [ValidatePattern('^14\.\d{2}$')]
    [string] $MsvcVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'enter-msvc.ps1') -Version $MsvcVersion
# Ahead of the CMake and Ninja that Visual Studio's own tools put on the PATH.
$env:PATH = "$(Join-Path $env:CI_TOOLS 'Scripts');$env:PATH"

$failures = 0

function Fail([string] $Message) {
    [Console]::Error.WriteLine("check-runner: $Message")
    $script:failures++
}

# Runs a program and returns the lines it printed on stdout and stderr, or reports why it couldn't
# run or failed and returns $null.
function Invoke-Program([string] $What, [string] $Program, [string[]] $Arguments) {
    if (-not (Get-Command $Program -ErrorAction SilentlyContinue)) {
        Fail "${What}: $Program isn't on the PATH"
        return $null
    }
    $lines = @(& $Program @Arguments 2>&1 | ForEach-Object { "$_" })
    if ($LASTEXITCODE -ne 0) {
        Fail "${What}: ``$Program $Arguments`` failed with ${LASTEXITCODE}: $($lines -join "`n")"
        return $null
    }
    return , $lines
}

# Runs a program and expects a line of its output to match a regular expression.
function Expect([string] $What, [string] $Pattern, [string] $Program, [string[]] $Arguments) {
    $lines = Invoke-Program $What $Program $Arguments
    if ($null -ne $lines -and ($lines -join "`n") -notmatch "(?m)$Pattern") {
        Fail "${What}: ``$Program $Arguments`` printed: $($lines -join "`n")"
    }
}

# cl's own version runs five majors ahead of the toolset's, and _MSC_VER is that version's major
# and minor run together: 1952 for MSVC 14.52.
$expectedMscVer = 1900 + [int] $MsvcVersion.Split('.')[1]

$work = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid()))
Push-Location $work

# The toolset, as the developer command prompt set it up, then as cl itself reports it while it
# builds C++23 with the standard library. C++23 is /std:c++latest, as CMake asks MSVC for it.
if ("$env:VCToolsVersion" -notlike "$MsvcVersion.*") {
    Fail "the environment isn't set up for MSVC ${MsvcVersion}: VCToolsVersion is '$env:VCToolsVersion'"
}
Set-Content -Path program.cpp -Value @'
#include <print>

static_assert(_MSC_VER == EXPECTED_MSC_VER, "cl is the MSVC toolset the job names");

int main() { std::println("{}", 42); }
'@
$flags = @('/nologo', '/std:c++latest', '/EHsc', "/DEXPECTED_MSC_VER=$expectedMscVer")
$built = Invoke-Program "MSVC $MsvcVersion" 'cl' ($flags + @('program.cpp', '/Fe:program.exe'))
if ($null -ne $built) {
    Expect "MSVC $MsvcVersion" '^42$' (Join-Path $work 'program.exe') @()
}

# The build tools the presets name. The job's CMake must come first, ahead of any older one.
$cmake = Invoke-Program 'CMake is 4.4 or newer' 'cmake' @('--version')
if ($null -ne $cmake) {
    if (($cmake -join "`n") -notmatch '^cmake version (\d+\.\d+)') {
        Fail "CMake is 4.4 or newer: ``cmake --version`` printed: $($cmake -join "`n")"
    } elseif ([version] $Matches[1] -lt [version] '4.4') {
        Fail "CMake is 4.4 or newer: the CMake on the PATH is $($Matches[1]), at $((Get-Command cmake).Source)"
    }
}
Expect 'Ninja is on the PATH' '^\d+\.\d+' 'ninja' @('--version')

Pop-Location
Remove-Item -Recurse -Force $work

if ($failures -ne 0) {
    [Console]::Error.WriteLine("check-runner: promises broken: $failures")
    exit 1
}
Write-Output 'check-runner: every promise holds'
