## Baseline Details

| Detail              | Value      |
|---------------------|------------|
| Current Branch      | Main       |
| Commit Hash         | e47f6ds6   |
| Working Tree Status | up to date |

## Local only Prerequisites

| Category |	Current project example	| Why it matters |
| ---- | ---- | ---- |
| Compiler/toolchain | A C++ compiler supporting C++23	| The Makefile uses -std=c++23. |
| Build tool	| GNU Make |	The current build path is Make-based. |
| SFML	| SFML 3.x development headers and libraries |	The project includes SFML graphics, window, system, and audio modules. |
| GLEW	| GLEW headers and library	| The project calls glewInit() and uses GLEW OpenGL entry points. |
| OpenGL  development support |	Linux OpenGL libraries or macOS OpenGL framework |	The Makefile links -lGL on Linux and -framework OpenGL on macOS. |
| OpenMP |	OpenMP compiler/runtime support	| The build uses -fopenmp; macOS additionally references Homebrew libomp. |
Graphics hardware/driver |	A GPU and driver exposing the required OpenGL features |	The runtime reports the active OpenGL and GLSL versions. |
Display environment	| A working graphical desktop/display session |	The game creates a fullscreen window and cannot be meaningfully baseline-tested headlessly. |
Audio device |	An available audio backend/device |	The game loads music and sound effects through SFML audio. |
Working directory |	Running from the repository’s bin/ directory, or otherwise preserving asset paths |	The executable is launched from bin, while assets are referenced using relative paths. |

## Current Known Limitations

The program will compile under macOS and allow you to launch to the main menu, however the extensive use of OpenGL version 460 features causes it to crash immediately upon entering Scene_IC_Camp.  This is precisely why we are transitioning to Vulkan and ultimately MoltenVK for Mac.