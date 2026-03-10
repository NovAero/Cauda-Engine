#pragma once

#include <vector>
#include <random>
#include <algorithm>

namespace RNG 
{
    class Random 
    {
    private:
        static std::mt19937 generator;
        static bool isInitialized;

    public:
        static void SetSeed(uint32_t seed) 
        {
            generator.seed(seed);
            isInitialized = true;
        }

        static void SetRandomSeed()
        {
            std::random_device rd;
            SetSeed(rd());
        }

        static std::mt19937& GetGenerator()
        {
            if (!isInitialized) 
            {
                SetRandomSeed();
            }
            return generator;
        }

        static float Float() 
        {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(GetGenerator());
        }

        static float Float(float min, float max) 
        {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(GetGenerator());
        }

        static int Int(int min, int max) 
        {
            std::uniform_int_distribution<int> dist(min, max);
            return dist(GetGenerator());
        }

        static bool Bool(float probability = 0.5f) 
        {
            return Float() < probability;
        }
    };

    std::mt19937 Random::generator;
    bool Random::isInitialized = false;

    template<typename T>
    class ProbabilityList 
    {
    private:
        struct Item 
        {
            T value;
            float weight;
            int units;        // -1 = unlimited, 0+ = limited quantity
            bool isEnabled;   

            Item(const T& val, float w, int u = -1)
                : value(val), weight(w), units(u), isEnabled(true) 
            {
            }
        };

        mutable std::vector<Item> items;
        mutable std::vector<float> cumulativeWeights;
        mutable bool needsUpdate;

        mutable T lastPicked;
        mutable bool hasLastPicked;
        bool repeatPreventionEnabled;

    public:
        ProbabilityList() : needsUpdate(true), repeatPreventionEnabled(false), hasLastPicked(false) {}

        void AddItem(const T& value, float weight = 1.0f, int units = -1)
        {
            items.emplace_back(value, weight, units);
            needsUpdate = true;
        }

        void SetWeight(const T& value, float weight) 
        {
            for (auto& item : items)
            {
                if (item.value == value) 
                {
                    item.weight = weight;
                    needsUpdate = true;
                    return;
                }
            }
        }

        float GetWeight(const T& value) const 
        {
            for (const auto& item : items)
            {
                if (item.value == value)
                {
                    return item.weight;
                }
            }
            return 0.0f;
        }

        void SetRepeatPrevention(bool enabled) 
        {
            repeatPreventionEnabled = enabled;
            if (!enabled) 
            {
                hasLastPicked = false;
            }
        }

        void SetItemEnabled(const T& value, bool enabled)
        {
            for (auto& item : items)
            {
                if (item.value == value) 
                {
                    item.isEnabled = enabled;
                    needsUpdate = true;
                    return;
                }
            }
        }

        const T& PickValue() const
        {
            if (needsUpdate) 
            {
                UpdateCumulativeWeights();
                needsUpdate = false;
            }

            if (repeatPreventionEnabled && hasLastPicked) 
            {
                const int maxAttempts = 50;
                for (int attempt = 0; attempt < maxAttempts; ++attempt) 
                {
                    const T& selected = SelectItem();

                    if (!(selected == lastPicked)) 
                    {
                        HandleItemSelection(selected);
                        return GetItemReference(selected);
                    }
                }
            }

            const T& selected = SelectItem();
            HandleItemSelection(selected);
            return GetItemReference(selected);
        }

        std::vector<T> PickValues(int count) const 
        {
            std::vector<T> results;
            results.reserve(count);

            ProbabilityList<T> tempList = *this;

            for (int i = 0; i < count && !tempList.Empty(); ++i)
            {
                const T& picked = tempList.PickValue();
                results.push_back(picked);

                tempList.RemoveItem(picked);
            }

            return results;
        }

        size_t Size() const { return items.size(); }
        bool Empty() const { return items.empty(); }

        void Clear() 
        {
            items.clear();
            cumulativeWeights.clear();
            hasLastPicked = false;
            needsUpdate = true;
        }

    private:
        const T& SelectItem() const 
        {
            float randomValue = Random::Float(0.0f, cumulativeWeights.back());

            for (size_t i = 0; i < cumulativeWeights.size(); ++i) {
                if (randomValue <= cumulativeWeights[i]) {
                    return items[i].value;
                }
            }

            return items.back().value;
        }

        void HandleItemSelection(const T& selected) const
        {
            if (repeatPreventionEnabled) 
            {
                lastPicked = selected;
                hasLastPicked = true;
            }

            for (auto& item : items) {
                if (item.value == selected && item.units > 0)
                {
                    item.units--;
                    if (item.units == 0) 
                    {
                        item.isEnabled = false; 
                        needsUpdate = true;
                    }
                    break;
                }
            }
        }

        const T& GetItemReference(const T& value) const
        {
            for (const auto& item : items) 
            {
                if (item.value == value) 
                {
                    return item.value;
                }
            }
            return items.back().value;
        }

        void RemoveItem(const T& value) 
        {
            items.erase(std::remove_if(items.begin(), items.end(),
                [&value](const Item& item) { return item.value == value; }), items.end());
            needsUpdate = true;
        }

        void UpdateCumulativeWeights() const
        {
            cumulativeWeights.clear();
            cumulativeWeights.reserve(items.size());

            float runningTotal = 0.0f;
            for (const auto& item : items)
            {
                if (item.isEnabled && item.units != 0)
                {
                    runningTotal += item.weight;
                }
                cumulativeWeights.push_back(runningTotal);
            }
        }
    };

    template<typename Container>
    const auto& RandomElement(const Container& container) 
    {
        auto it = container.begin();
        std::advance(it, Random::Int(0, static_cast<int>(container.size()) - 1));
        return *it;
    }

    template<typename Container>
    void Shuffle(Container& container)
    {
        std::shuffle(container.begin(), container.end(), Random::GetGenerator());
    }
}