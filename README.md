# UltimateMangaReader
An online manga reader for Kobo E-Ink devices.


# Windows

To build the UltimateMangaReader for Windows you will need Qt 4.8.5. 
OpenSSL 1.0.2 (prebuild or self build as well as includes) are nessasarry and you probbably have to
edit the paths project file.

# Kobo

To build for Kobo you have to setup an arm cross-compiling environment with Qt 4.8.5 first, instructions can be found at https://www.mobileread.com/forums/showthread.php?t=297335.
You also need the KoboLauncher sources from https://bitbucket.org/vlasovsoft/free and place them into the same directory as the UltimateMangaReader sources.
