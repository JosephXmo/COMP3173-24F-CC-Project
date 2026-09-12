param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$Compiler = '',
    [switch]$Strict
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root ('build/' + $Configuration.ToLowerInvariant())
$obj = Join-Path $out 'obj'
New-Item -ItemType Directory -Force $obj | Out-Null
if (!$Compiler) {
    foreach ($candidate in @('clang.exe', 'gcc.exe', 'cl.exe')) {
        $found = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($found) { $Compiler = $found.Source; break }
    }
}
if (!$Compiler) { throw 'Install Clang 18+ or run from Developer PowerShell with cl.exe available.' }
$msvc = [IO.Path]::GetFileName($Compiler) -eq 'cl.exe'
if ($Strict -and $msvc) { throw 'MSVC is a compatibility toolchain; strict C23 requires Clang 18+ or GCC 14+.' }
function Invoke-Compiler([string[]]$Arguments) {
    Write-Host ($Compiler + ' ' + (($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '))
    & $Compiler @Arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if ($msvc) {
    $flags = @('/nologo', '/std:clatest', '/W4', '/permissive-', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS')
    $alignmentProbe = Join-Path $out 'alignment-probe.c'
    [IO.File]::WriteAllText($alignmentProbe, "#include <stddef.h>`nmax_align_t uslc_alignment_probe;`n")
    $probeArgs = @('/nologo', '/std:clatest', '/c', $alignmentProbe, ('/Fo' + (Join-Path $obj 'alignment-probe.obj')))
    Write-Host ($Compiler + ' ' + ($probeArgs -join ' '))
    & $Compiler @probeArgs *> (Join-Path $out 'alignment-probe.log')
    if ($LASTEXITCODE -ne 0) { $flags += '/DUSLC_NO_MAX_ALIGN_T' }
    if ($Configuration -eq 'Debug') { $flags += @('/Od', '/Zi', ('/Fd' + (Join-Path $out 'compiler.pdb'))) }
    else { $flags += @('/O2', '/DNDEBUG') }
    Write-Host 'MSVC compatibility build (not a strict C23 gate).'
} else {
    $probe = Join-Path $out 'c23-probe.c'
    [IO.File]::WriteAllText($probe, "#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L`n#error C23 is required`n#endif`nint main(void) { return 0; }`n")
    Invoke-Compiler @('-std=c23', '-c', $probe, '-o', (Join-Path $obj 'c23-probe.obj'))
    $flags = @('-std=c23', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-D_CRT_SECURE_NO_WARNINGS')
    if ($Configuration -eq 'Debug') { $flags += @('-O0', '-g3') } else { $flags += @('-O2', '-DNDEBUG') }
}
$sources = @(Get-ChildItem (Join-Path $root 'src/*.c') | Sort-Object Name)
$tests = @(Get-ChildItem (Join-Path $root 'tests/test_*.c') | Sort-Object Name)
foreach ($file in ($sources + $tests)) {
    $object = Join-Path $obj ($file.BaseName + '.obj')
    if ($msvc) { $argsList = $flags + @(('/I' + (Join-Path $root 'include')), ('/I' + (Join-Path $root 'tests')), '/c', $file.FullName, ('/Fo' + $object)) }
    else { $argsList = $flags + @(('-I' + (Join-Path $root 'include')), ('-I' + (Join-Path $root 'tests')), '-c', $file.FullName, '-o', $object) }
    Invoke-Compiler $argsList
}
foreach ($target in @('uslc', 'uslc_tests')) {
    if ($target -eq 'uslc') { $inputs = $sources }
    else { $inputs = @($sources | Where-Object BaseName -ne 'main') + $tests }
    $objects = @($inputs | ForEach-Object { Join-Path $obj ($_.BaseName + '.obj') })
    $binary = Join-Path $out ($target + '.exe')
    if ($msvc) {
        $linkFlags = @('/link', ('/PDB:' + (Join-Path $out ($target + '.pdb'))))
        if ($Configuration -eq 'Debug') { $linkFlags += '/DEBUG' }
        Invoke-Compiler (@('/nologo') + $objects + @(('/Fe' + $binary)) + $linkFlags)
    }
    else { Invoke-Compiler ($objects + @('-o', $binary)) }
}
Write-Host "Built $Configuration with $Compiler"
