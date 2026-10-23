# Compiles win\deckctl.cs into win\deckctl.exe using the in-box .NET Framework
# compiler. No SDK required. Re-run after editing deckctl.cs.
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$csc = Join-Path $env:SystemRoot "Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if (-not (Test-Path $csc)) { throw "csc.exe not found at $csc" }

& $csc /nologo /target:exe /optimize+ /out:"$here\deckctl.exe" "$here\deckctl.cs"
if ($LASTEXITCODE -ne 0) { throw "csc failed with exit code $LASTEXITCODE" }
"built $here\deckctl.exe"
