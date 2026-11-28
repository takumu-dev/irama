// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/application.hpp"

#include "app/config_files.hpp"
#include "app/options.hpp"
#include "app/settings.hpp"
#include "app/shortcut_actions.hpp"
#include "app/song_editor.hpp"
#include "audio/audio_engine.hpp"
#include "audio/meter_display.hpp"
#include "input/key_router.hpp"
#include "input/note_keys.hpp"
#include "input/shortcut_table.hpp"
#include "ui/flags.hpp"
#include "ui/fonts.hpp"
#include "ui/theme.hpp"
#include "views/audio_window.hpp"
#include "views/status_bar.hpp"
#include "views/workspace.hpp"
#include <irama/engine/base/narrow.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>

namespace irama_studio {
namespace {

constexpr const char* title = "Irama Studio";
constexpr const char* settings_file_name = "studio.ini";
constexpr const char* layout_file_name = "imgui.ini";
// What the frame is cleared to, beneath the windows, which cover all of it.
constexpr SDL_FColor clear_color{.r = 0.06F, .g = 0.06F, .b = 0.06F, .a = 1.0F};
// How long the loop waits for an event while the window can't be seen, so that it still looks at
// the audio device's heartbeat.
constexpr std::int32_t hidden_wait_ms = 250;

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

// Reports why Studio can't start: the step that failed, then SDL's error. It goes to stderr, and
// also to a message box, since a user who started Studio from a desktop sees no stderr, but not
// under a test, where nobody would close the box. Returns the exit status to end with.
[[nodiscard]] int fail(const Options& options, std::string_view step) {
    const std::string_view why = SDL_GetError();
    const std::string message =
        std::format("{}: {}", step, why.empty() ? "SDL gave no reason" : why);
    std::println(stderr, "irama-studio: {}", message);
    if (!options.exit_after_frames) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), nullptr);
    }
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

// The scale that turns a window size in logical pixels into the display's coordinates, which
// already include the pixel density where there is one.
[[nodiscard]] float display_content_scale(SDL_DisplayID display) {
    const float scale = SDL_GetDisplayContentScale(display);
    return scale > 0.0F ? scale : 1.0F;
}

[[nodiscard]] int to_display(int logical, float scale) {
    return irama::narrow<int>(std::lround(static_cast<float>(logical) * scale));
}

[[nodiscard]] int to_logical(int size, float scale, int min, int max) {
    return std::clamp(irama::narrow<int>(std::lround(static_cast<float>(size) / scale)), min, max);
}

// Copies the window's size and whether it is maximised into the settings. The size kept is the one
// the window has when neither maximised nor minimised, which it returns to when restored.
void remember_window(SDL_Window* window, Settings& settings) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    if ((flags & SDL_WINDOW_MINIMIZED) != 0) {
        return;
    }
    settings.window_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    int width = 0;
    int height = 0;
    if (settings.window_maximized || !SDL_GetWindowSize(window, &width, &height)) {
        return;
    }
    const float scale = display_content_scale(SDL_GetDisplayForWindow(window));
    settings.window_width = to_logical(width, scale, min_window_width, max_window_size);
    settings.window_height = to_logical(height, scale, min_window_height, max_window_size);
}

// Reports a settings file that couldn't be written. Studio carries on without it, since the file
// holds only what Studio would remember next time.
void report(const std::expected<void, std::string>& written) {
    if (!written) {
        std::println(stderr, "irama-studio: {}", written.error());
    }
}

// The settings and the layout as Studio read them at start-up, and the files it keeps them in, if
// it has a settings directory. Without one, Studio starts from the defaults and keeps nothing.
class SettingsFiles {
  public:
    explicit SettingsFiles(const std::optional<std::filesystem::path>& directory) {
        if (!directory) {
            return;
        }
        const std::filesystem::path settings_path = *directory / settings_file_name;
        const std::filesystem::path layout_path = *directory / layout_file_name;
        std::string settings_text = read_text_file(settings_path).value_or(std::string{});
        settings_ = parse_settings(settings_text);
        layout_ = read_text_file(layout_path).value_or(std::string{});
        settings_file_.emplace(settings_path, std::move(settings_text));
        layout_file_.emplace(layout_path, layout_);
    }

    [[nodiscard]] const Settings& settings() const { return settings_; }

    [[nodiscard]] const std::string& layout() const { return layout_; }

    void change_settings(const Settings& settings, DelayedFile::Clock::time_point now) {
        if (settings_file_) {
            settings_file_->change(format_settings(settings), now);
        }
    }

    void change_layout(std::string layout, DelayedFile::Clock::time_point now) {
        if (layout_file_) {
            layout_file_->change(std::move(layout), now);
        }
    }

    void write_if_due(DelayedFile::Clock::time_point now) {
        if (settings_file_ && layout_file_) {
            report(settings_file_->write_if_due(now));
            report(layout_file_->write_if_due(now));
        }
    }

    void flush() {
        if (settings_file_ && layout_file_) {
            report(settings_file_->flush());
            report(layout_file_->flush());
        }
    }

  private:
    Settings settings_;
    std::string layout_;
    std::optional<DelayedFile> settings_file_;
    std::optional<DelayedFile> layout_file_;
};

// The key event as the key router reads it. SDL's scancodes are USB HID usage IDs, which the note
// keys and named keys are. The character comes from the key's keycode in the current layout with
// no modifier held, where a keycode for a key that types a character is that character, and SDL
// gives a layout with no Latin letters the US layout's.
[[nodiscard]] KeyEvent key_event(const SDL_KeyboardEvent& key) {
    const SDL_Keycode keycode = SDL_GetKeyFromScancode(key.scancode, SDL_KMOD_NONE, true);
    return {
        .key = static_cast<int>(key.scancode),
        .character = (keycode & SDLK_SCANCODE_MASK) == 0 ? char32_t{keycode} : 0,
        .modifiers =
            {
                .ctrl = (key.mod & SDL_KMOD_CTRL) != 0,
                .alt = (key.mod & SDL_KMOD_ALT) != 0,
                .shift = (key.mod & SDL_KMOD_SHIFT) != 0,
            },
        .super = (key.mod & SDL_KMOD_GUI) != 0,
        .down = key.down,
        .repeat = key.repeat,
    };
}

// Routes a key going down or up: runs its shortcut, or sends its note message. A text field takes
// the key while ImGui takes typed text, so Ctrl+Z there undoes its typing, not an edit.
void handle_key(const SDL_KeyboardEvent& key, KeyRouter& router, SongEditor& editor,
                AudioEngine& engine) {
    const KeyFocus focus{.text_field = ImGui::GetIO().WantTextInput};
    const RoutedKey routed = router.route(key_event(key), focus);
    if (routed.shortcut) {
        run_shortcut(*routed.shortcut, router.note_keys(), editor, engine);
    }
    if (routed.note) {
        engine.send(*routed.note);
    }
}

// Handles the events waiting in SDL's queue, and returns false once the user has asked to quit.
[[nodiscard]] bool handle_events(SDL_Window* window, float font_size, Settings& settings,
                                 SettingsFiles& files, KeyRouter& router, SongEditor& editor,
                                 AudioEngine& engine) {
    bool keep_running = true;
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        const bool closed = event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                            event.window.windowID == SDL_GetWindowID(window);
        if (event.type == SDL_EVENT_QUIT || closed) {
            keep_running = false;
        } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            handle_key(event.key, router, editor, engine);
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            router.pointer_pressed();
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            // The keys still held will go up where Studio can't see them.
            engine.send(router.release_all());
        } else if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
            apply_theme(content_scale(window), font_size);
        } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                   event.type == SDL_EVENT_WINDOW_MAXIMIZED ||
                   event.type == SDL_EVENT_WINDOW_RESTORED) {
            remember_window(window, settings);
            files.change_settings(settings, DelayedFile::Clock::now());
        }
    }
    return keep_running;
}

// Opens the audio device again where the user picked a device or a buffer size, or pressed Reopen,
// and closes a device that has stopped calling back. Either way the player is new, or gone, so the
// keys held are forgotten.
void update_audio(const AudioWindow& window, Settings& settings, SettingsFiles& files,
                  NoteKeys& keys, AudioEngine& engine, DelayedFile::Clock::time_point now) {
    if (window.wants_open()) {
        settings.audio_device = window.chosen_device.value_or(settings.audio_device);
        settings.buffer_size = window.chosen_buffer_size.value_or(settings.buffer_size);
        files.change_settings(settings, now);
        keys.forget();
        static_cast<void>(engine.open(settings.audio_device, settings.buffer_size));
    }
    if (engine.watch(now)) {
        keys.forget();
    }
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
        return fail(options, "Couldn't start SDL's video");
    }
    const Deferred quit_sdl{[] { SDL_Quit(); }};

    SettingsFiles files{config_directory(options.config_dir, read_environment())};
    Settings settings = files.settings();

    const float scale = display_content_scale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags =
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    if (settings.window_maximized) {
        window_flags |= SDL_WINDOW_MAXIMIZED;
    }
    SDL_Window* const window =
        SDL_CreateWindow(title, to_display(settings.window_width, scale),
                         to_display(settings.window_height, scale), window_flags);
    if (window == nullptr) {
        return fail(options, "Couldn't open the window");
    }
    const Deferred destroy_window{[window] { SDL_DestroyWindow(window); }};
    SDL_SetWindowMinimumSize(window, to_display(min_window_width, scale),
                             to_display(min_window_height, scale));

    // ImGui's shaders for Direct3D 12 are DXBC, which SDL's Direct3D 12 driver always takes.
    SDL_GPUDevice* const device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_METALLIB,
                            false, nullptr);
    if (device == nullptr) {
        return fail(options, "Couldn't create the GPU device");
    }
    const Deferred destroy_device{[device] { SDL_DestroyGPUDevice(device); }};
    // Direct3D 12 sets no error of its own when it can't make the window's swapchain, so an
    // earlier step's error mustn't stand in for it.
    SDL_ClearError();
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        return fail(options, "Couldn't claim the window for the GPU device");
    }
    const Deferred release_window{[device, window] {
        SDL_WaitForGPUIdle(device);
        SDL_ReleaseWindowFromGPUDevice(device, window);
    }};
    if (!SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       SDL_GPU_PRESENTMODE_VSYNC)) {
        return fail(options, "Couldn't present with vsync");
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    const Deferred destroy_context{[] { ImGui::DestroyContext(); }};
    ImGuiIO& io = ImGui::GetIO();
    // ImGui hands its layout over as text, which Studio keeps in its settings directory, rather
    // than writing it to the working directory. ImGui reports a change to the layout on the frame
    // after it, and the file waits for the layout to settle.
    io.IniFilename = nullptr;
    io.IniSavingRate = std::numeric_limits<float>::min();
    if (!files.layout().empty()) {
        ImGui::LoadIniSettingsFromMemory(files.layout().data(), files.layout().size());
    }
    // Windows dock inside the main window only. Windows of their own work only under some video
    // drivers, and never on Wayland.
    io.ConfigFlags = combine_flags(io.ConfigFlags, ImGuiConfigFlags_DockingEnable);
    const auto font_size = static_cast<float>(settings.font_size);
    apply_theme(content_scale(window), font_size);
    load_fonts(font_size);

    if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
        return fail(options, "Couldn't start ImGui's SDL backend");
    }
    const Deferred shut_down_platform{[] { ImGui_ImplSDL3_Shutdown(); }};
    ImGui_ImplSDLGPU3_InitInfo renderer{};
    renderer.Device = device;
    renderer.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    renderer.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&renderer)) {
        return fail(options, "Couldn't start ImGui's SDL_GPU backend");
    }
    const Deferred shut_down_renderer{[device] {
        SDL_WaitForGPUIdle(device);
        ImGui_ImplSDLGPU3_Shutdown();
    }};

    // With no device, Studio runs silent, and the Audio window says why.
    SongEditor editor;
    AudioEngine engine{studio_backends, editor.document()};
    static_cast<void>(engine.open(settings.audio_device, settings.buffer_size));
    KeyRouter router{shortcut_table(), settings.octave};
    NoteKeys& keys = router.note_keys();
    MeterDisplay meters;

    Workspace workspace;
    StatusBar status;
    std::uint64_t frames = 0;
    bool running = true;
    while (running) {
        // A window that can't be seen draws nothing until something happens to it, or the wait
        // runs out.
        const SDL_WindowFlags hidden = SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN;
        if ((SDL_GetWindowFlags(window) & hidden) != 0) {
            SDL_WaitEventTimeout(nullptr, hidden_wait_ms);
        }
        if (!handle_events(window, font_size, settings, files, router, editor, engine)) {
            running = false;
        }
        engine.flush();
        meters.update(
            MeterDisplay::Clock::now(), [&engine] { return engine.take_cpu_peak(); },
            engine.take_clipped());

        status.octave = keys.octave();
        status.voices = engine.active_voices();
        status.cpu_peak = meters.cpu_peak();
        status.clipping = meters.clipping();
        status.refusal = editor.refusal();
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        if (!workspace.draw(status, engine, editor)) {
            running = false;
        }
        ImGui::Render();
        render(device, window);
        if (const std::optional chosen = workspace.chosen_shortcut()) {
            run_shortcut(*chosen, keys, editor, engine);
        }

        const DelayedFile::Clock::time_point now = DelayedFile::Clock::now();
        update_audio(workspace.audio_window(), settings, files, keys, engine, now);
        keys.set_octave(keys.octave() + status.octave_step);
        if (keys.octave() != settings.octave) {
            settings.octave = keys.octave();
            files.change_settings(settings, now);
        }
        if (io.WantSaveIniSettings) {
            io.WantSaveIniSettings = false;
            files.change_layout(ImGui::SaveIniSettingsToMemory(), now);
        }
        files.write_if_due(now);

        ++frames;
        if (options.exit_after_frames && frames >= *options.exit_after_frames) {
            running = false;
        }
    }

    const DelayedFile::Clock::time_point now = DelayedFile::Clock::now();
    files.change_settings(settings, now);
    files.change_layout(ImGui::SaveIniSettingsToMemory(), now);
    files.flush();
    return 0;
}

} // namespace irama_studio
