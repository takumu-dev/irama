// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

// Studio's windows drawn by ImGui with no SDL window and no GPU: the frames are built and thrown
// away, so ImGui's own checks, and every sanitizer, watch the drawing code, where the smoke test
// can't run.

#include "app/song_editor.hpp"
#include "audio/audio_device.hpp"
#include "audio/audio_engine.hpp"
#include "doc/edits.hpp"
#include "document_builder.hpp"
#include "ui/flags.hpp"
#include "ui/fonts.hpp"
#include "ui/theme.hpp"
#include "views/about.hpp"
#include "views/status_bar.hpp"
#include "views/workspace.hpp"
#include <irama/engine/voice/instrument.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include <span>

using irama_studio::AudioBackend;
using irama_studio::AudioEngine;
using irama_studio::SongEditor;

namespace {

constexpr float font_size = 15.0F;
constexpr int frames = 4;

// An ImGui context with a display of its own size and a renderer that only marks each texture
// request as done, as Studio's renderer would.
class HeadlessContext {
  public:
    HeadlessContext() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2{1280.0F, 800.0F};
        io.DeltaTime = 1.0F / 60.0F;
        io.ConfigFlags =
            irama_studio::combine_flags(io.ConfigFlags, ImGuiConfigFlags_DockingEnable);
        io.BackendFlags =
            irama_studio::combine_flags(io.BackendFlags, ImGuiBackendFlags_RendererHasTextures);
        irama_studio::apply_theme(1.0F, font_size);
        irama_studio::load_fonts(font_size);
    }

    HeadlessContext(const HeadlessContext&) = delete;
    HeadlessContext& operator=(const HeadlessContext&) = delete;
    HeadlessContext(HeadlessContext&&) = delete;
    HeadlessContext& operator=(HeadlessContext&&) = delete;

    ~HeadlessContext() {
        for (ImTextureData* texture : ImGui::GetPlatformIO().Textures) {
            if (texture->RefCount == 1) {
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
        ImGui::DestroyContext();
    }

    // Ends the frame, and honours its texture requests.
    static void render() {
        ImGui::Render();
        for (ImTextureData* texture : ImGui::GetPlatformIO().Textures) {
            if (texture->Status == ImTextureStatus_WantCreate ||
                texture->Status == ImTextureStatus_WantUpdates) {
                texture->SetTexID(ImTextureID{1});
                texture->SetStatus(ImTextureStatus_OK);
            } else if (texture->Status == ImTextureStatus_WantDestroy &&
                       texture->UnusedFrames > 0) {
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
    }
};

} // namespace

TEST_CASE("every window draws over a test song for a few frames with no window or GPU",
          "[studio][frames]") {
    const HeadlessContext context;
    SongEditor editor{irama_studio::test::test_song()};
    AudioEngine engine{std::span<const AudioBackend>{}, editor.document()};
    irama_studio::Workspace workspace;
    irama_studio::StatusBar status;
    status.clipping = true;
    for (int frame = 0; frame < frames; ++frame) {
        INFO("frame " << frame);
        ImGui::NewFrame();
        CHECK(workspace.draw(status, engine, editor));
        bool about_open = true;
        irama_studio::draw_about(about_open);
        HeadlessContext::render();
        CHECK(ImGui::GetCurrentContext()->ErrorCountCurrentFrame == 0);
        CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);

        // The next frame draws what an edit changed, and a refusal in the status bar.
        editor.apply(irama_studio::set_instrument_value(0x01, irama::InstrumentParam::cutoff,
                                                        100.0F * static_cast<float>(frame + 1)),
                     engine);
        editor.apply(irama_studio::delete_instrument(0x40), engine);
        status.refusal = editor.refusal();
    }
}

TEST_CASE("the Instrument window draws an emptied instrument 01", "[studio][frames]") {
    const HeadlessContext context;
    SongEditor editor;
    AudioEngine engine{std::span<const AudioBackend>{}, editor.document()};
    REQUIRE(editor.apply(irama_studio::delete_instrument(0x01), engine));
    irama_studio::Workspace workspace;
    irama_studio::StatusBar status;
    for (int frame = 0; frame < frames; ++frame) {
        ImGui::NewFrame();
        CHECK(workspace.draw(status, engine, editor));
        HeadlessContext::render();
        CHECK(ImGui::GetCurrentContext()->ErrorCountCurrentFrame == 0);
    }
}
