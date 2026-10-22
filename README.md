# Renderer

A real-time renderer made in C++ and Vulkan as a learning project.


## Status

Vulkan initialization done: the renderer creates a swapchain and clears it every frame, with window resizing and minimizing handled. Next: drawing a triangle.

## Features

- Vulkan 1.3 setup with validation layers (via vk-bootstrap)
- Swapchain with frames in flight and proper CPU/GPU synchronization
- Swapchain recreation on window resize, minimize handling

## Building

### Requirements

- Windows 10/11
- Vulkan SDK 1.3 or newer
- Build Tools for Visual Studio (Desktop development with C++)
- vcpkg, with the `VCPKG_ROOT` environment variable set
- A GPU with Vulkan 1.3 support

### Steps

1. Clone the repository: git clone https://github.com/Stefan015/renderer.git
2. Open the folder in VS Code and install the recommended extensions (search `@recommended` in the Extensions panel).
3. Run **CMake: Select Configure Preset** from the command palette (`Ctrl+Shift+P`) and choose **Windows MSVC (Ninja, Debug)**. The first configure takes a few minutes, because vcpkg downloads and builds the dependencies.
4. Build with **CMake: Build** (`F7`). The executable is written to `build/windows-msvc/`.
5. Run it with **CMake: Run Without Debugging** (`Shift+F5`), or start `build/windows-msvc/renderer.exe` directly.

## Roadmap

- [x] Project setup and window
- [x] Vulkan initialization and clearing the screen
- [ ] Triangle
- [ ] Camera, model loading, textures
- [ ] Lighting and PBR
- [ ] Shadows
- [ ] Post-processing