# ccleste

![screenshot](https://raw.githubusercontent.com/lemon-sherbet/ccleste/master/screenshot.png)

This is a C source port of the [original celeste (Celeste classic)](https://www.lexaloffle.com/bbs/?tid=2145) for the PICO-8.

celeste.c + celeste.h is where the game code is, translated from the pico 8 lua code by hand.
These files don't depend on anything other than the c standard library and don't perform any allocations (it uses its own internal global state).

sdl12main.c provides a "frontend" written in SDL1.2 (plus SDL mixer) which implements graphics and audio output. It can be compiled on unix-like platforms by running
```
make
```

3DS is also a supported platform. Compile to 3DS with:
```
make -f Makefile.3ds    # this will generate ccleste.3dsx
```
You will need devkitPro with these dkp packages installed:
```
3ds-sdl
3ds-sdl_mixer
libctru
devkitARM
```

# credits

Sound and music assets in data/ are taken from [https://github.com/JeffRuLz/Celeste-Classic-GBA/tree/master/maxmod_data](https://github.com/JeffRuLz/Celeste-Classic-GBA/tree/master/maxmod_data)

All credit for the original games goes to the original developers (Matt Thorson & Noel Berry).
