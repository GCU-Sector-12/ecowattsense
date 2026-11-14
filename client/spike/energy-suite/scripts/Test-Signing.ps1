# Test-Signing.ps1: tests the signing functions of Build-Package.ps1 with a throw-away certificate.
# Who uses it: whoever changes the signing code. It needs the Windows SDK (signtool.exe) and network access to the timestamp server.
# How to run: pwsh -NoProfile -File Test-Signing.ps1
# What it does: makes a throw-away certificate, exports it to a temporary .pfx (random password in an environment variable),
#   removes it from the store, then runs the REAL functions of Build-Package.ps1 (read from the file, not copied) on a copy of
#   the unsigned .NET apphost.exe template. It checks the signer thumbprint, a wrong thumbprint and an unsigned file. Everything it made is deleted at the end,
#   also when a check fails. It never touches the LocalMachine stores.
# Exit codes: 0 all checks passed; 1 a check failed.
$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot 'Build-Package.ps1'
$work = Join-Path ([IO.Path]::GetTempPath()) ("ews-sign-test-" + [guid]::NewGuid().ToString('N'))
$subject = 'CN=EcoWattSense TEST Throwaway ' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$passed = 0
$failed = 0
$thumbprint = ''

<#
.SYNOPSIS Records one check result and prints it.
.PARAMETER Name What was checked.
.PARAMETER Condition $true when the check passed.
#>
function Assert-Check {
    param([string] $Name, [bool] $Condition)
    if ($Condition) { $script:passed++; Write-Host "PASS $Name" }
    else { $script:failed++; Write-Host "FAIL $Name" }
}

try {
    # The real functions are read from Build-Package.ps1 (parsed, not run) so that the test cannot drift from the code.
    $ast = [System.Management.Automation.Language.Parser]::ParseFile($buildScript, [ref]$null, [ref]$null)
    $all = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true)
    $signingNames = 'Find-SignTool', 'Import-SigningPfx', 'Invoke-SignFiles', 'Get-WronglySigned', 'Invoke-SigningRun'
    $defs = @($all | Where-Object { $signingNames -contains $_.Name })
    Assert-Check 'five signing functions found in Build-Package.ps1' ($defs.Count -eq 5)
    $text = ($defs | ForEach-Object { $_.Extent.Text }) -join "`n"
    Invoke-Expression $text

    New-Item -ItemType Directory -Path $work -Force | Out-Null
    $signTool = Find-SignTool
    Assert-Check 'signtool.exe found' ([bool]$signTool)

    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 `
        -KeyExportPolicy Exportable -NotAfter (Get-Date).AddDays(1) -CertStoreLocation 'Cert:\CurrentUser\My'
    $thumbprint = $cert.Thumbprint
    $passwordText = [guid]::NewGuid().ToString('N') + 'Aa1!'
    $env:EWS_TEST_SIGNING_PASSWORD = $passwordText
    $pfx = Join-Path $work 'test.pfx'
    $secure = ConvertTo-SecureString -String $env:EWS_TEST_SIGNING_PASSWORD -AsPlainText -Force
    Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $secure | Out-Null
    Remove-Item -LiteralPath "Cert:\CurrentUser\My\$thumbprint" -Force
    Assert-Check 'certificate left the store before signing (import path is tested)' (-not (Test-Path -LiteralPath "Cert:\CurrentUser\My\$thumbprint"))

    $signed = Join-Path $work 'signed.exe'
    $unsigned = Join-Path $work 'unsigned.exe'
    # whoami.exe is catalog-signed by Microsoft and cannot be stripped, so the unsigned .NET apphost template is the test file.
    $source = Get-ChildItem -Path (Join-Path $env:ProgramFiles 'dotnet\sdk\*\AppHostTemplate\apphost.exe') | Select-Object -First 1
    Assert-Check 'an unsigned test exe was found' ($null -ne $source)
    Copy-Item -LiteralPath $source.FullName -Destination $signed
    Copy-Item -LiteralPath $source.FullName -Destination $unsigned

    Assert-Check 'the test copies start unsigned' ((Get-AuthenticodeSignature -LiteralPath $signed).Status -eq 'NotSigned')

    Invoke-SigningRun -Files @($signed) -Pfx $pfx -PasswordText $env:EWS_TEST_SIGNING_PASSWORD -TimestampUrl 'http://timestamp.digicert.com'
    Assert-Check 'Invoke-SigningRun finished without error' $true
    Assert-Check 'signer thumbprint equals the .pfx certificate' (@(Get-WronglySigned -Files @($signed) -Thumbprint $thumbprint).Count -eq 0)
    Assert-Check 'a wrong thumbprint is reported' (@(Get-WronglySigned -Files @($signed) -Thumbprint ('0' * 40)).Count -eq 1)
    Assert-Check 'an unsigned file is reported' (@(Get-WronglySigned -Files @($unsigned) -Thumbprint $thumbprint).Count -eq 1)
    Assert-Check 'the certificate was removed from the store again' (-not (Test-Path -LiteralPath "Cert:\CurrentUser\My\$thumbprint"))
    $stamp = (Get-AuthenticodeSignature -LiteralPath $signed).TimeStamperCertificate
    Assert-Check 'the signature carries a timestamp' ($null -ne $stamp)
}
catch {
    $failed++
    Write-Host "FAIL unexpected error: $($_.Exception.Message)"
}
finally {
    Remove-Item Env:\EWS_TEST_SIGNING_PASSWORD -ErrorAction SilentlyContinue
    if ($thumbprint) { Remove-Item -LiteralPath "Cert:\CurrentUser\My\$thumbprint" -Force -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue }
}

# Count only the throw-away certificate of this test: the team's real signing certificate may live in the same store.
$leftCert = @(Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Thumbprint -eq $thumbprint }).Count
$leftDir = Test-Path -LiteralPath $work
Write-Host "Passed: $passed  Failed: $failed  Certificates left: $leftCert  Temp folder left: $leftDir"
if ($failed -eq 0 -and $leftCert -eq 0 -and -not $leftDir) { exit 0 }
exit 1
