# vkEngine
![vkEngine screen shot](https://github.com/seishuku/vkEngine/blob/master/screen%20shot.png)

vkEngine, my long term "game engine" project.
Might end up as an actual game at some point, no idea when.

This started long long ago in 2004, with OpenGL and ATI GPU demos... My only game that has actually used this "engine" was my [Tetris game/graphics demo](https://github.com/seishuku/Tetris).

I also have the [OpenGL version](https://github.com/seishuku/Engine), this is a *far* more evolved codebase than that though, the OpenGL version I should archive the repo as I haven't touched it in quite some time.

I love doing everything my self with as little external library dependencies as possible.<br>
This is an on-going learning experience for me and for anyone else that can learn from my code, I try to keep it clean and readable.<br>
> "Building square wheels to better appreciate the round ones."

---

## To-do (in no particular order):
- Improve audio (needs pops/clicks fixed)
- Improve physics (needs more refining, it doesn't pass a box stack very well)
- Streamline post processing effects (it's kind of tacked on right now)
- Improve networking (this is better now, [server here](https://github.com/seishuku/vkEngineServer))
- 3D model animation?
- ???
- Profit?

---

External dependencies for building this (versions as of this writing, newer *should* work):
 - `libvorbis` 1.3.7
 - `libogg` 1.3.5
 - `OpenXR` 1.2
 - `Vulkan` 1.3.231.1

> Note: recursive clone to fetch submodules

---

## Controls:
- `W/A/S/D` Typical strafe movement
- `Q/E` Roll
- `Up/Down arrows` Pitch
- `Left/Right arrows` Yaw
- `Space` Fire projectile
- `Control` Fire "laser"
- `Z` "Explode" asteroid field (used for testing physics)
- `O` Generate new random play area
- `P` Pauses all physics except for camera

> Mouse interacts with UI, but also controls ship pitch/yaw.

---

## Building:
Should be pretty straightforward cmake:<br>
```cmake -S. -B out -GNinja```<br>
or use a preset:<br>
```cmake -S. --preset "Linux x64 Release"```<br>
```cmake -S. --preset "Windows x64 Release"```<br>
```cmake -S. --preset "Android Release"```

> <b>Note:</b> Android building has only been tested on Windows. It should work on Linux, I just don't have the Android SDK installed there.

---

## Network:
To connect to a server, just bring up the console in game with '~'/'`' and type:<br>
```connect XXX.XXX.XXX.XXX```<br>
Where XXX.XXX.XXX.XXX is the remote IP.
> <b>Note:</b> Networking is very much a WIP.
