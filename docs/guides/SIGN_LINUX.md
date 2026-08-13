# Signing Linux artifacts

Linux does not require one universal platform signature, but release artifacts
should still be verifiable.

## AppImage signing

Create or select a dedicated release GPG key. Keep its private material outside
the repository and publish the fingerprint over an authenticated channel.

Build a signed AppImage:

```bash
cd ui/web
export SIGN=1
export SIGN_KEY="YOUR_LONG_GPG_KEY_ID"
export APPIMAGETOOL_FORCE_SIGN=1
npm run tauri:build -- --bundles appimage
```

Allow GPG to prompt locally. In CI, store the passphrase and private key only in
the CI secret store; never put either value in workflow YAML.

Display the embedded signature:

```bash
./src-tauri/target/release/bundle/appimage/Puffy_*.AppImage --appimage-signature
```

An AppImage does not automatically validate its own embedded signature. Publish
SHA-256 checksums and detached GPG signatures alongside every release as an
additional verification path:

```bash
sha256sum Puffy_*.AppImage > SHA256SUMS
gpg --armor --detach-sign SHA256SUMS
```

## Arch package

Sign the built package using the maintainer's configured makepkg GPG key and
publish the corresponding public-key fingerprint. Package signing policy should
be documented before any AUR or repository publication.

## Reference

- [Tauri Linux code signing](https://v2.tauri.app/distribute/sign/linux/)
