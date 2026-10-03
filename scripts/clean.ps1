$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$target = [IO.Path]::GetFullPath((Join-Path $root 'build'))
if ((Split-Path $target -Parent) -ne $root) { throw 'Refusing to remove a path outside the project.' }
if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
