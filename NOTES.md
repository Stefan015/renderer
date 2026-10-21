# Dev notes

## Current status

- Milestone: Vulkan initialization and clearing the screen (in progress)
- Working: full Vulkan setup (instance, surface, GPU, device, swapchain), per-frame command buffers and sync objects, frame loop clears the swapchain image to a pulsing blue; no validation errors
- Known issues:
  - Resizing the window freezes the image (acquire returns `VK_ERROR_OUT_OF_DATE_KHR` and the frame is skipped; swapchain is not recreated yet)
  - Minimizing the window is not handled (framebuffer size becomes 0)
- Next: recreate the swapchain on resize, handle minimized window

## Open questions

- How to choose precise stage and access masks for barriers instead of `ALL_COMMANDS`

## Log

### 2026-10-22 — Swapchain and frame loop

- Created the swapchain with vk-bootstrap: `B8G8R8A8_UNORM` + sRGB nonlinear color space, FIFO present mode (vsync, always supported), extent from `glfwGetFramebufferSize`, extra usage `TRANSFER_DST` so images can be cleared. Got 3 images and their image views
- `createSwapchain()` / `destroySwapchain()` are separate functions so they can be called again on resize. Swapchain images are owned by the swapchain; image views are ours to destroy
- Added `FrameData` (command pool, command buffer, fence, image available semaphore), 2 frames in flight, used alternately via `frameNumber % FRAMES_IN_FLIGHT`
- Render finished semaphores are one per swapchain image, not per frame, because presentation can hold a semaphore until that image is reused; recent validation layers report an error otherwise
- Fences are created signaled, so the first wait doesn't block forever
- Frame loop: wait on fence → acquire image → reset fence → record (transition `UNDEFINED` → `TRANSFER_DST_OPTIMAL`, clear, transition → `PRESENT_SRC_KHR`) → submit (wait image available, signal render finished + fence) → present
- The fence is reset only after a successful acquire: resetting it and then skipping the frame would leave it unsignaled and the next wait on that slot would hang
- Barriers use `ALL_COMMANDS` stage masks for now: simple and correct, to be made precise once there's real rendering
- Mental model that made it click: images live in VRAM and only their handles are in my program; per frame the CPU sends commands, not images. Fence = CPU waits for GPU, semaphores = GPU and presentation engine wait for each other, barriers = ordering + layout changes inside one command buffer

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