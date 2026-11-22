# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-SignatureCheck.ps1
# Compiles the real SignatureRules and SigningPin (no WPF, no WinVerifyTrust call) and checks the decision about the signature
# of the extracted Setup.exe: valid and matching, untrusted root, other thumbprint, no signature, tampered file, placeholder pin.
$ErrorActionPreference = 'Stop'
$update = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Update'
Add-Type -Path (Join-Path $update 'SigningPin.cs'), (Join-Path $update 'SignatureRules.cs')
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$rules = [EcoWattSense.Client.Gui.Update.SignatureRules]
$pinType = [EcoWattSense.Client.Gui.Update.SigningPin]
$pin = '0123456789abcdef0123456789abcdef01234567'
$other = 'f' * 40

function Decide($trust, $thumbprint, $pinValue) { return $rules::Decide($trust, $thumbprint, $pinValue).ToString() }

Check ((Decide $rules::TrustOk $pin $pin) -eq 'Start') 'valid and matching starts'
Check ((Decide $rules::TrustOk $pin.ToUpperInvariant() $pin) -eq 'Start') 'thumbprint case does not matter'
Check ((Decide $rules::TrustUntrustedRoot $pin $pin) -eq 'Start') 'untrusted root and matching starts'
Check ((Decide $rules::TrustChaining $pin $pin) -eq 'Start') 'chaining result and matching starts'
Check ((Decide $rules::TrustOk $other $pin) -eq 'Refuse') 'valid but another thumbprint is refused'
Check ((Decide $rules::TrustUntrustedRoot $other $pin) -eq 'Refuse') 'untrusted root and another thumbprint is refused'
Check ((Decide $rules::TrustNoSignature $null $pin) -eq 'Refuse') 'no signature is refused'
Check ((Decide $rules::TrustOk $null $pin) -eq 'Refuse') 'no signer certificate is refused'
Check ((Decide $rules::TrustBadDigest $pin $pin) -eq 'Refuse') 'a tampered file is refused'
Check ((Decide ([int]0x80092003 -bor 0) $pin $pin) -eq 'Refuse') 'another failure is refused'
Check ((Decide $rules::TrustOk $pin 'abc') -eq 'Refuse') 'a malformed pin is refused'
Check ((Decide $rules::TrustNoSignature $null $pinType::Placeholder) -eq 'Skipped') 'the placeholder pin skips an unsigned file'
Check ((Decide $rules::TrustOk $pin $pinType::Placeholder) -eq 'Skipped') 'the placeholder pin skips a signed file'
Check ($rules::RefusedNote -eq 'The downloaded Setup is not signed by EcoWattSense. It was not started.') 'the refusal line'
Check ($rules::TrustBadDigest -eq [int]0x80096010) 'TRUST_E_BAD_DIGEST value'

Write-Output "Passed $checks signature checks."
