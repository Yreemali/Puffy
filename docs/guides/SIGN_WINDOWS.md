# Signing Puffy for Windows

There are two independent signatures:

1. Authenticode signing for `Puffy.exe`, `puffy_native.dll` and the NSIS
   installer.
2. Microsoft dashboard signing for the virtual-audio driver package.

Completing the first does not make the second loadable by Windows.

## Sign the desktop application

Use a trusted code-signing certificate, not a TLS certificate. Import the PFX
into the current user's Personal certificate store without committing it:

```powershell
$securePassword = Read-Host "PFX password" -AsSecureString
Import-PfxCertificate -FilePath .\PuffySigning.pfx `
  -CertStoreLocation Cert:\CurrentUser\My `
  -Password $securePassword
Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert
```

Pass the certificate identity to Puffy's platform-aware Tauri wrapper. The
timestamp URL must be the RFC 3161 service documented by the certificate
authority:

```powershell
Set-Location ui\web
$env:PUFFY_WINDOWS_CERTIFICATE_THUMBPRINT = "YOUR_CERTIFICATE_THUMBPRINT"
$env:PUFFY_WINDOWS_TIMESTAMP_URL = "YOUR_CA_RFC3161_TIMESTAMP_URL"
npm run tauri:build -- --bundles nsis
```

Alternatively, sign final files with Windows SDK SignTool. Use the timestamp URL
provided by the certificate authority:

```powershell
signtool sign /a /fd SHA256 /tr "YOUR_CA_RFC3161_TIMESTAMP_URL" /td SHA256 Puffy.exe
signtool verify /pa /all /v Puffy.exe
```

Sign binaries before packaging and sign the final installer after packaging.
Hardware-backed or remote signing is preferable to exporting a reusable private
key into CI.

## Sign the virtual-audio driver

Production Windows 10/11 systems require Microsoft-signed new kernel drivers.
The release path is:

1. Build the driver and INF with the matching WDK.
2. Generate and validate the catalog package.
3. Run Driver Verifier and audio-specific tests on dedicated machines.
4. Run HLK and submit the results through Hardware Dev Center for the preferred
   production path.
5. Download the Microsoft-signed package and verify its catalog signature.
6. Install using the signed INF package and test Secure Boot/HVCI scenarios.

Attestation signing is limited to testing scenarios and does not replace HLK for
retail Windows Update distribution. Do not publish test certificates or tell
users to disable Secure Boot/signature enforcement.

## CI secrets

Never commit PFX/P12 files, passwords or certificate blobs. Use protected
environments and secret storage. The repository's default CI intentionally
produces unsigned artifacts until release signing is configured by the owner.

## References

- [Tauri Windows code signing](https://v2.tauri.app/distribute/sign/windows/)
- [Microsoft SignTool](https://learn.microsoft.com/windows/win32/seccrypto/signtool)
- [Microsoft driver signing options](https://learn.microsoft.com/windows-hardware/drivers/dashboard/driver-signing-offerings)
