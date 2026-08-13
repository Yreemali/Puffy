# Puffy installation and signing guides

These documents separate normal desktop application delivery from privileged
virtual-audio components. They are deliberately separate because signing an app
does not sign a driver, and optimism is not a certificate authority.

## Installation

- [Linux](INSTALL_LINUX.md)
- [Windows](INSTALL_WINDOWS.md)
- [macOS](INSTALL_MACOS.md)

## Signing and release

- [Linux artifacts](SIGN_LINUX.md)
- [Windows application and virtual-audio driver](SIGN_WINDOWS.md)
- [macOS application and virtual-audio component](SIGN_MACOS.md)
- [Release checklist](RELEASE_CHECKLIST.md)

Commands assume the repository root unless a guide changes directory. Never
commit certificates, private keys, provisioning files, API keys or passwords.
