# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Copies the logs a failed job leaves into a directory, for CircleCI to keep, under their paths in
# the checkout: each build's configure log, test logs, JUnit file, vcpkg install log and the output
# of failed golden tests. vcpkg's logs of the ports it built go under vcpkg/.
#
#   ci/windows/gather-logs.ps1 logs
param(
    [Parameter(Mandatory)]
    [string] $Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$patterns = @(
    'build/*/CMakeFiles/CMakeConfigureLog.yaml'
    'build/*/Testing/Temporary'
    'build/*/junit.xml'
    'build/*/vcpkg-manifest-install.log'
    'build/*/golden-failures'
)
foreach ($pattern in $patterns) {
    foreach ($item in @(Get-Item -Path $pattern -ErrorAction SilentlyContinue)) {
        $relative = [IO.Path]::GetRelativePath($PWD.Path, $item.FullName)
        $target = Join-Path $Destination (Split-Path -Parent $relative)
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        Copy-Item -Recurse -Force -Path $item.FullName -Destination $target
    }
}
$buildLogs = Join-Path $env:VCPKG_ROOT 'buildtrees/*/*.log'
foreach ($log in @(Get-Item -Path $buildLogs -ErrorAction SilentlyContinue)) {
    $target = Join-Path $Destination "vcpkg/$($log.Directory.Name)"
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item -Force -Path $log.FullName -Destination $target
}
