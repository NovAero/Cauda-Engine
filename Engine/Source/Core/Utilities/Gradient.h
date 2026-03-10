#pragma once

#include "ImGradient.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>

namespace Gradient
{
    struct ColourStop
    {
        float position;  // 0-1
        glm::vec4 colour; // RGBA

        ColourStop(float pos, glm::vec4 col) : position(pos), colour(col) {}
        ColourStop(float pos, float r, float g, float b, float a = 1.0f)
            : position(pos), colour(r, g, b, a) {
        }
    };

    class Gradient
    {
    private:
        std::vector<ColourStop> stops;

    public:
        Gradient()
        {
            stops.push_back(ColourStop(0.0f, glm::vec4(0, 0, 0, 1)));
            stops.push_back(ColourStop(1.0f, glm::vec4(1, 1, 1, 1)));
        }

        glm::vec4 Evaluate(float t) const
        {
            if (stops.empty())
                return glm::vec4(0, 0, 0, 1);

            if (stops.size() == 1)
                return stops[0].colour;

            t = glm::clamp(t, 0.0f, 1.0f);

            for (size_t i = 0; i < stops.size() - 1; ++i)
            {
                if (t >= stops[i].position && t <= stops[i + 1].position)
                {
                    float alpha = (t - stops[i].position) / (stops[i + 1].position - stops[i].position);
                    return glm::mix(stops[i].colour, stops[i + 1].colour, alpha);
                }
            }

            if (t <= stops.front().position)
                return stops.front().colour;
            return stops.back().colour;
        }

        glm::vec4 operator()(float t) const { return Evaluate(t); }

        void AddStop(float position, const glm::vec4& color)
        {
            stops.push_back(ColourStop(position, color));
            SortStops();
        }

        void AddStop(float position, float r, float g, float b, float a = 1.0f)
        {
            stops.push_back(ColourStop(position, r, g, b, a));
            SortStops();
        }

        void SetStops(const std::vector<ColourStop>& newStops)
        {
            stops = newStops;
            SortStops();
        }

        void EditStop(size_t index, float position, const glm::vec4& color)
        {
            if (index < stops.size())
            {
                stops[index].position = position;
                stops[index].colour = color;
                SortStops();
            }
        }

        void RemoveStop(size_t index)
        {
            if (index < stops.size())
                stops.erase(stops.begin() + index);
        }

        void Clear()
        {
            stops.clear();
        }

        const std::vector<ColourStop>& GetStops() const { return stops; }
        size_t GetStopCount() const { return stops.size(); }

        ImU32 EvaluateU32(float t) const
        {
            glm::vec4 color = Evaluate(t);
            return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a));
        }

    private:
        void SortStops()
        {
            std::sort(stops.begin(), stops.end(),
                [](const ColourStop& a, const ColourStop& b) { return a.position < b.position; });
        }
    };

    class Editor : public ImGradient::Delegate
    {
    private:
        Gradient* gradient;
        mutable std::vector<ImVec4> pointsCache;

    public:
        Editor(Gradient* g) : gradient(g) {}

        size_t GetPointCount() override
        {
            return gradient->GetStopCount();
        }

        ImVec4* GetPoints() override
        {
            const auto& stops = gradient->GetStops();
            pointsCache.resize(stops.size());

            for (size_t i = 0; i < stops.size(); ++i)
            {
                const glm::vec4& c = stops[i].colour;
                pointsCache[i] = ImVec4(c.r, c.g, c.b, c.a);
                pointsCache[i].w = stops[i].position;
            }

            return pointsCache.data();
        }

        int EditPoint(int pointIndex, ImVec4 value) override
        {
            if (pointIndex >= 0 && pointIndex < (int)gradient->GetStopCount())
            {
                gradient->EditStop(pointIndex, value.w, glm::vec4(value.x, value.y, value.z, 1.0f));
            }
            return pointIndex;
        }

        ImVec4 GetPoint(float t) override
        {
            glm::vec4 color = gradient->Evaluate(t);
            ImVec4 result(color.r, color.g, color.b, color.a);
            result.w = t;  
            return result;
        }

        void AddPoint(ImVec4 value) override
        {
            gradient->AddStop(value.w, glm::vec4(value.x, value.y, value.z, 1.0f));
        }
    };
}