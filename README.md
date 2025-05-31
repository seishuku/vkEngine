# vkEngine
![vkEngine screen shot](https://github.com/seishuku/vkEngine/blob/master/screen%20shot.png)

vkEngine, my long term "game engine" project.
Might end up as an actual game at some point, no idea when.

This started long long ago in 2004, with OpenGL and ATI GPU demos... My only game that has actually used this "engine" was my [Tetris game/graphics demo](https://github.com/seishuku/Tetris).

I also have the [OpenGL version](https://github.com/seishuku/Engine), this is a *far* more evolved codebase than that though, the OpenGL version I should archive the repo as I haven't touched it in quite some time.

I love doing everything my self with as little external library dependencies as possible.<br>
This is also an on-going learning experience for me and for anyone else that can learn from my code, I try to keep it clean and readable.<br>
Very much "Building square wheels to better appreciate the round ones."

To-do (in no particular order):
- Improve audio (needs pops/clicks fixed)
- Improve physics (more collision object support?)
- Streamline post processing effects (it's kind of tacked on right now)
- Improve networking (this *kind of* works, but physics system is a problem, [server here](https://github.com/seishuku/vkEngineServer))
- 3D model animation?
- ???
- Profit?


External dependencies for building this (versions as of this writing, newer *should* work):
 - libvorbis 1.3.7
 - libogg 1.3.5
 - OpenXR 1.2
 - Vulkan 1.3.231.1

(note: recursive clone to fetch dependency submodules)
