# Renderer

A real-time renderer made in c++ and Vulkan as a learning project.


## Status

Starting the project: window and project setup done, starting with Vulkan

## Features

## Building

### Requirements

- Windows 10/11
- Vulkan SDK 1.3 or newer
- Build Tools for Visual Studio (Desktop development with C++)
- vcpkg, with the `VCPKG_ROOT` environment variable set
- A GPU with Vulkan 1.3 support

### Steps

1. Clone the repo
2. Open it in VS Code, get the @recommended extensions
3. Select the preset (Ctrl+shift+p), it should be CMake:Select Configure preset and pick Windows MSVC (Ninja)
4. Then build (f7) and the build will appear in \build\windows-msvc\ (the first build might take a few minutes)
5. Run it in vscode (CMake:Run Without Debugging (shift + f5)) or by running the build\windows-msvc\renderer.exe

## Roadmap

- [x] Project setup and window
- [ ] Vulkan initialization and clearing the screen
- [ ] Triangle
- [ ] Camera, model loading, textures
- [ ] Lighting and PBR
- [ ] Shadows
- [ ] Post-processing