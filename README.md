# vkEngine

vkEngine, my long term "game engine" project.
Might end up as an actual game at some point, no idea when.

This started long long ago in 2004, with OpenGL and ATI GPU demos... My only game that has actually used this "engine" was my [Tetris game/graphics demo](https://github.com/seishuku/Tetris).

I also have the [OpenGL version](https://github.com/seishuku/Engine), this is a *far* more evolved codebase than that though, the OpenGL version I should archive the repo as I haven't touched it in quite some time.

To-do (in no particular order):
- Improve audio (~~3D spatializer~~, ~~streaming support~~, synth?)
- Improve physics (more collision object support, angular velocity, ???)
- Streamline post processing effects (it's kind of tacked on right now)
- Networking? (I want to have multiplayer support at some point) **This gets a half check, it does *kind of* work.**
- 3D model animation?
- Hud? (is this considered part of GUI?)
- ~~GUI?~~
- ???
- Profit?


External dependencies needed for building this (versions as of this writing, newer *should* work):
 - libvorbis 1.3.7
 - OpenVR 1.23.7
 - OpenXR 1.2??? (working on porting to this)
 - pthread (whatever Linux has, or pthread-win32 2.0 for windows)
 - portaudio 2.0
 - Vulkan 1.3.231.1
