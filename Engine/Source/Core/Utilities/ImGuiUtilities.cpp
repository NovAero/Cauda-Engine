#include "cepch.h"
#include "ImGuiUtilities.h"
#include "Platform/Win32/Application.h"
#include "imgui.h"
#include <iostream>
#include <unordered_map>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Cauda
{
    namespace Fonts
    {
        ImFont* Default = nullptr;
        ImFont* Custom = nullptr;

        bool LoadFonts()
        {
            ImGuiIO& io = ImGui::GetIO();
            io.Fonts->Clear();
            Default = io.Fonts->AddFontDefault();
            try
            {
                Custom = io.Fonts->AddFontFromFileTTF("content/fonts/pixelparchment.ttf", 18.0f);
                if (!Custom) Custom = Default;
            }
            catch (...)
            {
                Logger::PrintLog("Warning: Failed to load custom font, using default font instead");
            }
            bool success = io.Fonts->Build();
            if (!success)
            {
                Logger::PrintLog("Error: Failed to build font atlas");
                return false;
            }
            SetDefaultFont();
            Logger::PrintLog("Fonts loaded successfully");
            return true;
        }

        void SetDefaultFont()
        {
            if (Default)
            {
                ImGui::GetIO().FontDefault = Default;
            }
        }
    }

    namespace UI
    {
        void UpdateSpriteAnimation(SpriteAnimation& anim, float deltaTime)
        {
            if (!anim.isPlaying || !anim.texture) return;

            anim.currentTime += deltaTime;

            if (anim.currentTime >= anim.frameDuration)
            {
                anim.currentTime -= anim.frameDuration;

                if (!anim.pingPong)
                {

                    anim.currentFrame++;
                    if (anim.currentFrame > anim.endFrame)
                    {
                        if (anim.loop)
                        {
                            anim.currentFrame = anim.startFrame;
                        }
                        else
                        {
                            anim.currentFrame = anim.endFrame;
                            anim.isPlaying = false;
                        }
                    }
                }
                else
                {
                    if (!anim.reversing)
                    {
                        anim.currentFrame++;
                        if (anim.currentFrame >= anim.endFrame)
                        {
                            if (anim.loop)
                            {
                                anim.reversing = true;
                            }
                            else
                            {
                                anim.currentFrame = anim.endFrame;
                                anim.isPlaying = false;
                            }
                        }
                    }
                    else
                    {
                        anim.currentFrame--;
                        if (anim.currentFrame <= anim.startFrame)
                        {
                            anim.reversing = false;
                            if (!anim.loop)
                            {
                                anim.isPlaying = false;
                            }
                        }
                    }
                }
            }
        }

        void DrawSpriteAnimation(const SpriteAnimation& anim, ImVec2 pos, ImVec2 size, ImVec4 tint)
        {
            if (!anim.texture) return;

            int cellX = anim.currentFrame % anim.atlasColumns;
            int cellY = anim.currentFrame / anim.atlasColumns;

            DrawImageAtlasCell(anim.texture, pos, size, cellX, cellY,
                anim.atlasColumns, anim.atlasRows, tint);
        }

        void DrawSpriteAnimationAnchored(const SpriteAnimation& anim, Anchor anchor, ImVec2 size, ImVec2 offset, ImVec4 tint)
        {
            ImVec2 pos = GetAnchorPosition(anchor, size, offset);
            DrawSpriteAnimation(anim, pos, size, tint);
        }

        void PlaySpriteAnimation(SpriteAnimation& anim)
        {
            anim.isPlaying = true;
        }

        void PauseSpriteAnimation(SpriteAnimation& anim)
        {
            anim.isPlaying = false;
        }

        void StopSpriteAnimation(SpriteAnimation& anim)
        {
            anim.isPlaying = false;
            anim.currentFrame = anim.startFrame;
            anim.currentTime = 0.0f;
            anim.reversing = false;
        }

        void RestartSpriteAnimation(SpriteAnimation& anim)
        {
            StopSpriteAnimation(anim);
            PlaySpriteAnimation(anim);
        }

        void SetAnimationFrame(SpriteAnimation& anim, int frame)
        {
            if (frame >= anim.startFrame && frame <= anim.endFrame)
            {
                anim.currentFrame = frame;
                anim.currentTime = 0.0f;
            }
        }

        bool IsAnimationFinished(const SpriteAnimation& anim)
        {
            return !anim.isPlaying &&
                ((anim.currentFrame == anim.endFrame && !anim.reversing) ||
                    (anim.pingPong && anim.currentFrame == anim.startFrame && anim.reversing));
        }

        struct GridState
        {
            int columns = 0;
            int currentColumn = 0;
            ImVec2 cellSize = ImVec2(0, 0);
            float spacing = 4.0f;
            ImVec2 startPos = ImVec2(0, 0);
            bool active = false;
        };
        static GridState s_gridState;

        ImVec2 GetViewportSize()
        {
            return ImGui::GetIO().DisplaySize;
        }

        ImVec2 GetViewportCentre()
        {
            ImVec2 size = GetViewportSize();
            return ImVec2(size.x * 0.5f, size.y * 0.5f);
        }

        ImVec2 GetAnchorPosition(Anchor anchor, ImVec2 offset)
        {
            ImVec2 viewport = GetViewportSize();
            ImVec2 windowPos;
            {
                int x, y;
                SDL_GetWindowPosition(Cauda::Application::Get().GetSDLWindow(), &x, &y);
                windowPos = { (float)x,(float)y };
            }
            ImVec2 pos(0, 0);

            switch (anchor)
            {
            case Anchor::TopLeft:     pos = windowPos; break;
            case Anchor::TopCentre:   pos = ImVec2(windowPos.x + viewport.x * 0.5f, windowPos.y); break;
            case Anchor::TopRight:    pos = ImVec2(windowPos.x + viewport.x, windowPos.y); break;
            case Anchor::CentreLeft:  pos = ImVec2(windowPos.x, windowPos.y + viewport.y * 0.5f); break;
            case Anchor::Centre:      pos = ImVec2(windowPos.x + viewport.x * 0.5f, windowPos.y + viewport.y * 0.5f); break;
            case Anchor::CentreRight: pos = ImVec2(windowPos.x + viewport.x, windowPos.y + viewport.y * 0.5f); break;
            case Anchor::BottomLeft:  pos = ImVec2(windowPos.x, windowPos.y + viewport.y); break;
            case Anchor::BottomCentre: pos = ImVec2(windowPos.x + viewport.x * 0.5f, windowPos.y + viewport.y); break;
            case Anchor::BottomRight: pos = ImVec2(windowPos.x + viewport.x, windowPos.y + viewport.y); break;
            }

            return ImVec2(pos.x + offset.x, pos.y + offset.y);
        }

        void SetWindowAnchor(Anchor anchor, ImVec2 offset)
        {
            ImVec2 pos = GetAnchorPosition(anchor, offset);
            ImGui::SetNextWindowPos(pos);
        }

        ImVec2 GetAnchorPosition(Anchor anchor, ImVec2 elementSize, ImVec2 offset)
        {
            ImVec2 pos = GetAnchorPosition(anchor, offset);

            switch (anchor)
            {
            case Anchor::TopCentre:
            case Anchor::Centre:
            case Anchor::BottomCentre:
                pos.x -= elementSize.x * 0.5f;
                break;
            case Anchor::TopRight:
            case Anchor::CentreRight:
            case Anchor::BottomRight:
                pos.x -= elementSize.x;
                break;
            }

            switch (anchor)
            {
            case Anchor::CentreLeft:
            case Anchor::Centre:
            case Anchor::CentreRight:
                pos.y -= elementSize.y * 0.5f;
                break;
            case Anchor::BottomLeft:
            case Anchor::BottomCentre:
            case Anchor::BottomRight:
                pos.y -= elementSize.y;
                break;
            }

            return pos;
        }

        bool BeginOverlay(const char* name, Anchor anchor, ImVec2 size, ImVec2 offset)
        {
            ImVec2 pos = GetAnchorPosition(anchor, size, offset);
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground|
                ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_AlwaysAutoResize;

            return ImGui::Begin(name, nullptr, flags);
        }

        void EndOverlay()
        {
            ImGui::End();
        }

        bool BeginFullscreenOverlay(const char* name, float backgroundAlpha)
        {
            ImVec2 viewportSize = GetViewportSize();

            ImVec2 anchor = GetAnchorPosition(Anchor::TopLeft);

            ImGui::SetNextWindowPos(ImVec2(anchor.x, anchor.y));
            ImGui::SetNextWindowSize(viewportSize);
            ImGui::SetNextWindowBgAlpha(backgroundAlpha);

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoBringToFrontOnFocus;

            if (backgroundAlpha == 0.0f)
                flags |= ImGuiWindowFlags_NoBackground;

            return ImGui::Begin(name, nullptr, flags);
        }

        void EndFullscreenOverlay()
        {
            ImGui::End();
        }

        bool BeginGrid(int columns, ImVec2 cellSize, float spacing)
        {
            if (columns <= 0) return false;

            s_gridState.columns = columns;
            s_gridState.currentColumn = 0;
            s_gridState.cellSize = cellSize;
            s_gridState.spacing = spacing;
            s_gridState.startPos = ImGui::GetCursorPos();
            s_gridState.active = true;

            return true;
        }

        void GridNextColumn()
        {
            if (!s_gridState.active) return;

            s_gridState.currentColumn++;
            if (s_gridState.currentColumn >= s_gridState.columns)
            {
                GridNextRow();
                return;
            }

            ImVec2 pos = s_gridState.startPos;
            pos.x += s_gridState.currentColumn * (s_gridState.cellSize.x + s_gridState.spacing);
            ImGui::SetCursorPos(pos);
        }

        void GridNextRow()
        {
            if (!s_gridState.active) return;

            s_gridState.currentColumn = 0;
            s_gridState.startPos.y += s_gridState.cellSize.y + s_gridState.spacing;
            ImGui::SetCursorPos(s_gridState.startPos);
        }

        void EndGrid()
        {
            s_gridState.active = false;
        }

        void DrawProgressBar(ImVec2 pos, ImVec2 size, float progress, ImU32 bgColour, ImU32 fillColour, float rounding)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);

            drawList->AddRectFilled(pos, max, bgColour, rounding);

            if (progress > 0.0f)
            {
                progress = std::clamp(progress, 0.0f, 1.0f);
                ImVec2 fillMax = ImVec2(pos.x + size.x * progress, pos.y + size.y);
                drawList->AddRectFilled(pos, fillMax, fillColour, rounding);
            }
        }

        void DrawCircularProgress(ImVec2 centre, float radius, float progress, ImU32 colour, float thickness)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            progress = std::clamp(progress, 0.0f, 1.0f);
            int segments = 32;
            float angleStep = (2.0f * M_PI) / segments;
            float endAngle = -M_PI * 0.5f + (2.0f * M_PI * progress);

            for (int i = 0; i < segments; ++i)
            {
                float angle1 = -M_PI * 0.5f + angleStep * i;
                float angle2 = -M_PI * 0.5f + angleStep * (i + 1);

                if (angle1 > endAngle) break;
                if (angle2 > endAngle) angle2 = endAngle;

                ImVec2 p1 = ImVec2(centre.x + cosf(angle1) * radius, centre.y + sinf(angle1) * radius);
                ImVec2 p2 = ImVec2(centre.x + cosf(angle2) * radius, centre.y + sinf(angle2) * radius);

                drawList->AddLine(p1, p2, colour, thickness);
            }
        }

        void DrawImage(ImTextureID texture, ImVec2 pos, ImVec2 size, ImVec4 tint)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
            drawList->AddImage(texture, pos, max, ImVec2(0, 0), ImVec2(1, 1), ColourToU32(tint));
        }

        void DrawImageAnchored(ImTextureID texture, Anchor anchor, ImVec2 size, ImVec2 offset, ImVec4 tint)
        {
            ImVec2 pos = GetAnchorPosition(anchor, size, offset);
            DrawImage(texture, pos, size, tint);
        }

        void DrawImageAtlas(ImTextureID texture, ImVec2 pos, ImVec2 size,
            ImVec2 uvMin, ImVec2 uvMax, ImVec4 tint)
        {
            if (!texture) return;

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
            drawList->AddImage(texture, pos, max, uvMin, uvMax, ColourToU32(tint));
        }

        void DrawImageAtlasCell(ImTextureID texture, ImVec2 pos, ImVec2 size,
            int cellX, int cellY, int atlasColumns, int atlasRows,
            ImVec4 tint)
        {
            if (!texture || atlasColumns <= 0 || atlasRows <= 0) return;
            if (cellX < 0 || cellX >= atlasColumns || cellY < 0 || cellY >= atlasRows) return;

            float cellWidth = 1.0f / atlasColumns;
            float cellHeight = 1.0f / atlasRows;

            ImVec2 uvMin = ImVec2(cellX * cellWidth, cellY * cellHeight);
            ImVec2 uvMax = ImVec2((cellX + 1) * cellWidth, (cellY + 1) * cellHeight);

            DrawImageAtlas(texture, pos, size, uvMin, uvMax, tint);
        }

        void DrawImageAtlasIndex(ImTextureID texture, ImVec2 pos, ImVec2 size,
            int index, int atlasColumns, int atlasRows, ImVec4 tint)
        {
            int cellX = index % atlasColumns;
            int cellY = index / atlasColumns;
            DrawImageAtlasCell(texture, pos, size, cellX, cellY, atlasColumns, atlasRows, tint);
        }

        void DrawImageAtlasPixelRect(ImTextureID texture, ImVec2 pos, ImVec2 size,
            float pixelX, float pixelY, float pixelWidth, float pixelHeight,
            float textureWidth, float textureHeight, ImVec4 tint)
        {
            if (!texture || textureWidth <= 0.0f || textureHeight <= 0.0f) return;

            ImVec2 uvMin = ImVec2(pixelX / textureWidth, pixelY / textureHeight);
            ImVec2 uvMax = ImVec2((pixelX + pixelWidth) / textureWidth, (pixelY + pixelHeight) / textureHeight);

            DrawImageAtlas(texture, pos, size, uvMin, uvMax, tint);
        }

        void DrawImageAtlasNormalized(ImTextureID texture, ImVec2 pos, ImVec2 size,
            float normX, float normY, float normWidth, float normHeight,
            ImVec4 tint)
        {
            if (!texture) return;

            ImVec2 uvMin = ImVec2(normX, normY);
            ImVec2 uvMax = ImVec2(normX + normWidth, normY + normHeight);

            DrawImageAtlas(texture, pos, size, uvMin, uvMax, tint);
        }

        void DrawImageAtlasRect(ImTextureID texture, ImVec2 pos, ImVec2 size,
            const SpriteRect& rect, float textureWidth, float textureHeight,
            ImVec4 tint)
        {
            DrawImageAtlasPixelRect(texture, pos, size, rect.x, rect.y, rect.width, rect.height,
                textureWidth, textureHeight, tint);
        }

        bool ButtonWithImage(const char* label, ImTextureID texture, ImVec2 buttonSize, ImVec2 imageSize, ImVec4 tint)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            bool pressed = false;
            bool held = false;

            if (ImGui::Button(label, buttonSize))
            {
                pressed = true;
            }
            else if (ImGui::IsItemActive())
            {
                held = true;
            }
            else if (ImGui::IsItemHovered())
            {
                tint = ImVec4(tint.x * 0.5, tint.y * 0.5, tint.z * 0.5, tint.w);
            }

            ImVec2 imageSizeDifference = ImVec2(imageSize.x - buttonSize.x, imageSize.y - buttonSize.y);
            pos = ImVec2(pos.x - (imageSizeDifference.x / 2), pos.y - (imageSizeDifference.y / 2));

            DrawImage(texture, pos, imageSize, tint);
            return pressed;
        }

        bool ButtonWithImageAtlasCell(const char* label, ImTextureID texture, ImVec2 buttonSize, ImVec2 imageSize, ImVec2 normalCellCoords, ImVec2 hoveredCellCoords, ImVec2 atlasLayout, ImVec4 tint)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            bool pressed = false;
            bool held = false;

            if (ImGui::Button(label, buttonSize))
            {
                pressed = true;
            }
            else if (ImGui::IsItemActive())
            {
                held = true;
            }
            else if (ImGui::IsItemHovered())
            {
                tint = ImVec4(tint.x * 0.5, tint.y * 0.5, tint.z * 0.5, tint.w);
            }

            ImVec2 imageSizeDifference = ImVec2(imageSize.x - buttonSize.x, imageSize.y - buttonSize.y);
            pos = ImVec2(pos.x - (imageSizeDifference.x / 2), pos.y - (imageSizeDifference.y / 2));

            ImVec2 resultCell = pressed || held ? hoveredCellCoords : normalCellCoords;

            DrawImageAtlasCell(texture, pos, imageSize, resultCell.x, resultCell.y, atlasLayout.x, atlasLayout.y, tint);
            return pressed;
        }

        bool ButtonWithImageAtlasIndex(const char* label, ImTextureID texture, ImVec2 buttonSize, ImVec2 imageSize, int normalIndex, int pressedIndex, ImVec2 atlasLayout, ImVec4 tint)
        {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            bool pressed = false;
            bool held = false;

            if (ImGui::Button(label, buttonSize))
            {
                pressed = true;
            }
            else if (ImGui::IsItemActive())
            {
                held = true;
            }
            else if (ImGui::IsItemHovered())
            {
                tint = ImVec4(tint.x * 0.5, tint.y * 0.5, tint.z * 0.5, tint.w);
            }

            ImVec2 imageSizeDifference = ImVec2(imageSize.x - buttonSize.x, imageSize.y - buttonSize.y);
            pos = ImVec2(pos.x - (imageSizeDifference.x / 2), pos.y - (imageSizeDifference.y / 2));

            int resultIndex = pressed || held ? pressedIndex : normalIndex;

            DrawImageAtlasIndex(texture, pos, imageSize, resultIndex, atlasLayout.x, atlasLayout.y, tint);

            return pressed;
        }

        void TextCentred(const char* text, ImVec2 centre)
        {
            if (centre.x == 0 && centre.y == 0)
                centre = GetViewportCentre();

            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImVec2 textPos = ImVec2(centre.x - textSize.x * 0.5f, centre.y - textSize.y * 0.5f);

            ImGui::SetCursorScreenPos(textPos);
            ImGui::Text("%s", text);
        }


        ImU32 ColourToU32(const ImVec4& colour)
        {
            return ImGui::ColorConvertFloat4ToU32(colour);
        }
    }
}

//    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
//    ImGui::ProgressBar(value1, { -1, 0 }, nullptr);
//    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

//    if (!value2)
//        value2 = ImRad::LoadTextureFromFile("");
//    ImGui::Image(value2.id, { 96, 96 }, { 0, 0 }, { 1, 1 }); 

//    ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
//    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

//    ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
//    if (!value3)
//    ImGui::Image(value3.id, { 32, 32 }, { 0, 0 }, { 1, 1 }); 

//    ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
//    if (!value4)
//    ImGui::Image(value4.id, { 32, 32 }, { 0, 0 }, { 1, 1 }); 

//    ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
//    if (!value5)
//    ImGui::Image(value5.id, { 32, 32 }, { 0, 0 }, { 1, 1 }); 

//    ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
//    if (!value7)
//    ImGui::Image(value7.id, { 32, 32 }, { 0, 0 }, { 1, 1 }); 

//    ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.x);
//    if (!value8)
//    ImGui::Image(value8.id, { 32, 32 }, { 0, 0 }, { 1, 1 });
