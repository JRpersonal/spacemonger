<#
.SYNOPSIS
  Start SpaceMonger.exe and make sure Windows actually runs it.

.DESCRIPTION
  Unit tests never load the application's embedded manifest. A malformed
  merged manifest (for example a duplicate <dpiAware> element) builds fine
  and passes every test, but Windows refuses to start the program with
  "the side-by-side configuration is incorrect". This script catches that:
  it checks the embedded manifest for duplicate settings, launches the
  executable, requires it to still be running after a few seconds, and then
  closes it.

.PARAMETER Path
  Path to SpaceMonger.exe.
#>
param(
  [Parameter(Mandatory = $true)][string]$Path,
  [int]$Seconds = 5,
  # ARM64 builds cannot run on the x64 CI runner; check the manifest only.
  [switch]$ManifestOnly
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Path)) { throw "File not found: $Path" }

# The embedded RT_MANIFEST resource is plain UTF-8 XML inside the PE file.
$text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path)))
$start = $text.IndexOf('<assembly')
if ($start -lt 0) { throw "No embedded application manifest found in $Path" }
$end = $text.IndexOf('</assembly>', $start)
$manifest = $text.Substring($start, $end - $start + '</assembly>'.Length)
foreach ($element in 'dpiAware', 'dpiAwareness', 'longPathAware', 'requestedExecutionLevel') {
  $count = ([regex]::Matches($manifest, "<$element[\s>/]")).Count
  if ($count -gt 1) { throw "Embedded manifest has $count <$element> elements; Windows will refuse to start it.`n$manifest" }
}
Write-Host "Embedded manifest OK."
if ($ManifestOnly) { return }

try {
  $p = Start-Process -FilePath $Path -PassThru
} catch {
  throw "Windows could not start ${Path}: $($_.Exception.Message)"
}
Start-Sleep -Seconds $Seconds
if ($p.HasExited) { throw "$Path exited after start with code $($p.ExitCode)." }
Stop-Process -Id $p.Id -Force
Write-Host "Started and running after $Seconds s: $Path"
