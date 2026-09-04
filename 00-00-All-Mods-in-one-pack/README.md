# 00-00:  All-In-One Custom Mod Pack for Kobo Clara Colour

An all-in-one custom package providing essential homebrew utilities, alternative readers, and advanced system tools pre-configured for the Kobo Clara Colour.

---

## Included Features & Components

* **Alternative Readers & Launchers:**
  * **KOReader (`v2026.07.1`):** Feature-rich document and e-book reader.
  * **Plato (`0.9.45`):** Lightweight, fast reading application.
  * **NickelMenu (`0.6.0`):** Custom user-interface menu integration for launching mods directly from Nickel.
  * **KFMon (`v1.4.6-191-gca31869`):** Launcher for third-party applications via image triggers.

* **Networking & Remote Access:**
  * **Dropbear SSH & SFTP Server:** Located in `/opt/dropbear/`. Fully active wrappers and pre-generated host keys matching `inetd.conf` parameters for immediate use.
  * **USB-SSH Module:** Located at `/usr/bin/usb-ssh` for establishing direct SSH sessions over USB connections.
  * **Native Telnet:** Pre-activated network interface daemon.

* **Python Environment:**
  * **Python 3.11.16:** Self-contained build located in `/mnt/onboard/.adds/.python/` with wrapper symlinks in `/usr/bin/`.
  * **Pip 26.2.1:** Configured with its cache directory directed to `/mnt/onboard/.adds/.python/` and fully mapped system wrappers.

* **System Tools & Utilities:**
  * **GNU Tar Binary:** Installed at `/usr/bin/gnu_tar` alongside BusyBox `tar` to enable full archive features.
  * **Universal Screen Capture:** Executable script at `/usr/bin/screenshot`. Saves captured PNG files directly to `/mnt/onboard/screenshots/`.
  * **DRM Management Test Utility:** Proof-of-concept tool for handling encrypted documents when provided with a custom client and `key.der`.

---

## Installation Guide

The release archive is split into two split volumes (`KoboRoot.tgz.001` and `KoboRoot.tgz.002`).

1. Download both volume parts (`KoboRoot.tgz.001` and `KoboRoot.tgz.002`) into the same directory on your PC.
2. Extract **`KoboRoot.tgz.001`** using an archive utility such as 7-Zip or PeaZip (or standard Linux file utilities). It will automatically read `.002` and output a single **`KoboRoot.tgz`** file.
3. Connect your Kobo Clara Colour to your computer via USB.
4. Copy the newly extracted `KoboRoot.tgz` directly into the **`.kobo`** hidden directory on your e-Reader's onboard memory.
5. Safely eject your Kobo device from your computer.
6. The e-Reader will automatically reboot and apply the update package.

---

## Technical Specifications & Compatibility Matrix

### Component Versions

| Component | Version |
| :--- | :--- |
| **KFMon** | `v1.4.6-191-gca31869` |
| **NickelMenu** | `0.6.0` |
| **KOReader** | `v2026.07.1` |
| **Plato** | `0.9.45` |
| **Python** | `3.11.16` |
| **Pip** | `26.2.1` |

### Tested Device Environment

| Component | Version |
| :--- | :--- |
| **Device** | Kobo Clara Colour |
| **Kernel** | 4.9.77 |
| **GLIBC** | 2.19 |
| **CPU** | ARMv7 Cortex-A53 |
| **Architecture** | `armhf` |
| **Primary Test Target** | `kobo-update-4.45.23697` (2026/May/25) |
| **Confirmed Functional** | `kobo-update-4.46.23836` (2026/Aug/17) |

---

## Toolchain & Compilation Sources

Custom binaries included in this package (such as GNU `tar` and `dropbear`) were compiled specifically for this platform using an embedded custom cross-toolchain matching the system environment.

### Cross-Toolchain Specifications

| Component | Version |
| :--- | :--- |
| **GLIBC** | 2.19 |
| **Linux Headers** | 3.2.101 |
| **GCC** | 9.5.0 |
| **Binutils** | 2.32 |

### Upstream Binary Sources

| Utility / Dependency | Version |
| :--- | :--- |
| **GNU Tar** | `tar-1.35` |
| **OpenSSL** | `openssl-1.1.1w` |
| **Dropbear** | `dropbear-2026.94` |


