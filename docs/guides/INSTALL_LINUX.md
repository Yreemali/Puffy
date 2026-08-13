# Installing Puffy on Linux

Linux is Puffy's primary development platform. PipeWire supplies physical
capture, monitoring and the `Puffy Virtual Microphone` source.

## Arch Linux package

Install the build requirements:

```bash
sudo pacman -S --needed base-devel cmake ninja git cargo nodejs npm \
  webkit2gtk-4.1 gtk3 sqlite libsndfile pipewire libx11 libxtst
```

Build and install as a regular user. Do not run `makepkg` as root:

```bash
cd packaging/arch
makepkg -si
```

To install an already built local package:

```bash
sudo pacman -U ./puffy-0.2.0-1-x86_64.pkg.tar.zst
```

## AppImage

Build:

```bash
cd ui/web
npm ci
npm run tauri:build -- --bundles appimage
```

Run the result without installing it:

```bash
chmod +x src-tauri/target/release/bundle/appimage/Puffy_*.AppImage
./src-tauri/target/release/bundle/appimage/Puffy_*.AppImage
```

AppImages should be built on the oldest Linux baseline intended for support;
glibc compatibility travels forward much more reliably than backward.

## First launch

1. Confirm that PipeWire is running with `systemctl --user status pipewire`.
2. Start Puffy and select the physical microphone and monitoring output.
3. Select `Puffy Virtual Microphone` in Discord, OBS or another client.
4. On Wayland, global key access depends on compositor policy. Puffy does not
   bypass compositor security. An evdev fallback may require explicit device
   permissions configured by the administrator.

Application data is stored in `$XDG_DATA_HOME/Puffy` or
`~/.local/share/Puffy`.

## Uninstall

For the Arch package:

```bash
sudo pacman -Rns puffy
```

Removing the package does not remove the local sound library or profile. Delete
the Puffy data directory manually only if that data is no longer needed.

## References

- [Tauri prerequisites](https://v2.tauri.app/start/prerequisites/)
- [Tauri AppImage distribution](https://v2.tauri.app/distribute/appimage/)
- [Arch makepkg guide](https://wiki.archlinux.org/title/Makepkg)
