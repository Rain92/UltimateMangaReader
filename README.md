# UltimateMangaReader
A feature-rich online manga streamer and reader for Kobo E-Ink devices, based on Qt 6.

## Main features
* It's **cross-platform**. While the primary targets are Kobo E-Ink devices, it will also run on Windows, Linux and Mac. Qt 6 (with OpenSSL), libjpeg-turbo, libpng, libwebp and a platform backend are the only dependencies.

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
Requirements:
* Qt 6.6 or newer (developed with Qt 6.12) with the modules widgets, network, svg and concurrent, and OpenSSL
* libjpeg-turbo 3.0+ (the TurboJPEG 3 API), libpng and libwebp

On the desktop the libraries are found with pkg-config:

```shell
mkdir build && cd build
qmake ../UltimateMangaReader.pro
make -j$(nproc)
```

Qt Creator works as well, just open `UltimateMangaReader.pro`.

## Cross-compile for Kobo
Building for Kobo devices requires:
* the Kobo cross compiler toolchain (GCC 14, glibc 2.15) from [koxtoolchain](https://github.com/koreader/koxtoolchain)
* a cross compiled Qt 6 and the libraries above, installed into the toolchain's sysroot
* the source of the [Kobo platform plugin](https://github.com/Rain92/qt5-kobo-platform-plugin), which also has to be built and deployed with Qt

[kobo-qt-setup-scripts](https://github.com/Rain92/kobo-qt-setup-scripts) sets all of this up (toolchain, libraries, Qt 6 and deploying it to the device).

Then build with the cross compiled Qt's qmake and `CONFIG+=kobo`.
The platform plugin's source is expected next to this repository (`../qt5-kobo-platform-plugin`), another location can be set with `KOBO_PLATFORM_PLUGIN_PATH`:

```shell
mkdir build-kobo && cd build-kobo
~/kobo/qt-bin/qt-6.12.0-kobo/bin/qmake ../UltimateMangaReader.pro CONFIG+=kobo CONFIG+=release \
    KOBO_PLATFORM_PLUGIN_PATH=/path/to/qt5-kobo-platform-plugin
make -j$(nproc)
```
