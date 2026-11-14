# Code signing of the energy suite

## Why self-signed

A public code signing certificate costs money every year and needs a registered company. We are a team of eight. A self-signed
certificate gives us what we need: Windows shows a stable publisher name, we can see that a file was not changed after the build,
and the programs can pin the certificate thumbprint. The price: each laptop must be told once to trust it.

## How the certificate was made

Darek ran `scripts\New-SigningCertificate.ps1` once, as himself. It made an RSA 3072 / SHA256 certificate, valid for 2 years, with the
subject `CN=EcoWattSense Code Signing, O=GCU Sector 12`. It wrote the public part to `signing\EcoWattSense-CodeSigning.cer`
(committed) and the private key to a `.pfx` outside the repository (never committed, `*.pfx` is in `.gitignore`).
The `.pfx` and its password are in the password manager of the team and in two GitHub secrets (see "Releases").

## Trusting the certificate on a laptop (once per laptop)

With the script, in an administrator PowerShell:

    pwsh -NoProfile -File client\spike\energy-suite\scripts\Install-SigningCertificate.ps1

It shows the subject and the thumbprint and asks Y/N. Check the thumbprint with Darek, then answer Y.

By hand: run `certlm.msc`, open "Trusted Root Certification Authorities", then Certificates, right-click, All Tasks, Import, choose
`EcoWattSense-CodeSigning.cer`. Repeat for "Trusted Publishers".

To take the trust away again: run the script with `-Remove`.

## First install without warnings

SmartScreen judges a file that comes from the internet by the cloud reputation of the file. A certificate that only your laptop trusts
does not change that. So for the first install:

1. Download `EcoWattSense-Web-Setup.exe`.
2. Right-click the file, Properties, tick "Unblock", OK.
3. Run it.

## How releases are signed

A GitHub Actions workflow runs on a tag push. It writes the secret `ENERGY_SUITE_SIGNING_PFX` (base64 of the `.pfx`) to a temporary
file and calls `scripts\Build-Package.ps1 -Sign -PfxPath <file>`. The password comes from the secret `ENERGY_SUITE_SIGNING_PASSWORD`,
passed as the environment variable of the same name. Build-Package signs our own programs and DLLs (not the .NET runtime) and the web
installer with a timestamp, before the ZIP and the checksums are made, and checks that every signature comes from the expected
thumbprint. The secrets are set with `gh secret set` (the commands are printed by `New-SigningCertificate.ps1`).
To sign on your own machine: set `ENERGY_SUITE_SIGNING_PASSWORD` in your session and give `-Sign -PfxPath <file>`. Never put the password on a command line.

## Certificate expires or leaks

The signatures carry a timestamp, so files signed before the expiry stay valid. New builds need a new certificate.

1. Darek runs `New-SigningCertificate.ps1` again (new thumbprint) and updates the password manager and the two GitHub secrets.
2. Commit the new `.cer`.
3. Every laptop: run `Install-SigningCertificate.ps1 -Remove` with the OLD `.cer`, then `Install-SigningCertificate.ps1` with the new one.
4. Put the new pinned thumbprint in the programs that check it, and release again.

If the `.pfx` or its password leaked, do steps 1 to 4 at once and tell the team: the old certificate must be removed from every laptop,
because anyone with the key can sign files that those laptops would trust.
