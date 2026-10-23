<#
.SYNOPSIS
  Check or set the SpaceMonger version in every place it is recorded.

.DESCRIPTION
  The version lives in four places that must agree, or the signed executable
  shows one version in its file properties, another in the About dialog and a
  third on the release page:

    CMakeLists.txt   project(SpaceMonger VERSION X.Y.Z ...)
    SpaceMonger.rc   FILEVERSION / PRODUCTVERSION / "FileVersion" / "ProductVersion"
                     and the About dialog text "SpaceMonger Version X.Y.Z"
    README.TXT       "SpaceMonger Version X.Y.Z" header

  -Check (default) fails when they disagree, or when -Expected (a release tag
  such as v1.4.5) does not match. The release workflow runs it before building.
  -Set rewrites all of them; add a "Between ..." entry to README.TXT and a
  CHANGELOG.md section yourself.

.EXAMPLE
  .\tools\version.ps1                       # consistency check
  .\tools\version.ps1 -Expected v1.4.5      # check against a release tag
  .\tools\version.ps1 -Set 1.4.5            # bump everywhere
#>
param(
  [string]$Expected,
  [string]$Set
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$cmake = Join-Path $root 'CMakeLists.txt'
$rc = Join-Path $root 'SpaceMonger.rc'
$readme = Join-Path $root 'README.TXT'

function Read-Text($path) { [IO.File]::ReadAllText($path) }
# Keep each file's existing encoding-free bytes and CRLF endings: the sources
# are ASCII (.gitattributes pins eol=crlf), so a plain UTF-8 write without BOM
# is byte-identical outside the replaced version strings.
function Write-Text($path, $text) { [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding $false)) }

if ($Set) {
  if ($Set -notmatch '^(\d+)\.(\d+)\.(\d+)$') { throw "-Set expects X.Y.Z, got '$Set'" }
  $a, $b, $c = $Matches[1], $Matches[2], $Matches[3]

  $t = Read-Text $cmake
  $t = [regex]::Replace($t, '(project\(SpaceMonger VERSION )\d+\.\d+\.\d+', "`${1}$Set")
  Write-Text $cmake $t

  $t = Read-Text $rc
  $t = [regex]::Replace($t, '(FILEVERSION |PRODUCTVERSION )\d+,\d+,\d+,\d+', "`${1}$a,$b,$c,0")
  $t = [regex]::Replace($t, '(VALUE "(?:File|Product)Version", ")\d+, \d+, \d+, \d+', "`${1}$a, $b, $c, 0")
  $t = [regex]::Replace($t, '("SpaceMonger Version )\d+\.\d+\.\d+', "`${1}$Set")
  Write-Text $rc $t

  $t = Read-Text $readme
  $t = [regex]::Replace($t, '(SpaceMonger Version )\d+\.\d+\.\d+', "`${1}$Set")
  $t = [regex]::Replace($t, '(SpaceMonger )\d+\.\d+\.\d+( is optimized)', "`${1}$Set`${2}")
  Write-Text $readme $t
  Write-Host "Version set to $Set. Add release notes to CHANGELOG.md and README.TXT."
}

$found = [ordered]@{}
if ((Read-Text $cmake) -match 'project\(SpaceMonger VERSION (\d+\.\d+\.\d+)') { $found['CMakeLists.txt'] = $Matches[1] }
$rcText = Read-Text $rc
if ($rcText -match 'FILEVERSION (\d+),(\d+),(\d+),\d+') { $found['rc FILEVERSION'] = "$($Matches[1]).$($Matches[2]).$($Matches[3])" }
if ($rcText -match 'PRODUCTVERSION (\d+),(\d+),(\d+),\d+') { $found['rc PRODUCTVERSION'] = "$($Matches[1]).$($Matches[2]).$($Matches[3])" }
if ($rcText -match 'VALUE "FileVersion", "(\d+), (\d+), (\d+)') { $found['rc FileVersion string'] = "$($Matches[1]).$($Matches[2]).$($Matches[3])" }
if ($rcText -match 'VALUE "ProductVersion", "(\d+), (\d+), (\d+)') { $found['rc ProductVersion string'] = "$($Matches[1]).$($Matches[2]).$($Matches[3])" }
if ($rcText -match '"SpaceMonger Version (\d+\.\d+\.\d+)') { $found['rc About dialog'] = $Matches[1] }
if ((Read-Text $readme) -match 'SpaceMonger Version (\d+\.\d+\.\d+)') { $found['README.TXT'] = $Matches[1] }

$found.GetEnumerator() | ForEach-Object { Write-Host ("{0,-26} {1}" -f $_.Key, $_.Value) }
if ($found.Count -ne 7) { throw "Could not find every version location (found $($found.Count) of 7)." }
$distinct = @($found.Values | Sort-Object -Unique)
if ($distinct.Count -ne 1) { throw "Version mismatch: $($distinct -join ', ')" }
$version = $distinct[0]

if ($Expected) {
  $want = $Expected -replace '^v', '' -replace '-.*$', ''
  if ($want -ne $version) { throw "Release tag $Expected does not match the source version $version. Run tools\version.ps1 -Set $want first." }
}
Write-Host "Version $version is consistent."
if ($env:GITHUB_OUTPUT) { "version=$version" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8 }
