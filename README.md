# UltimateMangaReader
A feature-rich online manga streamer and reader for Kobo E-Ink devices, based on Qt5.

## Main features
* It's **cross-platform**. While the primary targets are Kobo E-Ink devices, it will also run on Windows, Linux and Mac. Qt5.15 (with OpenSSL) and a platform backend being the only hard dependency.

* It comes with a refined, modern design and a GUI that is DPI independent and will look great on various platforms.

* It supports **multiple websites** as manga sources with tens of thousands of available mangas.

* It's **fast**. It will stream, prerender and cache mangapages in advance while reading to minimize loading times and to enable the perfect reading experience.

* It supports **offline-reading**. Mangas can be downloaded in advance and read later without internet connection.

* It supports **favorites** and bookmarks **reading progress** automatically, so you can pick right up reading where you left off.

* It is designed to be ergonomic on E-Readers. It supports configurable gesture inputs, frontlight control and a sleep mode. Custom screensavers can be placed in the screensaver folder and will be picked at random.


## Install on Kobo devices
First you will need to install a launcher application. I recomend [KFMon](https://github.com/NiLuJe/kfmon), the latest release along with install instructions can be found [here](https://www.mobileread.com/forums/showthread.php?t=274231).

For the application itself just download the latest release [here](https://github.com/Rain92/UltimateMangaReader/releases) and extract the archive into the root directiory of your Kobo device.

## Build 
Building the application requires Qt 5.15+ with OpenSSL 1.1.1+. \
With Qt creator the build process is straight-foreward. \
Building qor kobo targets the requires a cross compiled Qt and a Kobo platform plugin. \
See https://github.com/Rain92/qt5-kobo-platform-plugin. \
Add CONFIG+=kobo to the qmake arguments and put the plugin source in the same parent folder as the app.


## Cross-compile for Kobo
### Setting up the cross-compile toolchain
The koxtoolchain is recomendet:  https://github.com/koreader/koxtoolchain \
Install the dependencies and run:
```shell
./gen-tc.sh kobo
```

### Cross-compile OpenSSH for Qt
Download the latest OpenSSL (1.1.1+). \
Configure, make and install, for example like this:

```shell
export CROSS=/home/${USER}/x-tools/arm-kobo-linux-gnueabihf/bin/arm-kobo-linux-gnueabihf
export SYSROOT=/home/${USER}/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot
export AR=${CROSS}-ar
export AS=${CROSS}-as
export CC=${CROSS}-gcc
export CXX=${CROSS}-g++
export LD=${CROSS}-ld
export RANLIB=${CROSS}-ranlib
export CFLAGS="-O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard -D__arm__ -D__ARM_NEON__ -fPIC -fno-omit-frame-pointer -funwind-tables -Wl,--no-merge-exidx-entries"
./Configure linux-elf no-comp no-asm shared --prefix=${SYSROOT}/usr --openssldir=${SYSROOT}/usr
make -j5
make install
```

### Prepare Qt
Download the latest Qt (5.15+). \
A descriptor for the kobo platform has to be added. \
In the source folder of Qt go to qtbase\mkspecs and add a new folder named linux-kobo-gnueabihf-g++ with theese two files: \
qmake.conf 
```
#
# Kobo qmake configuration
#

MAKEFILE_GENERATOR      = UNIX
CONFIG                 += incremental gdb_dwarf_index
QMAKE_INCREMENTAL_STYLE = sublib

include(../common/linux.conf)
include(../common/gcc-base-unix.conf)
include(../common/g++-unix.conf)


QMAKE_CFLAGS_RELEASE   = -O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard -D__arm__ -D__ARM_NEON__ -fPIC -fno-omit-frame-pointer -funwind-tables -Wl,--no-merge-exidx-entries
QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO -g

QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CFLAGS_RELEASE
QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO

# modifications to g++.conf
QMAKE_CC                = arm-kobo-linux-gnueabihf-gcc
QMAKE_CXX               = arm-kobo-linux-gnueabihf-g++
QMAKE_LINK              = arm-kobo-linux-gnueabihf-g++
QMAKE_LINK_SHLIB        = arm-kobo-linux-gnueabihf-g++

# modifications to linux.conf
QMAKE_AR                = arm-kobo-linux-gnueabihf-ar cqs
QMAKE_OBJCOPY           = arm-kobo-linux-gnueabihf-objcopy
QMAKE_NM                = arm-kobo-linux-gnueabihf-nm -P
QMAKE_STRIP             = arm-kobo-linux-gnueabihf-strip

load(qt_config)
```

qplatformdefs.h
```
#include "../linux-g++/qplatformdefs.h"
```


~As of Qt 5.15.0 there is an open issue that will lead to random artifacts in the gui when using high-dpi scaling with fractional scaling factors. This can be resolved with the following patch~ (Already fixed in Qt 5.15.1):
```
diff --git a/src/gui/kernel/qhighdpiscaling_p.h b/src/gui/kernel/qhighdpiscaling_p.h
index 55ad6df5c9da3d11a8900754eebc27528aec90ec..9c3d0cdba28a1dd51c18d3fcc0350a11f8000ebc 100644
--- a/src/gui/kernel/qhighdpiscaling_p.h
+++ b/src/gui/kernel/qhighdpiscaling_p.h
@@ -204,7 +204,7 @@ inline QRegion scale(const QRegion &region, qreal scaleFactor, QPoint origin = Q
 
     QRegion scaled;
     for (const QRect &rect : region)
-        scaled += scale(rect, scaleFactor, origin);
+        scaled += scale(QRectF(rect), scaleFactor, origin).toRect();
     return scaled;
 }
 ```


### Cross-compile Qt
Open a new terminal. Make sure arm-kobo-linux-gnueabihf-gcc is in your path. If in doubt add it with:

```shell
export PATH=$PATH:/home/${USER}/x-tools/arm-kobo-linux-gnueabihf/bin/
```


Configure, make and install Qt:
```shell
export QTDIR=qt-linux-5.15.1-kobo
export SYSROOT=/home/${USER}/x-tools/arm-kobo-linux-gnueabihf/arm-kobo-linux-gnueabihf/sysroot
./configure --recheck-all -opensource -confirm-license -release -verbose \
 -prefix /mnt/onboard/.adds/${QTDIR} \
 -extprefix /home/${USER}/qt-bin/${QTDIR} \
 -xplatform linux-kobo-gnueabihf-g++ \
 -sysroot ${SYSROOT} \
 -openssl-linked OPENSSL_PREFIX="${SYSROOT}/usr" \
 -qt-libjpeg -qt-zlib -qt-libpng -qt-freetype -qt-harfbuzz -qt-pcre -sql-sqlite -linuxfb \
 -no-sse2 -no-xcb -no-xcb-xlib -no-tslib -no-icu -no-iconv -no-dbus \
 -nomake tests -nomake examples -no-compile-examples -no-opengl \
 -skip qtx11extras -skip qtwayland -skip qtwinextras -skip qtmacextras -skip qtandroidextras \
 -skip qttools -skip qtdoc -skip qtlocation -skip qtremoteobjects -skip qtconnectivity -skip qtgamepad \
 -skip qt3d -skip qtquick3d -skip qtquickcontrols -skip qtsensors -skip qtspeech -skip qtdatavis3d \
 -skip qtpurchasing -skip qtserialbus -skip qtserialport -skip multimedia -skip qtquicktimeline -skip qtlottie \
 -skip activeqt -skip qtscript -skip qtxmlpatterns -skip qtscxml \
 -skip qtwebengine -skip qtwebview -skip qtwebglplugin \
 -no-cups -no-pch -no-libproxy \
 -no-sql-db2 -no-sql-ibase -no-sql-mysql -no-sql-oci -no-sql-odbc -no-sql-psql -no-sql-sqlite2 -no-sql-tds \
 -no-feature-printdialog -no-feature-printer -no-feature-printpreviewdialog -no-feature-printpreviewwidget

make -j5
make install
```
