# Install SSH Server (Dropbear)

This guide explains how to compile a modern **Dropbear SSH server** for the Kobo and integrate it with the existing `inetd` setup from the [previous step](https://github.com/alexandrglm/kobo-clara-colour-mods-telnet-ssh-debian-chroot/tree/main/00-02-Getting-TELNET/Step-4_OPT_partition_FW_edition) (remember that Telnet is needed to perform the final dropbear touchs for the first time)

- Dropbear is chosen over OpenSSH because it is significantly **simpler to compile**  due to minimal dependencies, and does not require external libraries like OpenSSL or zlib.
- Two toolchain options are provided: a generic ARM toolchain and the exact Kobo Clara Colour toolchain.
- SCP is supported but requires the `-O` flag to force legacy SCP mode.
- For SFTP support, OpenSSH would need to be compiled separately.
  
> [!WARNING]
> ## ⚠️ SSH on Modern Kobo Firmware
>
> Recent Kobo firmware includes a **disabled built-in OpenSSH server**, which can
> be enabled by creating `/mnt/onboard/.kobo/ssh-enabled` and rebooting.
>
> **We do NOT use it.**
>
> The bundled OpenSSH version is **outdated** and may contain known security
> vulnerabilities. Its configuration is also opaque, making it difficult to
> properly audit its security and network exposure.
>

> Instead, we compile and install the **latest Dropbear**, a lightweight SSH
> server designed for embedded systems, and integrate it with Kobo's existing
> **`inetd`** setup.
>
> This gives us a **current, controlled and auditable SSH implementation**.
>

> [!IMPORTANT]
> **Do not create `/mnt/onboard/.kobo/ssh-enabled`.**
> This enables Kobo's built-in OpenSSH, which this setup intentionally avoids.


The result is a lightweight SSH server running directly on the Kobo, providing a more secure alternative to Telnet.

---

## Prerequisites

*   A Kobo with **root Telnet access** from the [previous step](https://github.com/alexandrglm/kobo-clara-colour-mods-telnet-ssh-debian-chroot/tree/main/00-02-Getting-TELNET/Step-4_OPT_partition_FW_edition).
*   Telnet access for the first time (or KOreader terminal emulator).
*   An **ARM cross-compilation toolchain**.

---

## About the toolchain

Dropbear can be compiled using either:
-    The generic ARM toolchain available
-    The exact toolchain matching the Kobo's glibc version, explained [here](https://github.com/alexandrglm/kobo-clara-colour-toolchain-glibc2.19).

Both approaches work, but the specific toolchain provides maximum compatibility.

### Option A: Generic Modern Toolchain (Simpler)

This is the easiest method and works in most cases.

```bash
sudo dpkg --add-architecture armhh
sudo apt update

sudo apt install gcc-arm-linux-gnueabihf
```

The compiler is invoked via `arm-linux-gnueabihf-gcc`, check it:
```bash
$ arm-linux-gnueabihf-gcc --version

arm-linux-gnueabihf-gcc (Debian 14.2.0-19) 14.2.0
```

### Option B: Kobo Clara Colour toolchain `glibc` v219

The toolchain repository and setup instructions are available at:

[https://github.com/alexandrglm/kobo-clara-colour-toolchain-glibc2.19](https://github.com/alexandrglm/kobo-clara-colour-toolchain-glibc2.19)

Clone and set up the toolchain:

```bash
git clone https://github.com/alexandrglm/kobo-clara-colour-toolchain-glibc2.19.git
cd kobo-clara-colour-toolchain-glibc2.19
```

Once installed, set the environment variables:

```bash
export PATH="/path/to/x-tools/arm-unknown-linux-gnueabihf/bin:${PATH}"
export CC=arm-unknown-linux-gnueabihf-gcc
export AR=arm-unknown-linux-gnueabihf-ar
export RANLIB=arm-unknown-linux-gnueabihf-ranlib
```

The compiler is invoked as `arm-unknown-linux-gnueabihf-gcc`, check it:
```bash
$ arm-unknown-linux-gnueabihf-gcc --version

arm-unknown-linux-gnueabihf-gcc (crosstool-NG 1.29.0.3_2e5d0b8) 9.5.0
```

### Option C: Precompiled Binaries

Precompiled Dropbear binaries for ARM may exist in Alpine Linux repositories or other ARM distributions. However, they are not recommended because:

- They may be compiled against a different glibc version, causing runtime errors.
- They may not include all desired features (like `scp`).
- They are not tailored to the Kobo's specific environment.

**Compiling from source is the recommended approach.**

---

# Steps

## 1. Download Dropbear

Download the latest Dropbear source code from the [official Dropbear website](https://matt.ucc.asn.au/dropbear/dropbear.html).

> [!IMPORTANT]
> This guide targets **Dropbear 2026.94 or later**.

Extract the source archive and enter the resulting directory:

```bash
wget https://matt.ucc.asn.au/dropbear/releases/dropbear-2026.94.tar.bz2
tar xf dropbear-2026.94.tar.bz2
cd dropbear-2026.94
```

---

## 2. Cross-Compile Dropbear

### Option A: Using the Generic Toolchain

```bash
./configure \
    --host=arm-linux-gnueabihf \
    --disable-zlib \
    --enable-static
```

### Option B: Using the Kobo Clara Colour Exact Toolchain

> [!IMPORTANT]
> Building Dropbear statically (`--enable-static`) avoids dependency problems caused by differences between the build environment and the libraries available in the Kobo firmware.


```bash
./configure \
    --host=arm-unknown-linux-gnueabihf \
    --build=x86_64-linux-gnu \
    --disable-zlib \
    --enable-static
```

### Compile Dropbear (Both Options)

> [!IMPORTANT]
> **Include `scp` in the compilation.**
>
> Dropbear supports SCP, but OpenSSH clients on modern systems default to SFTP. Dropbear only supports the legacy SCP protocol. To use SCP with Dropbear, the client must use the `-O` flag to force legacy SCP mode.
>
> To include SCP in the compilation, add `scp` to the `PROGRAMS` list and enable `SCPPROGRESS` for a progress bar.



```bash
make PROGRAMS='dropbear dropbearkey scp' MULTI=1 SCPPROGRESS=1
```

This produces the multi-call binary `./dropbearmulti`


> [!IMPORTANT]
> Building Dropbear statically avoids dependency problems caused by differences between the build environment and the libraries available in the Kobo firmware.

---

## 3. Create the Kobo Directory Structure

- Create the directory structure that will eventually be packaged into `KoboRoot.tgz`:

```bash
mkdir -p KoboRoot/opt/dropbear
```

- Copy the compiled binary with the `inetd.conf` and the `afterinit.sh` script:

```bash
cp dropbearmulti KoboRoot/opt/dropbear/
```

---

## 4. Update `inetd.conf`

Edit:

```text
/opt/inetd.conf
```

Add an SSH entry for **TCP port 22**, including the key types and exact filenames will be in use:

For example:

```conf
# Telnet
23 stream tcp nowait root /bin/busybox telnetd -i
# SSH
22 stream tcp nowait root /opt/dropbear/dropbearmulti dropbear -i -r /opt/dropbear/rsa_key -r /opt/dropbear/ecdsa_key -r /opt/dropbear/ed25519_key
```

The `-i` option makes Dropbear operate through `inetd`.

> [!IMPORTANT]
> Keep the Telnet entry until SSH has been tested successfully. Once SSH works, Telnet can be removed.

---

## 5. Prepare the Final `KoboRoot.tgz`

The final package should contain:

```text
/KoboRoot/opt $ tree
├── afterinit.sh
├── dropbear
│   └── dropbearmulti
└── inetd.conf

2 directories, 3 files
```

> [!IMPORTANT]
> ## The installation also requires `rcS` ?
> 
> If you previously modified the `/etc/init.d/rcS` file **only if it was required by your previous Telnet installation process guide mentioned [here](https://github.com/alexandrglm/kobo-clara-colour-mods-telnet-ssh-debian-chroot/tree/main/00-02-Getting-TELNET/Step-2_inittab_from_rcS_edited)**, now you can replace the modified file with the original by including it in your KoboRoot temp folder at `./etc/init.d/rcS`.


### 5.1    Create the KoboRoot.tgz
```bash
tar czf ../KoboRoot.tgz ./opt/
```
---

## 6. Apply the Patch

Copy the resulting `KoboRoot.tgz` to the Kobo's internal storage at `.kobo/`.  

Safely eject the device and allow it to reboot.  

During boot, `inetd` will start Dropbear and listen on **TCP port 22**.  

---

## 7. Generate SSH Host Keys via TELNET (or via KOreader's terminal):

- Connect to the Kobo using Telnet.
- 
- To ensure SSH, SCP, and key generation work, **create the necessary** symlinks:

```bash
cd /opt/dropbear
ln -sf dropbearmulti dropbear
ln -sf dropbearmulti dropbearkey
ln -sf dropbearmulti scp
ln -sf /opt/dropbear/dropbearmulti /usr/bin/scp
```

-  Now, generate the SSH host keys:

```bash
cd /opt/dropbear

./dropbearmulti dropbearkey -t rsa -f rsa_key
./dropbearmulti dropbearkey -t ecdsa -f ecdsa_key
./dropbearmulti dropbearkey -t ed25519 -f ed25519_key
```

> [!WARNING]
> Keep the private host keys secure. They identify your Kobo's SSH server.

---

## 9. Connect via SSH

Find the Kobo's IP address and connect:

```bash
ssh root@<IP_ADDRESS>
```

If everything is working, you should receive a Dropbear SSH prompt.

---

## 10. Using SCP

Dropbear supports SCP but **does not support SFTP**. Modern OpenSSH clients attempt SFTP by default. To transfer files using Dropbear's SCP, you must force the legacy SCP protocol with the `-O` flag:

```bash
scp -O file root@<KOBOS_IP>:/
```
---

## Remove Telnet (Not recommended)

If you consider once SSH has been successfully tested, Telnet is no longer necessary, remove it from `/opt/inetd.conf` and leave only the SSH service:

```conf
22 stream tcp nowait root /opt/dropbear/dropbearmulti dropbear -i -r /opt/dropbear/rsa_key -r /opt/dropbear/ecdsa_key -r /opt/dropbear/ed25519_key
```

This makes **SSH the primary remote-access method** and removes the insecure Telnet service.

---

