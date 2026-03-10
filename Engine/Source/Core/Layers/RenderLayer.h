#pragma once

#include "common/ubos.h"
#include "common/postprocess_ubos.h"

#include "glad/glad.h"
#include <chrono>


class RendererModule;
class AnimationModule;
class InteractorModule;
class CameraModule;
class LightingModule;
class PhysicsModule;
class RopeModule;
class CaribbeanModule;
class UniformBuffer;
class ShaderStorageBuffer;
class FrameBuffer;
class Mesh;
class Texture;
class PostProcessComponent;


namespace Cauda
{

    enum class BufferVisualisation
    {
        Final = 0,

        GBuffer_Albedo,
        GBuffer_Normal,
        GBuffer_Position,
        GBuffer_Material,
        GBuffer_EntityID,
        GBuffer_Depth,
        GBuffer_Stencil,

        Backface_Position,

        Lighting_Colour,
        Lighting_Data,
        AO,
        AO_Blurred,
        Shadow,
        PostProcess,

        COUNT
    };

    struct BufferVisualisationInfo
    {
        int visualiseChannel = -1; // -1 = all, 0 = R, 1 = G, 2 = B, 3 = A
        float depthVisualisationScale = 1.0f;
        float positionVisualisationScale = 0.1f;
        bool normaliseVisualisation = false;
        bool colouriseEntityIDs = true;
    };

    class RenderLayer //: public Layer
    {
    public:
        RenderLayer(flecs::world& world);
        ~RenderLayer();

        void RenderPipeline();
        void ResizeFrameBuffers();

        virtual void OnAttach();
        virtual void OnDetach();
        //virtual void OnUpdate(float delta) override {}
        //virtual void OnImGuiRender() override {}
        //virtual void OnEvent(SDL_Event& event) override {}

        const FrameBuffer* GetViewportBuffer() const { return m_postProcessBuffer; }
        bool IsGameView() const { return m_isGameView; }
        void SetGameView(bool gameView) { m_isGameView = gameView; }

        flecs::entity PickEntity(int mouseX, int mouseY);
        const Texture* GetDebugTexture();

        BufferVisualisation GetDebugVisualisation() const { return m_debugVisualisation; }
        void SetDebugVisualisation(BufferVisualisation vis) { m_debugVisualisation = vis; }

        ImVec2 GetBufferSize(BufferVisualisation vis);
        const char* GetBufferFormat(BufferVisualisation vis);
        void SetWireframeMode(bool wireframe) { m_wireframeMode = wireframe; }
        bool IsWireframeMode() const { return m_wireframeMode; }

        BufferVisualisationInfo m_bufferVisInfo;
    private:
        void SetupSystems();
        void SetupObservers();

        void SetupUniformBuffers();
        void SetupShaderStorageBuffers();
        void SetupFrameBuffers();

        void SetupShaders();
        void SetupComputeShaders();
        void SetupTextures();
        void SetupMaterials();
        void SetupMeshes();

        void SetupAO();
        void SetupStencilView();

        void UpdateCameraUBO(flecs::entity camera);
        void UpdateLightingUBO();
        void UpdateTimeUBO(float deltaTime);
        void UpdatePostProcessUBO(const PostProcessComponent& postProcess);

        void RenderShadowPass();
        void RenderGeometryPass();
        void RenderBackfacePass();
        void RenderXrayPass();
        void RenderAOPass();
        void RenderBlurPass();
        void RenderLightingPass();
        void RenderBloomPass();
        void RenderDirectionalLight();
        void RenderPointLights();

        void RenderSpotLights();

        void RenderCloudPass();

        void RenderPostProcessPass();
        void RenderFinalPass();
        void RenderDebugBuffer();
        //void OnWindowResize(int width, int height) {}


        flecs::world& m_world;
        RendererModule* m_rendererModule = nullptr;
        AnimationModule* m_animationModule = nullptr;
        LightingModule* m_lightingModule = nullptr;
        CameraModule* m_cameraModule = nullptr;
        PhysicsModule* m_physicsModule = nullptr;
        RopeModule* m_ropeModule = nullptr;
        CaribbeanModule* m_caribbeanModule = nullptr;
        InteractorModule* m_interactorModule = nullptr;

        flecs::system m_updateUBOsSystem;
        flecs::observer m_postProcessUpdateObserver;
        flecs::observer m_postProcessRemoveObserver;

        std::unique_ptr<Mesh> m_screenTarget = nullptr;

        UniformBuffer* m_cameraUBO = nullptr;
        UniformBuffer* m_lightingUBO = nullptr;
        UniformBuffer* m_timeUBO = nullptr;
        UniformBuffer* m_postProcessUBO = nullptr;

        CameraUBO m_cameraData;
        LightingUBO m_lightingData;
        TimeUBO m_timeData;
        PostProcessUBO m_postProcessData;

        std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;

        FrameBuffer* m_gBuffer = nullptr;
        FrameBuffer* m_backfaceBuffer = nullptr;
        FrameBuffer* m_lightingBuffer = nullptr;
        FrameBuffer* m_postProcessBuffer = nullptr;
        FrameBuffer* m_shadowBuffer = nullptr;
        FrameBuffer* m_aoBuffer = nullptr;
        FrameBuffer* m_blurBuffer = nullptr;
        FrameBuffer* m_debugBuffer = nullptr;

        FrameBuffer* m_bloomExtractBuffer;          
        std::vector<FrameBuffer*> m_bloomMipChain;  

        float m_bloomIntensity = 0.5f;
        float m_bloomThreshold = 1.5f;
        float m_bloomKnee = 0.5f;
        int m_bloomMipLevels = 5;                   
                 
        FrameBuffer* m_cloudBuffer = nullptr;
        FrameBuffer* m_cloudBufferUpsampled = nullptr;
        int m_cloudResolutionDivisor = 1;
        Texture* m_cloudNoiseTexture = nullptr;

        GLuint m_stencilView = 0;

        bool m_isGameView = false;
        bool m_initialised = false;
        bool m_wireframeMode = false;

        BufferVisualisation m_debugVisualisation = BufferVisualisation::Final;

        int m_lightingLevels = 4;

        std::vector<glm::vec3> m_aoKernel;
        int m_aoKernelSize = 64;
        float m_aoRadius = 2.6f;
        float m_aoBias = 0.025f;
        float m_aoPower = 1.0f;

        static constexpr unsigned int SHADOW_WIDTH = 4096;
        static constexpr unsigned int SHADOW_HEIGHT = 4096;
    };

}