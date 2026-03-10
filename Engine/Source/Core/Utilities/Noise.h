#pragma once

#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <cstring>
#include <glm/glm.hpp>
#include <FastNoise/FastNoise.h>
#include "Renderer/Texture.h"

#ifdef _WIN32
    #include <direct.h> 
#else
    #include <sys/stat.h>  
    #include <sys/types.h>
#endif

namespace Noise
{
    class Generator
    {
    private:
        static FastNoise::SmartNode<> s_perlinNode;
        static FastNoise::SmartNode<> s_worleyNode;
        static FastNoise::SmartNode<> s_perlinFBMNode;
        static FastNoise::SmartNode<> s_worleyFBMNode;
        static int s_currentSeed;

        static float Hash(glm::vec3 p, glm::vec3 period)
        {
            p = glm::mod(p, period);
            p = glm::fract(p * glm::vec3(0.1031f, 0.1030f, 0.0973f));
            p += glm::dot(p, glm::vec3(p.y, p.z, p.x) + 19.19f);
            return glm::fract((p.x + p.y) * p.z);
        }

        static glm::vec3 Hash3(glm::vec3 p, glm::vec3 period)
        {
            p = glm::mod(p, period);
            p = glm::fract(p * glm::vec3(0.1031f, 0.1030f, 0.0973f));
            p += glm::dot(p, glm::vec3(p.y, p.z, p.x) + 19.19f);
            return glm::fract(glm::vec3((p.x + p.y) * p.z, (p.y + p.z) * p.x, (p.z + p.x) * p.y));
        }

        static float PerlinPeriodic(glm::vec3 p, glm::vec3 period)
        {
            glm::vec3 i = glm::floor(p);
            glm::vec3 f = glm::fract(p);
            f = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

            float a = Hash(i, period);
            float b = Hash(i + glm::vec3(1, 0, 0), period);
            float c = Hash(i + glm::vec3(0, 1, 0), period);
            float d = Hash(i + glm::vec3(1, 1, 0), period);
            float e = Hash(i + glm::vec3(0, 0, 1), period);
            float f2 = Hash(i + glm::vec3(1, 0, 1), period);
            float g = Hash(i + glm::vec3(0, 1, 1), period);
            float h = Hash(i + glm::vec3(1, 1, 1), period);

            return glm::mix(
                glm::mix(glm::mix(a, b, f.x), glm::mix(c, d, f.x), f.y),
                glm::mix(glm::mix(e, f2, f.x), glm::mix(g, h, f.x), f.y),
                f.z
            );
        }

        static float WorleyPeriodic(glm::vec3 p, glm::vec3 period)
        {
            glm::vec3 cell = glm::floor(p);
            glm::vec3 localPos = glm::fract(p);
            float minDist = 1.0f;

            for (int z = -1; z <= 1; z++)
            {
                for (int y = -1; y <= 1; y++)
                {
                    for (int x = -1; x <= 1; x++)
                    {
                        glm::vec3 offset(x, y, z);
                        glm::vec3 neighborCell = cell + offset;
                        glm::vec3 point = Hash3(neighborCell, period);
                        glm::vec3 toPoint = offset + point - localPos;
                        float dist = glm::length(toPoint);
                        minDist = glm::min(minDist, dist);
                    }
                }
            }

            return minDist;
        }

        static float PerlinFBMPeriodic(glm::vec3 p, glm::vec3 period, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f)
        {
            float sum = 0.0f;
            float amplitude = 0.5f;
            float frequency = 1.0f;

            for (int i = 0; i < octaves; i++)
            {
                sum += amplitude * PerlinPeriodic(p * frequency, period * frequency);
                frequency *= lacunarity;
                amplitude *= gain;
            }

            return sum * 0.5f + 0.5f;
        }

        static float WorleyFBMPeriodic(glm::vec3 p, glm::vec3 period, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
        {
            float sum = 0.0f;
            float amplitude = 0.5f;
            float frequency = 1.0f;

            for (int i = 0; i < octaves; i++)
            {
                sum += amplitude * (1.0f - WorleyPeriodic(p * frequency, period * frequency));
                frequency *= lacunarity;
                amplitude *= gain;
            }

            return sum;
        }

        static float PerlinWorleyPeriodic(glm::vec3 p, glm::vec3 period)
        {
            float perlin = PerlinFBMPeriodic(p, period, 7, 2.0f, 0.6f);
            float worley = WorleyFBMPeriodic(p, period, 3, 2.0f, 0.6f);
            float perlinWorley = Remap(perlin, 0.0f, 1.0f, worley, 1.0f);
            return glm::clamp(perlinWorley, 0.0f, 1.0f);
        }

        static void InitializeNodes(int seed = 1337)
        {
            if (s_currentSeed != seed || !s_perlinNode)
            {
                s_currentSeed = seed;

                s_perlinNode = FastNoise::New<FastNoise::Perlin>();

                s_worleyNode = FastNoise::New<FastNoise::CellularDistance>();

                auto perlinFractal = FastNoise::New<FastNoise::FractalFBm>();
                perlinFractal->SetSource(FastNoise::New<FastNoise::Perlin>());
                perlinFractal->SetOctaveCount(4);
                perlinFractal->SetLacunarity(2.0f);
                perlinFractal->SetGain(0.5f);
                s_perlinFBMNode = perlinFractal;

                auto worleyFractal = FastNoise::New<FastNoise::FractalFBm>();
                worleyFractal->SetSource(FastNoise::New<FastNoise::CellularDistance>());
                worleyFractal->SetOctaveCount(3);
                worleyFractal->SetLacunarity(2.0f);
                worleyFractal->SetGain(0.5f);
                s_worleyFBMNode = worleyFractal;
            }
        }

    public:
        static void SetSeed(int seed)
        {
            InitializeNodes(seed);
        }


        static float Perlin3D(glm::vec3 p, glm::vec3 period)
        {
            return PerlinPeriodic(p, period);
        }

        static float Worley(glm::vec3 p, glm::vec3 period)
        {
            return WorleyPeriodic(p, period);
        }

        static float WorleyF2MinusF1(glm::vec3 p, glm::vec3 period)
        {
            glm::vec3 cell = glm::floor(p);
            glm::vec3 localPos = glm::fract(p);

            float minDist1 = 10.0f;
            float minDist2 = 10.0f;

            for (int z = -1; z <= 1; z++)
            {
                for (int y = -1; y <= 1; y++)
                {
                    for (int x = -1; x <= 1; x++)
                    {
                        glm::vec3 offset(x, y, z);
                        glm::vec3 neighborCell = cell + offset;
                        glm::vec3 point = Hash3(neighborCell, period);
                        glm::vec3 toPoint = offset + point - localPos;
                        float dist = glm::length(toPoint);

                        if (dist < minDist1)
                        {
                            minDist2 = minDist1;
                            minDist1 = dist;
                        }
                        else if (dist < minDist2)
                        {
                            minDist2 = dist;
                        }
                    }
                }
            }

            return minDist2 - minDist1;
        }

        static float PerlinFBM(glm::vec3 p, glm::vec3 period, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f)
        {
            return PerlinFBMPeriodic(p, period, octaves, lacunarity, gain);
        }

        static float WorleyFBM(glm::vec3 p, glm::vec3 period, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
        {
            return WorleyFBMPeriodic(p, period, octaves, lacunarity, gain);
        }

        static float PerlinWorley(glm::vec3 p, glm::vec3 period)
        {
            return PerlinWorleyPeriodic(p, period);
        }

        static float Perlin3D_Fast(glm::vec3 p)
        {
            InitializeNodes();
            float value = s_perlinNode->GenSingle3D(p.x, p.y, p.z, s_currentSeed);
            return value * 0.5f + 0.5f;
        }

        static float Worley_Fast(glm::vec3 p)
        {
            InitializeNodes();
            float value = s_worleyNode->GenSingle3D(p.x, p.y, p.z, s_currentSeed);
            return glm::clamp(value * 0.5f + 0.5f, 0.0f, 1.0f);
        }

        static float WorleyF2MinusF1_Fast(glm::vec3 p)
        {
            InitializeNodes();

            auto cellValue = FastNoise::New<FastNoise::CellularValue>();
            cellValue->SetValueIndex(1);

            float f2 = cellValue->GenSingle3D(p.x, p.y, p.z, s_currentSeed);

            cellValue->SetValueIndex(0);
            float f1 = cellValue->GenSingle3D(p.x, p.y, p.z, s_currentSeed);

            return glm::clamp((f2 - f1) * 0.5f + 0.5f, 0.0f, 1.0f);
        }

        static float PerlinFBM_Fast(glm::vec3 p, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f)
        {
            InitializeNodes();

            auto fractal = FastNoise::New<FastNoise::FractalFBm>();
            fractal->SetSource(FastNoise::New<FastNoise::Perlin>());
            fractal->SetOctaveCount(octaves);
            fractal->SetLacunarity(lacunarity);
            fractal->SetGain(gain);

            float value = fractal->GenSingle3D(p.x, p.y, p.z, s_currentSeed);
            return value * 0.5f + 0.5f;
        }

        static float WorleyFBM_Fast(glm::vec3 p, int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
        {
            InitializeNodes();

            auto fractal = FastNoise::New<FastNoise::FractalFBm>();
            fractal->SetSource(FastNoise::New<FastNoise::CellularDistance>());
            fractal->SetOctaveCount(octaves);
            fractal->SetLacunarity(lacunarity);
            fractal->SetGain(gain);

            float value = fractal->GenSingle3D(p.x, p.y, p.z, s_currentSeed);
            return glm::clamp(1.0f - (value * 0.5f + 0.5f), 0.0f, 1.0f);
        }

        static float PerlinWorley_Fast(glm::vec3 p)
        {
            float perlin = PerlinFBM_Fast(p, 7, 2.0f, 0.5f);
            float worley = WorleyFBM_Fast(p, 5, 2.0f, 0.5f);
            float perlinWorley = Remap(perlin, 0.0f, 1.0f, worley, 1.0f);
            return glm::clamp(perlinWorley, 0.0f, 1.0f);
        }

        static Texture* CreateCloudNoiseTexture3D(int size = 128, bool saveCache = false, const char* cachePath = "Cache/CloudNoise.fn2t")
        {
            std::vector<unsigned char> data(size * size * size * 4);

            glm::vec3 period = glm::vec3(4.0f);

            int progressStep = size / 10; 
            if (progressStep == 0) progressStep = 1;

            for (int z = 0; z < size; z++)
            {
                if (z % progressStep == 0)
                {
                    float progress = (float)z / (float)size * 100.0f;
                    std::cout << "  Generating... " << (int)progress << "%" << std::endl;
                }

                for (int y = 0; y < size; y++)
                {
                    for (int x = 0; x < size; x++)
                    {
                        glm::vec3 pos = glm::vec3(x, y, z) / (float)size;

                        float perlinWorley = PerlinWorley(pos * 4.0f, period);

                        float worley1 = Worley(pos * 4.0f, period);
                        float worley2 = Worley(pos * 8.0f, period);
                        float worley3 = Worley(pos * 16.0f, period);

                        int idx = (z * size * size + y * size + x) * 4;
                        data[idx + 0] = (unsigned char)(glm::clamp(perlinWorley, 0.0f, 1.0f) * 255.0f);
                        data[idx + 1] = (unsigned char)(glm::clamp(worley1, 0.0f, 1.0f) * 255.0f);
                        data[idx + 2] = (unsigned char)(glm::clamp(worley2, 0.0f, 1.0f) * 255.0f);
                        data[idx + 3] = (unsigned char)(glm::clamp(worley3, 0.0f, 1.0f) * 255.0f);
                    }
                }
            }

            if (saveCache)
            {
                SaveTexture3DCache(data, size, cachePath);
            }

            Texture* tex = new Texture();
            tex->CreateTexture3D(size, size, size, data.data(), Texture::RGBA, Texture::UNORM8);

            return tex;
        }

        static Texture* CreateCloudNoiseTexture3D_Fast(int size = 128, bool saveCache = false, const char* cachePath = "Cache/CloudNoise_Fast.fn2t")
        {
            InitializeNodes();

            std::vector<unsigned char> data(size * size * size * 4);

            auto perlinGen = FastNoise::New<FastNoise::Perlin>();

            auto perlinFBM = FastNoise::New<FastNoise::FractalFBm>();
            perlinFBM->SetSource(perlinGen);
            perlinFBM->SetOctaveCount(7);   
            perlinFBM->SetLacunarity(2.0f);   
            perlinFBM->SetGain(0.6f);         

            auto worleyGen = FastNoise::New<FastNoise::CellularDistance>();

            auto worleyFBM = FastNoise::New<FastNoise::FractalFBm>();
            worleyFBM->SetSource(worleyGen);
            worleyFBM->SetOctaveCount(3);   
            worleyFBM->SetLacunarity(2.0f);  
            worleyFBM->SetGain(0.6f);        

            const float frequency = 4.0f;    

            int progressStep = size / 10;
            if (progressStep == 0) progressStep = 1;

            for (int z = 0; z < size; z++)
            {
                if (z % progressStep == 0)
                {
                    float progress = (float)z / (float)size * 100.0f;
                    std::cout << "  Generating... " << (int)progress << "%" << std::endl;
                }

                for (int y = 0; y < size; y++)
                {
                    for (int x = 0; x < size; x++)
                    {
                        glm::vec3 pos = glm::vec3(x, y, z) / (float)size;
                        glm::vec3 p = pos * frequency;

                        float perlin = perlinFBM->GenSingle3D(p.x, p.y, p.z, s_currentSeed) * 0.5f + 0.5f;
                        float worley_base = worleyFBM->GenSingle3D(p.x, p.y, p.z, s_currentSeed) * 0.5f + 0.5f;


                        float perlinWorley = Remap(perlin, 0.0f, 1.0f, worley_base, 1.0f);

                        float worley1 = worleyGen->GenSingle3D(p.x, p.y, p.z, s_currentSeed) * 0.5f + 0.5f;
                        float worley2 = worleyGen->GenSingle3D(p.x * 2.0f, p.y * 2.0f, p.z * 2.0f, s_currentSeed) * 0.5f + 0.5f;
                        float worley3 = worleyGen->GenSingle3D(p.x * 4.0f, p.y * 4.0f, p.z * 4.0f, s_currentSeed) * 0.5f + 0.5f;

                        int idx = (z * size * size + y * size + x) * 4;
                        data[idx + 0] = (unsigned char)(glm::clamp(perlinWorley, 0.0f, 1.0f) * 255.0f);
                        data[idx + 1] = (unsigned char)(glm::clamp(worley1, 0.0f, 1.0f) * 255.0f);
                        data[idx + 2] = (unsigned char)(glm::clamp(worley2, 0.0f, 1.0f) * 255.0f);
                        data[idx + 3] = (unsigned char)(glm::clamp(worley3, 0.0f, 1.0f) * 255.0f);
                    }
                }
            }

            if (saveCache)
            {
                SaveTexture3DCache(data, size, cachePath);
            }

            Texture* tex = new Texture();
            tex->CreateTexture3D(size, size, size, data.data(), Texture::RGBA, Texture::UNORM8);

            return tex;
        }

        static FastNoise::OutputMinMax GenerateNoiseSet2D(std::vector<float>& output, int width, int height,
            FastNoise::SmartNode<> generator, float offsetX = 0.0f, float offsetY = 0.0f, float frequency = 0.01f)
        {
            output.resize(width * height);
            return generator->GenUniformGrid2D(output.data(),
                static_cast<int>(offsetX), static_cast<int>(offsetY),
                width, height, frequency, s_currentSeed);
        }

        static FastNoise::OutputMinMax GenerateNoiseSet3D(std::vector<float>& output, int width, int height, int depth,
            FastNoise::SmartNode<> generator, float offsetX = 0.0f, float offsetY = 0.0f, float offsetZ = 0.0f, float frequency = 0.01f)
        {
            output.resize(width * height * depth);
            return generator->GenUniformGrid3D(output.data(),
                static_cast<int>(offsetX), static_cast<int>(offsetY), static_cast<int>(offsetZ),
                width, height, depth, frequency, s_currentSeed);
        }

        static FastNoise::SmartNode<> CreatePerlinGenerator(int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f)
        {
            auto fractal = FastNoise::New<FastNoise::FractalFBm>();
            fractal->SetSource(FastNoise::New<FastNoise::Perlin>());
            fractal->SetOctaveCount(octaves);
            fractal->SetLacunarity(lacunarity);
            fractal->SetGain(gain);
            return fractal;
        }

        static FastNoise::SmartNode<> CreateWorleyGenerator(int octaves = 3, float lacunarity = 2.0f, float gain = 0.5f)
        {
            auto fractal = FastNoise::New<FastNoise::FractalFBm>();
            fractal->SetSource(FastNoise::New<FastNoise::CellularDistance>());
            fractal->SetOctaveCount(octaves);
            fractal->SetLacunarity(lacunarity);
            fractal->SetGain(gain);
            return fractal;
        }

        static FastNoise::SmartNode<> CreateSimplexGenerator()
        {
            auto simplex = FastNoise::New<FastNoise::Simplex>();
            return simplex;
        }

        static FastNoise::SmartNode<> CreateOpenSimplex2Generator()
        {
            auto openSimplex = FastNoise::New<FastNoise::OpenSimplex2>();
            return openSimplex;
        }

        static float Remap(float value, float inMin, float inMax, float outMin, float outMax)
        {
            return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
        }

        static float SmoothStep(float edge0, float edge1, float x)
        {
            float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        static bool SaveTexture3DCache(const std::vector<unsigned char>& data, int size, const char* filepath)
        {
            std::string path(filepath);
            size_t lastSlash = path.find_last_of("/\\");
            if (lastSlash != std::string::npos)
            {
                std::string directory = path.substr(0, lastSlash);
                #ifdef _WIN32
                    _mkdir(directory.c_str());
                #else
                    mkdir(directory.c_str(), 0755);
                #endif
            }

            std::ofstream file(filepath, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to save 3D texture cache to: " << filepath << std::endl;
                return false;
            }

            file.write("FN2T", 4);
            file.write(reinterpret_cast<const char*>(&size), sizeof(int));

            size_t dataSize = data.size();
            file.write(reinterpret_cast<const char*>(&dataSize), sizeof(size_t));
            file.write(reinterpret_cast<const char*>(data.data()), dataSize);

            file.close();
            std::cout << "Saved 3D texture cache (" << size << "^3) to: " << filepath << std::endl;
            return true;
        }

        static bool LoadTexture3DCache(std::vector<unsigned char>& data, int& size, const char* filepath)
        {
            std::ifstream file(filepath, std::ios::binary);
            if (!file.is_open())
            {
                std::cout << "Cache file not found: " << filepath << std::endl;
                return false;
            }

            char magic[4];
            file.read(magic, 4);
            if (std::memcmp(magic, "FN2T", 4) != 0)
            {
                std::cerr << "Invalid cache file format: " << filepath << std::endl;
                file.close();
                return false;
            }

            file.read(reinterpret_cast<char*>(&size), sizeof(int));

            size_t dataSize;
            file.read(reinterpret_cast<char*>(&dataSize), sizeof(size_t));
            data.resize(dataSize);
            file.read(reinterpret_cast<char*>(data.data()), dataSize);

            file.close();
            std::cout << "Loaded 3D texture cache (" << size << "^3) from: " << filepath << std::endl;
            return true;
        }

        static Texture* CreateOrLoadCloudNoiseTexture3D(int size = 128, const char* cachePath = "Cache/CloudNoise.fn2t")
        {
            std::vector<unsigned char> data;
            int loadedSize = 0;

            std::cout << "Attempting to load cloud noise texture (" << size << "^3)..." << std::endl;

            if (LoadTexture3DCache(data, loadedSize, cachePath) && loadedSize == size)
            {
                std::cout << "Successfully loaded from cache!" << std::endl;
                Texture* tex = new Texture();
                tex->CreateTexture3D(size, size, size, data.data(), Texture::RGBA, Texture::UNORM8);
                return tex;
            }

            if (loadedSize != 0 && loadedSize != size)
            {
                std::cout << "Cache size mismatch: cached=" << loadedSize << ", requested=" << size << std::endl;
            }

            std::cout << "Generating new cloud noise texture (" << size << "^3)... This may take a while." << std::endl;

            Texture* tex = CreateCloudNoiseTexture3D(size, true, cachePath);
            return tex;
        }
    };

    inline FastNoise::SmartNode<> Generator::s_perlinNode = nullptr;
    inline FastNoise::SmartNode<> Generator::s_worleyNode = nullptr;
    inline FastNoise::SmartNode<> Generator::s_perlinFBMNode = nullptr;
    inline FastNoise::SmartNode<> Generator::s_worleyFBMNode = nullptr;
    inline int Generator::s_currentSeed = 1337;
}
