#pragma once
#include "imgui.h"
#include <functional>

namespace Cauda
{
    namespace Colour
    {
        constexpr ImVec4 LeadDarkest = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        constexpr ImVec4 LeadDark = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        constexpr ImVec4 LeadMid = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
        constexpr ImVec4 LeadLight = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
        constexpr ImVec4 LeadLightest = ImVec4(0.45f, 0.45f, 0.48f, 1.00f);
        constexpr ImVec4 HumbleRed = ImVec4(0.85f, 0.48f, 0.50f, 1.00f);
        constexpr ImVec4 HumbleBlue = ImVec4(0.48f, 0.50f, 0.85f, 1.00f);
        constexpr ImVec4 HumbleGreen = ImVec4(0.45f, 0.85f, 0.55f, 1.00f);
        constexpr ImVec4 HumbleOrange = ImVec4(0.85f, 0.58f, 0.25f, 1.00f);
        constexpr ImVec4 HumblePurple = ImVec4(0.65f, 0.48f, 0.85f, 1.00f);
        constexpr ImVec4 HumblePink = ImVec4(0.85f, 0.48f, 0.85f, 1.00f);
        constexpr ImVec4 HumbleYellow = ImVec4(0.85f, 0.84f, 0.50f, 1.00f);
        constexpr ImVec4 HumbleGold = ImVec4(0.90f, 0.70f, 0.13f, 1.00f);
        constexpr ImVec4 HumbleSilver = ImVec4(0.75f, 0.77f, 0.80f, 1.00f);
        constexpr ImVec4 HumbleCopper = ImVec4(0.80f, 0.62f, 0.45f, 1.00f);
        constexpr ImVec4 TextWhite = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        constexpr ImVec4 TextDisabled = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);

        constexpr ImVec4 WithAlpha(const ImVec4& colour, float alpha)
        {
            return ImVec4(colour.x, colour.y, colour.z, alpha);
        }
        constexpr ImVec4 Brighten(const ImVec4& colour, float amount = 0.1f)
        {
            return ImVec4(
                colour.x + amount > 1.0f ? 1.0f : colour.x + amount,
                colour.y + amount > 1.0f ? 1.0f : colour.y + amount,
                colour.z + amount > 1.0f ? 1.0f : colour.z + amount,
                colour.w
            );
        }
        constexpr ImVec4 Darken(const ImVec4& colour, float amount = 0.1f)
        {
            return ImVec4(
                colour.x - amount < 0.0f ? 0.0f : colour.x - amount,
                colour.y - amount < 0.0f ? 0.0f : colour.y - amount,
                colour.z - amount < 0.0f ? 0.0f : colour.z - amount,
                colour.w
            );
        }
    }

    namespace Fonts
    {
        extern ImFont* Default;
        extern ImFont* Custom;
        bool LoadFonts();
        void SetDefaultFont();
    }

    namespace UI
    {
        struct SpriteAnimation
        {
            ImTextureID texture = -1;
            int atlasColumns = 1;
            int atlasRows = 1;
            int startFrame = 0;
            int endFrame = 0;
            float frameDuration = 0.1f;    
            bool loop = true;
            bool pingPong = false;          

            float currentTime = 0.0f;
            int currentFrame = 0;
            bool reversing = false;         
            bool isPlaying = true;

            SpriteAnimation() = default;

            SpriteAnimation(ImTextureID tex, int cols, int rows, int start, int end,
                float duration = 0.1f, bool shouldLoop = true, bool shouldPingPong = false)
                : texture(tex), atlasColumns(cols), atlasRows(rows), startFrame(start),
                endFrame(end), frameDuration(duration), loop(shouldLoop), pingPong(shouldPingPong),
                currentFrame(start) {
            }
        };

        void UpdateSpriteAnimation(SpriteAnimation& anim, float deltaTime);
        void DrawSpriteAnimation(const SpriteAnimation& anim, ImVec2 pos, ImVec2 size, ImVec4 tint = ImVec4(1, 1, 1, 1));

        void PlaySpriteAnimation(SpriteAnimation& anim);
        void PauseSpriteAnimation(SpriteAnimation& anim);
        void StopSpriteAnimation(SpriteAnimation& anim);           
        void RestartSpriteAnimation(SpriteAnimation& anim);     
        void SetAnimationFrame(SpriteAnimation& anim, int frame);
        bool IsAnimationFinished(const SpriteAnimation& anim);

        enum class Anchor
        {
            TopLeft, TopCentre, TopRight,
            CentreLeft, Centre, CentreRight,
            BottomLeft, BottomCentre, BottomRight
        };



        ImVec2 GetAnchorPosition(Anchor anchor, ImVec2 offset = ImVec2(0, 0));
        ImVec2 GetAnchorPosition(Anchor anchor, ImVec2 elementSize, ImVec2 offset);
        void SetWindowAnchor(Anchor anchor, ImVec2 offset = ImVec2(0, 0));

        bool BeginOverlay(const char* name, Anchor anchor = Anchor::TopLeft,
            ImVec2 size = ImVec2(0, 0), ImVec2 offset = ImVec2(0, 0));
        void EndOverlay();

        bool BeginFullscreenOverlay(const char* name, float backgroundAlpha = 0.0f);
        void EndFullscreenOverlay();

        bool BeginGrid(int columns, ImVec2 cellSize = ImVec2(0, 0), float spacing = 4.0f);
        void GridNextColumn();
        void GridNextRow();
        void EndGrid();

        void DrawProgressBar(ImVec2 pos, ImVec2 size, float progress, ImU32 bgColour, ImU32 fillColour, float rounding = 0.0f);
        void DrawCircularProgress(ImVec2 centre, float radius, float progress, ImU32 colour, float thickness = 4.0f);

        void DrawImage(ImTextureID texture, ImVec2 pos, ImVec2 size, ImVec4 tint = ImVec4(1, 1, 1, 1));
        void DrawImageAnchored(ImTextureID texture, Anchor anchor, ImVec2 size, ImVec2 offset = ImVec2(0, 0), ImVec4 tint = ImVec4(1, 1, 1, 1));

        void DrawImageAtlas(ImTextureID texture, ImVec2 pos, ImVec2 size,
            ImVec2 uvMin, ImVec2 uvMax, ImVec4 tint = ImVec4(1, 1, 1, 1));
        void DrawImageAtlasCell(ImTextureID texture, ImVec2 pos, ImVec2 size,
            int cellX, int cellY, int atlasColumns, int atlasRows,
            ImVec4 tint = ImVec4(1, 1, 1, 1));
        void DrawImageAtlasIndex(ImTextureID texture, ImVec2 pos, ImVec2 size,
            int index, int atlasColumns, int atlasRows, ImVec4 tint);

        void DrawImageAtlasPixelRect(ImTextureID texture, ImVec2 pos, ImVec2 size,
            float pixelX, float pixelY, float pixelWidth, float pixelHeight,
            float textureWidth, float textureHeight, ImVec4 tint = ImVec4(1, 1, 1, 1));

        void DrawImageAtlasNormalized(ImTextureID texture, ImVec2 pos, ImVec2 size,
            float normX, float normY, float normWidth, float normHeight,
            ImVec4 tint = ImVec4(1, 1, 1, 1));

        struct SpriteRect
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;

            SpriteRect() = default;
            SpriteRect(float _x, float _y, float _w, float _h)
                : x(_x), y(_y), width(_w), height(_h) {}
        };

        void DrawImageAtlasRect(ImTextureID texture, ImVec2 pos, ImVec2 size,
            const SpriteRect& rect, float textureWidth, float textureHeight,
            ImVec4 tint = ImVec4(1, 1, 1, 1));

        bool ButtonWithImage(const char* label, ImTextureID texture, ImVec2 buttonSize, ImVec2 imageSize, ImVec4 tint);

        bool ButtonWithImageAtlasCell(const char* label, ImTextureID texture, ImVec2 buttonSize, ImVec2 imageSize,
            ImVec2 normalCellCoords, ImVec2 hoveredCellCoords, ImVec2 atlasLayout, ImVec4 tint);

        bool ButtonWithImageAtlasIndex(const char* label, ImTextureID texture, ImVec2 buttonSize, ImVec2 imageSize,
            int normalIndex, int pressedIndex, ImVec2 atlasLayout, ImVec4 tint);

        void TextCentred(const char* text, ImVec2 centre = ImVec2(0, 0));

        ImVec2 GetViewportSize();
        ImVec2 GetViewportCentre();
        ImU32 ColourToU32(const ImVec4& colour);
    }
}