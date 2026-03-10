#pragma once

#include "spline.h"
#include "ImCurveEdit.h"
#include <glm/glm.hpp>
#include <vector>
#include <algorithm>

namespace Curve
{
    enum class Type
    {
        Linear,
        Smooth,      
        Step
    };

    class Curve
    {
    public:
        std::vector<glm::vec2> points;
        Type type;
    private:
        mutable tk::spline spline;
        mutable bool needsRebuild;

    public:
        Curve(Type t = Type::Smooth) : type(t), needsRebuild(true)
        {
            points.push_back(glm::vec2(0.0f, 0.0f));
            points.push_back(glm::vec2(1.0f, 1.0f));
        }

        float Evaluate(float t) const
        {
            if (points.empty()) return 0.0f;
            if (points.size() == 1) return points[0].y;

            if (needsRebuild)
            {
                RebuildSpline();
                needsRebuild = false;
            }

            switch (type)
            {
            case Type::Step:
                return EvaluateStep(t);
            case Type::Linear:
                return EvaluateLinear(t);
            case Type::Smooth:
                return static_cast<float>(spline(t));
            }
            return 0.0f;
        }

        float operator()(float t) const { return Evaluate(t); }

        void AddPoint(float x, float y)
        {
            points.push_back(glm::vec2(x, y));
            SortPoints();
            needsRebuild = true;
        }

        void AddPoint(const glm::vec2& point)
        {
            points.push_back(point);
            SortPoints();
            needsRebuild = true;
        }

        void SetPoints(const std::vector<glm::vec2>& newPoints)
        {
            points = newPoints;
            SortPoints();
            needsRebuild = true;
        }

        void EditPoint(size_t index, const glm::vec2& value)
        {
            if (index < points.size())
            {
                points[index] = value;
                SortPoints();
                needsRebuild = true;
            }
        }

        void RemovePoint(size_t index)
        {
            if (index < points.size())
            {
                points.erase(points.begin() + index);
                needsRebuild = true;
            }
        }

        void Clear()
        {
            points.clear();
            needsRebuild = true;
        }

        void SetType(Type t)
        {
            if (type != t)
            {
                type = t;
                needsRebuild = true;
            }
        }

        Type GetType() const { return type; }
        const std::vector<glm::vec2>& GetPoints() const { return points; }
        size_t GetPointCount() const { return points.size(); }

        void SetState(const std::vector<glm::vec2>& newPoints, Type newType)
        {
            points = newPoints;
            type = newType;
            needsRebuild = true;
        }

        void Rebuild()
        {
            needsRebuild = true;
        }

    private:
        void SortPoints()
        {
            std::sort(points.begin(), points.end(),
                [](const glm::vec2& a, const glm::vec2& b) { return a.x < b.x; });
        }

        void RebuildSpline() const
        {
            if (points.size() < 2) return;

            std::vector<double> x, y;
            for (const auto& p : points)
            {
                x.push_back(p.x);
                y.push_back(p.y);
            }

            spline.set_points(x, y, tk::spline::cspline);
        }

        float EvaluateLinear(float t) const
        {
            if (t <= points.front().x) return points.front().y;
            if (t >= points.back().x) return points.back().y;

            for (size_t i = 0; i < points.size() - 1; ++i)
            {
                if (t >= points[i].x && t <= points[i + 1].x)
                {
                    float alpha = (t - points[i].x) / (points[i + 1].x - points[i].x);
                    return points[i].y + alpha * (points[i + 1].y - points[i].y);
                }
            }
            return points.back().y;
        }

        float EvaluateStep(float t) const
        {
            if (t <= points.front().x) return points.front().y;
            if (t >= points.back().x) return points.back().y;

            for (size_t i = 0; i < points.size() - 1; ++i)
            {
                if (t >= points[i].x && t < points[i + 1].x)
                    return points[i].y;
            }
            return points.back().y;
        }
    };

    class Editor : public ImCurveEdit::Delegate
    {
    private:
        Curve* curve;
        ImVec2 min, max;
        uint32_t color;
        mutable std::vector<ImVec2> pointsCache;
        bool showPointValues;

    public:
        Editor(Curve* c, glm::vec2 rangeMin = glm::vec2(0, 0), glm::vec2 rangeMax = glm::vec2(1, 1), uint32_t col = 0xFFFFFFFF)
            : curve(c), min(rangeMin.x, rangeMin.y), max(rangeMax.x, rangeMax.y), color(col), showPointValues(true) {
        }

        void SetShowPointValues(bool show) { showPointValues = show; }

        size_t GetCurveCount() override { return 1; }

        ImCurveEdit::CurveType GetCurveType(size_t) const override
        {
            switch (curve->GetType())
            {
            case Type::Linear: return ImCurveEdit::CurveLinear;
            case Type::Smooth: return ImCurveEdit::CurveSmooth;
            case Type::Step: return ImCurveEdit::CurveDiscrete;
            }
            return ImCurveEdit::CurveSmooth;
        }

        ImVec2& GetMin() override { return min; }
        ImVec2& GetMax() override { return max; }
        size_t GetPointCount(size_t) override { return curve->GetPointCount(); }
        uint32_t GetCurveColor(size_t) override { return color; }

        ImVec2* GetPoints(size_t) override
        {
            const auto& glmPoints = curve->GetPoints();
            pointsCache.resize(glmPoints.size());
            for (size_t i = 0; i < glmPoints.size(); ++i)
            {
                pointsCache[i] = ImVec2(glmPoints[i].x, glmPoints[i].y);
            }
            return pointsCache.data();
        }

        int EditPoint(size_t curveIndex, int pointIndex, ImVec2 value) override
        {
            if (curveIndex == 0 && pointIndex >= 0)
            {
                value.x = ImClamp(value.x, 0.0f, 1.0f);
                value.y = ImClamp(value.y, 0.0f, 1.0f);
                curve->EditPoint(pointIndex, glm::vec2(value.x, value.y));
            }
            return pointIndex;
        }

        void AddPoint(size_t curveIndex, ImVec2 value) override
        {
            if (curveIndex == 0)
            {
                value.x = ImClamp(value.x, 0.0f, 1.0f);
                value.y = ImClamp(value.y, 0.0f, 1.0f);
                curve->AddPoint(glm::vec2(value.x, value.y));
            }
        }

        void DeletePoint(size_t curveIndex, size_t pointIndex) override
        {
            if (curveIndex == 0)
            {
                curve->RemovePoint(pointIndex);
            }
        }


        void DrawPointLabels(const ImVec2& editorPos, const ImVec2& editorSize) const
        {
            if (!showPointValues) return;

            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            const auto& points = curve->GetPoints();

            const float rangeX = 1.0f;
            const float rangeY = 1.0f;

            for (size_t i = 0; i < points.size(); ++i)
            {

                ImVec2 normalized(points[i].x / rangeX, points[i].y / rangeY);
                ImVec2 screenPos(
                    editorPos.x + normalized.x * editorSize.x,
                    editorPos.y + editorSize.y - normalized.y * editorSize.y  
                );

                char label[64];
                snprintf(label, sizeof(label), "(%.2f, %.2f)", points[i].x, points[i].y);

                ImVec2 textSize = ImGui::CalcTextSize(label);
                ImVec2 labelPos(screenPos.x + 8, screenPos.y - textSize.y - 8);

                if (labelPos.x + textSize.x > editorPos.x + editorSize.x)
                    labelPos.x = screenPos.x - textSize.x - 8;
                if (labelPos.y < editorPos.y)
                    labelPos.y = screenPos.y + 8;

                draw_list->AddRectFilled(
                    ImVec2(labelPos.x - 2, labelPos.y - 2),
                    ImVec2(labelPos.x + textSize.x + 2, labelPos.y + textSize.y + 2),
                    IM_COL32(0, 0, 0, 200)
                );

                draw_list->AddText(labelPos, IM_COL32(255, 255, 255, 255), label);
            }
        }
    };
}