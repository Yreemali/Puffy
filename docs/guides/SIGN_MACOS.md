# Signing and notarizing Puffy for macOS

The application and virtual-audio component are separate signing targets. Direct
distribution requires an Apple Developer account, a Developer ID Application
identity and notarization.

## Sign and notarize the application

Install a valid Developer ID Application certificate in the login keychain and
confirm that codesign can see it:

```bash
security find-identity -v -p codesigning
```

Export the identity for Tauri without committing it:

```bash
export APPLE_SIGNING_IDENTITY="Developer ID Application: Your Name (TEAMID)"
```

For notarization, prefer an App Store Connect API key stored outside the
repository:

```bash
export APPLE_API_ISSUER="YOUR_ISSUER_ID"
export APPLE_API_KEY="YOUR_KEY_ID"
export APPLE_API_KEY_PATH="/secure/path/AuthKey_YOUR_KEY_ID.p8"
```

Then build the signed and notarized DMG:

```bash
cd ui/web
npm run tauri:build -- --bundles app,dmg
```

Verify the result:

```bash
codesign --verify --deep --strict --verbose=2 \
  src-tauri/target/release/bundle/macos/Puffy.app
spctl --assess --type execute --verbose=4 \
  src-tauri/target/release/bundle/macos/Puffy.app
xcrun stapler validate src-tauri/target/release/bundle/dmg/Puffy_*.dmg
```

Apple requires valid signatures for all shipped executable code, Hardened
Runtime, a secure timestamp and an appropriate Developer ID identity before
notarization. Ad-hoc signing is suitable only for local development and does not
replace notarization.

## Sign the virtual-audio component

The Audio Server Plug-in/DriverKit extension needs its own bundle identifier,
provisioning and entitlements. Follow Apple's current Audio DriverKit sample and
request required entitlements through the developer account. Test local signing
only on a dedicated development Mac.

Before distribution:

1. Sign every nested framework, helper, plug-in and extension with compatible
   identities and entitlements.
2. Sign the outer app/package last.
3. Notarize the complete distributed artifact.
4. Staple the notarization ticket where supported.
5. Test installation on a clean Mac with SIP and normal security enabled.

Do not ship instructions that require users to disable SIP. Apple's sample uses
that only for restricted local ad-hoc development scenarios.

## CI secrets

Store the P12, its password, notarization credentials and API key as protected
CI secrets. The default Puffy workflow deliberately builds an unsigned `.app`
until repository owners configure release credentials.

## References

- [Tauri macOS signing and notarization](https://v2.tauri.app/distribute/sign/macos/)
- [Apple notarization requirements](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
- [Apple Audio Server Plug-in and Driver Extension](https://developer.apple.com/documentation/coreaudio/building-an-audio-server-plug-in-and-driver-extension)
