export BASE="/home/dev/Desktop/ClaraColour/python/crosstoolsng"
export PATH="${HOME}/x-tools/arm-unknown-linux-gnueabihf/bin:${PATH}"
export CC=arm-unknown-linux-gnueabihf-gcc
export STRIP=arm-unknown-linux-gnueabihf-strip

cd ~/Desktop/ClaraColour/z_repos/kobo-clara-colour-mods-telnet-ssh-debian-chroot/01_my-modules-my-tools/01_03_drmc

# Recompilar
${CC} -static -O2 -pipe -Wall -Wextra \
    -I${BASE}/zlib-armhf-install/include \
    -I${BASE}/openssl-armhf-install/include \
    -I${BASE}/libzip-armhf-install/include \
    -I${BASE}/libxml2-armhf-install/include/libxml2 \
    -c drm.c -o drm.o

${CC} -static \
    -L${BASE}/zlib-armhf-install/lib \
    -L${BASE}/openssl-armhf-install/lib \
    -L${BASE}/libzip-armhf-install/lib \
    -L${BASE}/libxml2-armhf-install/lib \
    -o drm drm.o -lxml2 -lzip -lssl -lcrypto -lz -lm -lpthread -ldl

${STRIP} drm

file drm
ls -lh drm
