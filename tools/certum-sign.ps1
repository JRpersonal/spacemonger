<#
.SYNOPSIS
  Sign SpaceMonger executables with the Certum "Open Source" code signing
  certificate (Authenticode, SHA-256, RFC 3161 timestamp).

.DESCRIPTION
  Locates signtool.exe from the installed Windows SDK, signs every given file
  and verifies the result with the default Authenticode policy (/pa).

  The certificate is selected by its SHA-1 thumbprint. The private key lives in
  the Certum SimplySign cloud and is only reachable while a SimplySign Desktop
  session is open on this machine, so the script checks for that session first
  and fails with a clear message instead of a cryptic signtool error.

  Used by the sign-windows job in .github/workflows/release.yml (self-hosted
  runner labelled "signing") and usable by hand for a local test signature.

.PARAMETER File
  One or more PE files to sign.

.PARAMETER Thumbprint
  SHA-1 thumbprint of the code signing certificate. Defaults to $env:CERT_THUMBPRINT.

.EXAMPLE
  .\tools\certum-sign.ps1 -File dist\SpaceMonger-x64.exe -Thumbprint A797BE1F...
#>
param(
  [Parameter(Mandatory = $true)][string[]]$File,
  [string]$Thumbprint = $env:CERT_THUMBPRINT
)
$ErrorActionPreference = 'Stop'

if (-not $Thumbprint) {
  throw "No certificate thumbprint given (pass -Thumbprint or set CERT_THUMBPRINT)."
}
$Thumbprint = ($Thumbprint -replace '\s', '').ToUpperInvariant()

foreach ($f in $File) {
  if (-not (Test-Path -LiteralPath $f)) { throw "File not found: $f" }
}

# The cloud key is only usable through SimplySign Desktop. Without the process
# signtool fails with "No certificates were found that met all the given
# criteria" or a CSP error, which reads like a wrong thumbprint.
if (-not (Get-Process -Name 'SimplySignDesktop' -ErrorAction SilentlyContinue)) {
  throw "SimplySign Desktop is not running. Start it and log in with the SimplySign mobile app, then re-run the job."
}

$cert = Get-ChildItem Cert:\CurrentUser\My -ErrorAction SilentlyContinue |
  Where-Object { $_.Thumbprint -eq $Thumbprint } | Select-Object -First 1
if (-not $cert) {
  throw "Certificate $Thumbprint is not in Cert:\CurrentUser\My. Is the SimplySign session logged in (the virtual card is only mounted while it is)?"
}
if ($cert.NotAfter -lt (Get-Date).AddDays(14)) {
  Write-Warning "Code signing certificate expires on $($cert.NotAfter.ToString('yyyy-MM-dd')). Renew it at Certum."
}
Write-Host "Certificate: $($cert.Subject) (valid until $($cert.NotAfter.ToString('yyyy-MM-dd')))"

# Locate the newest x64 signtool.exe from the Windows SDK.
$signtool = $null
foreach ($r in @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "${env:ProgramFiles}\Windows Kits\10\bin")) {
  if (Test-Path -LiteralPath $r) {
    $cand = Get-ChildItem -Path $r -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
      Sort-Object FullName -Descending | Select-Object -First 1
    if ($cand) { $signtool = $cand.FullName; break }
  }
}
if (-not $signtool) {
  throw "signtool.exe not found. Install the Windows SDK (or Visual Studio Build Tools)."
}
Write-Host "Using signtool: $signtool"

# Certum's RFC 3161 timestamp authority. The timestamp keeps the signature
# valid after the certificate itself expires.
$timestamp = 'http://time.certum.pl'

foreach ($f in $File) {
  # The timestamp server is occasionally slow; retry before giving up.
  $ok = $false
  for ($attempt = 1; $attempt -le 3 -and -not $ok; $attempt++) {
    & $signtool sign /sha1 $Thumbprint /fd SHA256 /tr $timestamp /td SHA256 /d "SpaceMonger" /du "https://github.com/JRpersonal/spacemonger" /v "$f"
    if ($LASTEXITCODE -eq 0) { $ok = $true } else {
      Write-Warning "signtool sign failed for $f (attempt $attempt, exit $LASTEXITCODE)"
      Start-Sleep -Seconds (10 * $attempt)
    }
  }
  if (-not $ok) { throw "signtool sign failed for $f" }

  & $signtool verify /pa /v "$f"
  if ($LASTEXITCODE -ne 0) { throw "signtool verify failed for $f (exit $LASTEXITCODE)" }

  $sig = Get-AuthenticodeSignature -LiteralPath $f
  if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Thumbprint -ne $Thumbprint) {
    throw "Unexpected signature on ${f}: status=$($sig.Status) signer=$($sig.SignerCertificate.Thumbprint)"
  }
  Write-Host "Signed and verified: $f"
}
