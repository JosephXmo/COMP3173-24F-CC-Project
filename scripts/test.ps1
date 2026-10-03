param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$Compiler = '', [switch]$Strict, [switch]$NoBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$NoBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Compiler $Compiler -Strict:$Strict
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
$out = Join-Path $root ('build/' + $Configuration.ToLowerInvariant())
$binary = Join-Path $out 'uslc.exe'
$tests = Join-Path $out 'uslc_tests.exe'
function Invoke-Program([string[]]$Arguments, [string]$Output, [string]$InputText = $null) {
    New-Item -ItemType Directory -Force $Output | Out-Null
    $info = [Diagnostics.ProcessStartInfo]::new($binary)
    $info.WorkingDirectory = $root
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.RedirectStandardInput = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($info)
    $stdout = [IO.File]::Create((Join-Path $Output 'stdout'))
    $stderr = [IO.File]::Create((Join-Path $Output 'stderr'))
    try {
        $copyOut = $process.StandardOutput.BaseStream.CopyToAsync($stdout)
        $copyErr = $process.StandardError.BaseStream.CopyToAsync($stderr)
        if ($null -ne $InputText) { $process.StandardInput.Write($InputText) }
        $process.StandardInput.Close()
        $process.WaitForExit()
        $null = $copyOut.GetAwaiter().GetResult(); $null = $copyErr.GetAwaiter().GetResult()
        return $process.ExitCode
    } finally { $stdout.Dispose(); $stderr.Dispose(); $process.Dispose() }
}
function Compare-Files([string]$Expected, [string]$Actual, [switch]$Text) {
    $mode = if ($Text) { '--compare-text' } else { '--compare' }
    & $tests $mode $Expected $Actual
    if ($LASTEXITCODE -ne 0) { throw "Fixture differs: $Expected" }
}
Push-Location $root
try {
    & $tests
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $count = 0
    foreach ($source in (Get-ChildItem 'tests/fixtures/*.set' | Sort-Object Name)) {
        $base = Join-Path $source.DirectoryName $source.BaseName
        $output = Join-Path $out ('integration-windows/' + $source.BaseName)
        $artifactOutput = Join-Path $output 'artifacts'
        New-Item -ItemType Directory -Force $artifactOutput | Out-Null
        $argsList = @()
        if (Test-Path ($base + '.args')) { $argsList += [IO.File]::ReadAllLines($base + '.args') }
        if (Test-Path ($base + '.artifacts')) { $argsList += @('--emit-dir', $artifactOutput) }
        $argsList += 'tests/fixtures/' + $source.Name
        $actual = Invoke-Program $argsList $output
        $expected = [int][IO.File]::ReadAllText($base + '.exit')
        if ($actual -ne $expected) { throw "$($source.Name): expected exit $expected, got $actual" }
        Compare-Files ($base + '.stdout') (Join-Path $output 'stdout') -Text
        Compare-Files ($base + '.stderr') (Join-Path $output 'stderr') -Text
        if (Test-Path ($base + '.artifacts')) {
            $expectedFiles = @(Get-ChildItem ($base + '.artifacts/*.json') | Sort-Object Name)
            $actualFiles = @(Get-ChildItem (Join-Path $artifactOutput '*.json') | Sort-Object Name)
            if ($expectedFiles.Count -ne $actualFiles.Count) { throw 'Artifact count differs.' }
            foreach ($file in $expectedFiles) { Compare-Files $file.FullName (Join-Path $artifactOutput $file.Name) }
        }
        ++$count
    }
    Write-Host "$count integration fixtures passed"
    $cli = Join-Path $out 'cli-windows'
    $cases = @(
        @{ Code = 64; Arguments = @('--phase', 'invalid', 'examples/arithmetic.set') },
        @{ Code = 64; Arguments = @('--unknown') }, @{ Code = 64; Arguments = @() },
        @{ Code = 66; Arguments = @('tests/fixtures/does-not-exist.set') },
        @{ Code = 74; Arguments = @('--emit-dir', (Join-Path $cli 'does-not-exist/nested'), 'examples/arithmetic.set') },
        @{ Code = 0; Arguments = @('--help') }, @{ Code = 0; Arguments = @('--version') },
        @{ Code = 0; Arguments = @('--dump-grammar', '--dump-slr') },
        @{ Code = 0; Arguments = @('--trace-parser', 'examples/arithmetic.set') }
    )
    foreach ($case in $cases) {
        $actual = Invoke-Program $case.Arguments $cli
        if ($actual -ne $case.Code) { throw "CLI: expected $($case.Code), got $actual" }
    }
    $reused = Join-Path $cli 'reused'
    New-Item -ItemType Directory -Force $reused | Out-Null
    [IO.File]::WriteAllText((Join-Path $reused 'unrelated.txt'), "preserve`n")
    $actual = Invoke-Program @('--emit-dir', $reused, 'tests/fixtures/compact.set') $cli
    if ($actual -ne 0) { throw 'Initial artifact run failed' }
    $actual = Invoke-Program @('--emit-dir', $reused, 'tests/fixtures/lex-error.set') $cli
    $remaining = @(Get-ChildItem (Join-Path $reused '*.json'))
    if ($actual -ne 1 -or $remaining.Count -ne 1 -or $remaining[0].Name -ne 'diagnostics.json' -or
        !(Test-Path (Join-Path $reused 'unrelated.txt'))) { throw 'Stale artifact cleanup failed' }
    $actual = Invoke-Program @('-') $cli "show 3.`n"
    if ($actual -ne 0) { throw 'stdin failed' }
    Compare-Files 'tests/fixtures/stdin.stdout' (Join-Path $cli 'stdout') -Text
    Write-Host 'CLI checks passed'
} finally { Pop-Location }
