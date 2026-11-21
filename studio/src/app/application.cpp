// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/application.hpp"

#include "app/options.hpp"
#include "ui/flags.hpp"
#include "ui/fonts.hpp"
#include "ui/theme.hpp"
#include "views/workspace.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <utility>

namespace irama_studio {
namespace {

constexpr const char* title = "Irama Studio";
// The window's size when it opens, in logical pixels.
constexpr float window_width = 1280.0F;
constexpr float window_height = 800.0F;
// What the frame is cleared to, beneath the windows, which cover all of it.
constexpr SDL_FColor clear_color{.r = 0.06F, .g = 0.06F, .b = 0.06F, .a = 1.0F};

// Runs a function as it goes out of scope, so that each step of start-up is undone, in reverse,
// however run() returns.
template <std::invocable Function> class [[nodiscard]] Deferred {
  public:
    explicit Deferred(Function function) : function_(std::move(function)) {}

    Deferred(const Deferred&) = delete;
    Deferred(Deferred&&) = delete;
    Deferred& operator=(const Deferred&) = delete;
    Deferred& operator=(Deferred&&) = delete;

    ~Deferred() { function_(); }

  private:
    Function function_;
};

// Reports why Studio can't start, with SDL's error, both on stderr and in a message box, since a
// user who started Studio from a desktop sees no stderr. Returns the exit status to end with.
[[nodiscard]] int fail(std::string_view what) {
    const std::string message = std::format("{}: {}", what, SDL_GetError());
    std::println(stderr, "irama-studio: {}", message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), nullptr);
    return exit_failure;
}

// The scale the user chose for the display, by which the window, the font and the style grow.
// ImGui works in the window's own coordinates and its backend scales for the pixel density itself,
// so of the window's display scale, only what the pixel density leaves is applied.
[[nodiscard]] float content_scale(SDL_Window* window) {
    const float display_scale = SDL_GetWindowDisplayScale(window);
    const float density = SDL_GetWindowPixelDensity(window);
    if (display_scale <= 0.0F || density <= 0.0F) {
        return 1.0F;
    }
    return display_scale / density;
}

// Draws ImGui's frame into the window's next swapchain texture, which waits for vsync. A window
// with no area, such as one minimised, has nothing drawn.
void render(SDL_GPUDevice* device, SDL_Window* window) {
    ImDrawData* const draw_data = ImGui::GetDrawData();
    SDL_GPUCommandBuffer* const commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == nullptr) {
        return;
    }
    SDL_GPUTexture* swapchain = nullptr;
    const bool acquired =
        SDL_WaitAndAcquireGPUSwapchainTexture(commands, window, &swapchain, nullptr, nullptr);
    if (acquired && swapchain != nullptr && draw_data->DisplaySize.x > 0.0F &&
        draw_data->DisplaySize.y > 0.0F) {
        // The backend uploads its vertices outside a render pass.
        ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, commands);
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain;
        target.clear_color = clear_color;
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, commands, pass);
        SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(commands);
}

} // namespace

int run(const Options& options) {
    SDL_SetAppMetadata(title, IRAMA_VERSION_STRING, nullptr);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return fail("Couldn't start SDL's video");
    }
    const Deferred quit_sdl{[] { SDL_Quit(); }};

    // Window sizes are in the display's coordinates, which already include the pixel density where
    // there is one, so only the content scale makes the size logical.
    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (scale <= 0.0F) {
        scale = 1.0F;
    }
    const SDL_WindowFlags window_flags =
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    SDL_Window* const window =
        SDL_CreateWindow(title, static_cast<int>(std::lround(window_width * scale)),
                         static_cast<int>(std::lround(window_height * scale)), window_flags);
    if (window == nullptr) {
        return fail("Couldn't open the window");
    }
    const Deferred destroy_window{[window] { SDL_DestroyWindow(window); }};

    SDL_GPUDevice* const device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
        false, nullptr);
    if (device == nullptr) {
        return fail("Couldn't create the GPU device");
    }
    const Deferred destroy_device{[device] { SDL_DestroyGPUDevice(device); }};
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        return fail("Couldn't draw in the window with the GPU device");
    }
    const Deferred release_window{[device, window] {
        SDL_WaitForGPUIdle(device);
        SDL_ReleaseWindowFromGPUDevice(device, window);
    }};
    if (!SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       SDL_GPU_PRESENTMODE_VSYNC)) {
        return fail("Couldn't present with vsync");
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    const Deferred destroy_context{[] { ImGui::DestroyContext(); }};
    ImGuiIO& io = ImGui::GetIO();
    // Nothing is written to the working directory.
    io.IniFilename = nullptr;
    // Windows dock inside the main window only. Windows of their own work only under some video
    // drivers, and never on Wayland.
    io.ConfigFlags = combine_flags(io.ConfigFlags, ImGuiConfigFlags_DockingEnable);
    apply_theme(content_scale(window));
    load_fonts();

    if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
        return fail("Couldn't start ImGui's SDL backend");
    }
    const Deferred shut_down_platform{[] { ImGui_ImplSDL3_Shutdown(); }};
    ImGui_ImplSDLGPU3_InitInfo renderer{};
    renderer.Device = device;
    renderer.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    renderer.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&renderer)) {
        return fail("Couldn't start ImGui's SDL_GPU backend");
    }
    const Deferred shut_down_renderer{[device] {
        SDL_WaitForGPUIdle(device);
        ImGui_ImplSDLGPU3_Shutdown();
    }};

    Workspace workspace;
    std::uint64_t frames = 0;
    bool running = true;
    while (running) {
        // A window that can't be seen draws nothing until something happens to it.
        const SDL_WindowFlags hidden = SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN;
        if ((SDL_GetWindowFlags(window) & hidden) != 0) {
            SDL_WaitEvent(nullptr);
        }
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            const bool closed = event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                event.window.windowID == SDL_GetWindowID(window);
            if (event.type == SDL_EVENT_QUIT || closed) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                apply_theme(content_scale(window));
            }
        }

        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (!workspace.draw()) {
            running = false;
        }
        ImGui::Render();
        render(device, window);

        ++frames;
        if (options.exit_after_frames && frames >= *options.exit_after_frames) {
            running = false;
        }
    }
    return 0;
}

} // namespace irama_studio
