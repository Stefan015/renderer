# Dev notes

## Current status

- Milestone: Vulkan initialization and clearing the screen (in progress)
- Working:  window opens; Vulkan instance with validation layers, surface, GPU selection and logical device + graphics queue are created; everything cleans up without validation errors, also when setup fails halfway
- Known issues: none
- Next: swapchain, then clearing the screen to a color

## Log

### 2026-10-21 — Surface, device, Renderer class

- Created the window surface with `glfwCreateWindowSurface`, selected the GPU with vk-bootstrap (requires Vulkan 1.3, dynamic rendering, synchronization2, prefers discrete GPU), created the logical device and got the graphics queue + family index
- Moved all setup from `main.cpp` into a `Renderer` class (`renderer.h` / `renderer.cpp`). Setup is in `init()` which returns false on failure; cleanup is in the destructor, which destroys only what was created (handles start as `VK_NULL_HANDLE`)
- Decision: vk-bootstrap objects are only local variables during setup; the class stores the raw Vulkan handles. Handles are stored right after each successful step so the destructor can clean up after a later failure
- Copying `Renderer` is disabled, because two copies would destroy the same Vulkan objects twice
- Tested the failure path by requiring Vulkan 1.5: instance and surface were created, GPU selection failed, no validation errors on exit
- The debug messenger has to be destroyed with `vkb::destroy_debug_utils_messenger`, because extension functions aren't exported by the loader and must be loaded at runtime

### 2026-10-20 — Vulkan instance

- Added vk-bootstrap through vcpkg
- Created the instance with validation layers and the default debug messenger, enabling the surface extensions GLFW requires
- Tested validation by destroying the instance without the messenger: the validation layer reported the leaked `VkDebugUtilsMessengerEXT`

### 2026-10-19 — Project setup

- Installed Build Tools for Visual Studio, Vulkan SDK, Git, vcpkg; VS Code with CMake Tools, clangd and C/C++
- Set up CMake + vcpkg manifest mode + Ninja + MSVC with a preset, pinned library versions with a vcpkg baseline
- GLFW window opens and prints the required Vulkan instance extensions
- Problem: clangd underlined all includes in red. The clangd log only showed `argv[0]`, so the settings weren't applied. Cause: `.vscode` was inside `src/` instead of the project root. Also added a `.clangd` file pointing to `compile_commands.json`