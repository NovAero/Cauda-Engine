#include "cepch.h"
#include "RenderLayer.h"
#include "Platform/Win32/Application.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/ShaderStorageBuffer.h"
#include "Renderer/FrameBuffer.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Math/TransformModule.h"
#include "Core/Scene/CameraModule.h"
#include "Core/Scene/LightingModule.h"
#include "Renderer/RendererModule.h"
#include "Renderer/InteractorModule.h"
#include "Core/Physics/RopeModule.h"
#include "Renderer/CaribbeanModule.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Utilities/Noise.h"


namespace Cauda
{
    RenderLayer::RenderLayer(flecs::world& world) : //Layer("Render Layer"),
        m_world(world)
    {
        m_cameraModule = m_world.try_get_mut<CameraModule>();
        m_rendererModule = m_world.try_get_mut<RendererModule>();
        m_animationModule = m_world.try_get_mut<AnimationModule>();
        m_lightingModule = m_world.try_get_mut<LightingModule>();
        m_physicsModule = m_world.try_get_mut<PhysicsModule>();
        m_ropeModule = m_world.try_get_mut<RopeModule>();
        m_caribbeanModule = m_world.try_get_mut<CaribbeanModule>();
        m_interactorModule = m_world.try_get_mut<InteractorModule>();

        m_startTime = std::chrono::high_resolution_clock::now();
    }

    RenderLayer::~RenderLayer()
    {
        delete m_cameraUBO;
        delete m_lightingUBO;
        delete m_timeUBO;
        delete m_postProcessUBO;

        delete m_gBuffer;
        delete m_backfaceBuffer;
        delete m_lightingBuffer;
        delete m_postProcessBuffer;
        delete m_shadowBuffer;
        delete m_aoBuffer;
        delete m_blurBuffer;
        delete m_debugBuffer;
        delete m_bloomExtractBuffer;
        delete m_cloudBuffer;
        delete m_cloudBufferUpsampled;

        delete m_cloudNoiseTexture;

        for (auto* buffer : m_bloomMipChain)
        {
            delete buffer;
        }
        m_bloomMipChain.clear();

        if (m_stencilView != 0)
        {
            glDeleteTextures(1, &m_stencilView);
        }
    }

    void RenderLayer::RenderPipeline()
    {
        if (!m_initialised) return;

        Application& app = Application::Get();

        auto camera = m_cameraModule->GetMainCamera();
        auto camComp = camera.try_get_mut<CameraComponent>();

        if (app.HasGameViewportResized() || camComp->isResolutionDirty)
        {
            ResizeFrameBuffers();
            camComp->isResolutionDirty = false;
        }

        // RENDER PIPELINE
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (m_lightingModule->GetMainLightComponent()->castShadows)
                RenderShadowPass();

            RenderGeometryPass();
            // RenderBackfacePass();
            RenderAOPass();
            RenderBlurPass();
            RenderLightingPass();
            RenderXrayPass();
            RenderBloomPass();
            RenderCloudPass();
            RenderPostProcessPass();
            RenderFinalPass();
        }
    }

    void RenderLayer::ResizeFrameBuffers()
    {
        Application& app = Application::Get();
        const glm::vec2 screenSize = app.GetScreenSize();
        const glm::vec2 viewportSize = app.GetGameViewportSize();

        auto camera = m_cameraModule->GetMainCamera();
        auto camComp = camera.try_get_mut<CameraComponent>();
        camComp->resolution = viewportSize;
        float pixScale = camComp->pixelScale > 0.0f ? camComp->pixelScale : 1.0f;
        //  pixScale *= std::min(screenSize.x / viewportSize.x, screenSize.y / viewportSize.y);
         // glm::vec2 resolution = viewportSize / pixScale;
        glm::vec2 resolution = camComp->resolution / pixScale;

        unsigned int width = (unsigned int)resolution.x;
        unsigned int height = (unsigned int)resolution.y;

        m_gBuffer->Resize(width, height);
        m_backfaceBuffer->Resize(width, height);
        m_aoBuffer->Resize(width, height);
        m_blurBuffer->Resize(width, height);
        m_lightingBuffer->Resize(width, height);
        m_postProcessBuffer->Resize(width, height);
        m_debugBuffer->Resize(width, height);

        int bloomWidth = width / 2;
        int bloomHeight = height / 2;

        m_bloomExtractBuffer->Resize(bloomWidth, bloomHeight);

        int cloudWidth = width / m_cloudResolutionDivisor;
        int cloudHeight = height / m_cloudResolutionDivisor;
        m_cloudBuffer->Resize(cloudWidth, cloudHeight);

        m_cloudBufferUpsampled->Resize(width, height);

        for (int i = 0; i < m_bloomMipChain.size(); ++i)
        {
            int mipWidth = bloomWidth >> i;
            int mipHeight = bloomHeight >> i;
            m_bloomMipChain[i]->Resize(mipWidth, mipHeight);
        }
    }

    void RenderLayer::OnAttach()
    {
        SetupSystems();
        SetupObservers();

        SetupUniformBuffers();
        SetupShaderStorageBuffers();
        SetupFrameBuffers();

        SetupComputeShaders();
        SetupShaders();
        SetupTextures();
        SetupMaterials();
        SetupMeshes();
        SetupAO();
        SetupStencilView();
        m_initialised = true;
    }

    void RenderLayer::OnDetach()
    {

    }

    void RenderLayer::SetupSystems()
    {
        m_updateUBOsSystem = m_world.system("UpdateUBOs")
            .kind(flecs::PreStore)
            .run([&](flecs::iter& it)
                {
                    float deltaTime = it.delta_system_time();
                    flecs::entity camera = m_cameraModule->GetMainCamera();

                    UpdateCameraUBO(camera);
                    UpdateLightingUBO();
                    UpdateTimeUBO(deltaTime);

                });
    }

    void RenderLayer::SetupObservers()
    {
        // Doesn't update for child postprocess right now.

        m_postProcessUpdateObserver = m_world.observer<PostProcessComponent>()
            .event(flecs::OnSet)
            .with<CameraComponent>().filter()
            .with<MainCamera>().filter()
            .each([this](flecs::entity entity, PostProcessComponent& postProcess)
                {
                    UpdatePostProcessUBO(postProcess);
                });

        m_postProcessRemoveObserver = m_world.observer<PostProcessComponent>()
            .event(flecs::OnRemove)
            .event(flecs::OnDelete)
            .each([this](flecs::entity entity, PostProcessComponent& postProcess)
                {
                    UpdatePostProcessUBO(postProcess);
                });
    }

    void RenderLayer::SetupUniformBuffers()
    {
        m_cameraUBO = new UniformBuffer();
        m_cameraUBO->Bind();
        m_cameraUBO->BufferData(sizeof(CameraUBO));
        m_cameraUBO->BindBufferBase(CAMERA_UBO_BINDING);
        m_cameraUBO->Unbind();

        m_lightingUBO = new UniformBuffer();
        m_lightingUBO->Bind();
        m_lightingUBO->BufferData(sizeof(LightingUBO));
        m_lightingUBO->BindBufferBase(LIGHTING_UBO_BINDING);
        m_lightingUBO->Unbind();

        m_timeUBO = new UniformBuffer();
        m_timeUBO->Bind();
        m_timeUBO->BufferData(sizeof(TimeUBO));
        m_timeUBO->BindBufferBase(TIME_UBO_BINDING);
        m_timeUBO->Unbind();

        m_postProcessUBO = new UniformBuffer();
        m_postProcessUBO->Bind();
        m_postProcessUBO->BufferData(sizeof(PostProcessUBO));
        m_postProcessUBO->BindBufferBase(POSTPROCESS_UBO_BINDING);
        m_postProcessUBO->Unbind();
    }

    void RenderLayer::SetupShaderStorageBuffers()
    {
    }

    void RenderLayer::SetupFrameBuffers()
    {
        Application& app = Application::Get();
        const glm::vec2 screenSize = app.GetScreenSize();
        //const glm::vec2 viewportSize = app.GetGameViewportSize();

        auto camera = m_world.entity("MainCamera");

        camera.add<TransformComponent>();
        CameraComponent& camComp = camera.ensure<CameraComponent>();
        camComp.resolution = screenSize;

        Logger::PrintLog("Camera resolution set to: %.0fx%.0f", screenSize.x, screenSize.y);

        m_cameraModule->SetMainCamera(camera);

        float pixScale = camComp.pixelScale > 0.0f ? camComp.pixelScale : 1.0f;
        // pixScale *= std::min(screenSize.x / viewportSize.x, screenSize.y / viewportSize.y);
        glm::vec2 resolution = camComp.resolution / pixScale;

        m_gBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_gBuffer->AddColourAttachment(Texture::RGBA, Texture::UNORM8);
        m_gBuffer->AddFloatColourAttachment(Texture::RGBA);
        m_gBuffer->AddFloatColourAttachment(Texture::RGBA);
        m_gBuffer->AddColourAttachment(Texture::RGBA, Texture::UNORM8);
        m_gBuffer->AddColourAttachment(Texture::RG, Texture::UINT32);
        m_gBuffer->AddDepthStencilAttachment();

        m_backfaceBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_backfaceBuffer->AddFloatColourAttachment(Texture::RGBA);
        m_backfaceBuffer->AddDepthStencilAttachment();

        m_lightingBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_lightingBuffer->AddFloatColourAttachment(Texture::RGBA);
        m_lightingBuffer->AddFloatColourAttachment(Texture::RGBA);
        m_lightingBuffer->AddDepthStencilAttachment();

        m_postProcessBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_postProcessBuffer->AddFloatColourAttachment(Texture::RGBA);

        int cloudWidth = (int)resolution.x / m_cloudResolutionDivisor;
        int cloudHeight = (int)resolution.y / m_cloudResolutionDivisor;

        m_cloudBuffer = new FrameBuffer(cloudWidth, cloudHeight);
        m_cloudBuffer->AddFloatColourAttachment(Texture::RGBA, FrameBuffer::FilterMode::LINEAR);

        m_cloudBufferUpsampled = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_cloudBufferUpsampled->AddFloatColourAttachment(Texture::RGBA, FrameBuffer::FilterMode::LINEAR);

        m_shadowBuffer = new FrameBuffer(SHADOW_WIDTH, SHADOW_HEIGHT);
        m_shadowBuffer->AddDepthAttachment(true);

        m_aoBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_aoBuffer->AddFloatColourAttachment(Texture::RED);

        m_blurBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_blurBuffer->AddFloatColourAttachment(Texture::RED);

        m_debugBuffer = new FrameBuffer((int)resolution.x, (int)resolution.y);
        m_debugBuffer->AddFloatColourAttachment(Texture::RGBA);

        int bloomWidth = (int)resolution.x / 2;
        int bloomHeight = (int)resolution.y / 2;

        m_bloomExtractBuffer = new FrameBuffer(bloomWidth, bloomHeight);
        m_bloomExtractBuffer->AddFloatColourAttachment(Texture::RGB, FrameBuffer::FilterMode::LINEAR);

        m_bloomMipChain.clear();
        for (int i = 0; i < m_bloomMipLevels; ++i)
        {
            int mipWidth = bloomWidth >> i;
            int mipHeight = bloomHeight >> i;

            auto* mipBuffer = new FrameBuffer(mipWidth, mipHeight);
            mipBuffer->AddFloatColourAttachment(Texture::RGB, FrameBuffer::FilterMode::LINEAR);
            m_bloomMipChain.push_back(mipBuffer);
        }

        m_screenTarget = std::make_unique<Mesh>();
        m_screenTarget->CreateQuad();
    }

    void RenderLayer::UpdateCameraUBO(flecs::entity camera)
    {
        auto camComp = camera.try_get<CameraComponent>();
        auto transform = camera.try_get<TransformComponent>();

        if (!camComp || !transform) return;

        Application& app = Application::Get();
        const glm::vec2 screenSize = app.GetScreenSize();
        // const glm::vec2 resolution = app.GetGameViewportSize();
        const glm::vec2 resolution = camComp->resolution;

        m_cameraData.view = m_cameraModule->GetViewMatrix(camera);
        m_cameraData.projection = m_cameraModule->GetProjectionMatrix(camera);
        m_cameraData.viewProjection = m_cameraData.projection * m_cameraData.view;
        m_cameraData.position = transform->position;
        m_cameraData.forward = m_cameraModule->GetForwardVector(camera);
        m_cameraData.nearPlane = camComp->nearPlane;
        m_cameraData.farPlane = camComp->farPlane;
        m_cameraData.resolution = camComp->resolution;
        m_cameraData.orthoSize = camComp->orthoSize;
        m_cameraData.isOrthographic = (m_cameraModule->GetProjectionMode(camera) == ProjectionMode::ORTHOGRAPHIC) ? 1 : 0;
        m_cameraData.snapObjects = camComp->snapObjects ? 1 : 0;
        m_cameraData.snapSpaceMatrix = camComp->snapSpace;

        float pixScale = camComp->pixelScale > 0.0f ? camComp->pixelScale : 1.0f;
        // pixScale *= std::min(screenSize.x / resolution.x, screenSize.y / resolution.y);
        m_cameraData.texelSize = (2.0f * camComp->orthoSize) / (resolution.y / pixScale);
        m_cameraData.pixelScale = pixScale;

        m_cameraUBO->Bind();
        m_cameraUBO->BufferSubData(0, sizeof(CameraUBO), &m_cameraData);
        m_cameraUBO->Unbind();
    }

    void RenderLayer::UpdateLightingUBO()
    {
        auto mainLight = m_lightingModule->GetMainLight();
        auto* mainLightComp = m_lightingModule->GetMainLightComponent();

        if (!mainLightComp) return;

        m_lightingData.lightSpaceMatrix = m_lightingModule->CalculateLightSpaceMatrix();
        m_lightingData.lightDirection = m_lightingModule->GetMainLightDirection();
        m_lightingData.lightColour = mainLightComp->colour;
        m_lightingData.lightIntensity = mainLightComp->intensity;
        m_lightingData.shadowBias = mainLightComp->shadowBias;
        m_lightingData.castShadows = mainLightComp->castShadows ? 1 : 0;
        m_lightingData.ambientColour = m_lightingModule->GetCombinedAmbientLight();
        m_lightingData.shadowMapSize = glm::vec2(SHADOW_WIDTH, SHADOW_HEIGHT);
        m_lightingData.lightingLevels = m_lightingLevels;

        m_lightingData.numPointLights = static_cast<int>(m_lightingModule->GetActivePointLights().size());
        m_lightingData.numSpotLights = static_cast<int>(m_lightingModule->GetActiveSpotLights().size());

        m_lightingUBO->Bind();
        m_lightingUBO->BufferSubData(0, sizeof(LightingUBO), &m_lightingData);
        m_lightingUBO->Unbind();
    }

    void RenderLayer::UpdateTimeUBO(float deltaTime)
    {
        static int frameCount = 0;
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = currentTime - m_startTime;

        m_timeData.time = std::chrono::duration_cast<std::chrono::duration<float>>(duration).count();
        m_timeData.deltaTime = deltaTime;
        m_timeData.frameCount = frameCount++;
        m_timeData.sinTime = std::sin(m_timeData.time);

        m_timeUBO->Bind();
        m_timeUBO->BufferSubData(0, sizeof(TimeUBO), &m_timeData);
        m_timeUBO->Unbind();
    }

    void RenderLayer::UpdatePostProcessUBO(const PostProcessComponent& postProcess)
    {
        m_postProcessData.enableFog = postProcess.enableFog;
        m_postProcessData.fogExtinction = postProcess.fogExtinction;
        m_postProcessData.fogInscatter = postProcess.fogInscatter;
        m_postProcessData.fogBaseColour = postProcess.fogBaseColour;
        m_postProcessData.fogSunColour = postProcess.fogSunColour;
        m_postProcessData.fogSunPower = postProcess.fogSunPower;

        m_postProcessData.fogHeight = postProcess.fogHeight;
        m_postProcessData.fogHeightFalloff = postProcess.fogHeightFalloff;
        m_postProcessData.fogHeightDensity = postProcess.fogHeightDensity;

        m_postProcessData.colourFactor = postProcess.colourFactor;
        m_postProcessData.brightnessFactor = postProcess.brightnessFactor;
        m_postProcessData.enableHDR = postProcess.enableHDR;
        m_postProcessData.exposure = postProcess.exposure;

        m_postProcessData.enableHSV = postProcess.enableHSV;
        m_postProcessData.hueShift = postProcess.hueShift;
        m_postProcessData.saturationAdjust = postProcess.saturationAdjust;
        m_postProcessData.valueAdjust = postProcess.valueAdjust;

        m_postProcessData.enableEdgeDetection = postProcess.enableEdgeDetection;
        m_postProcessData.showEdgesOnly = postProcess.showEdgesOnly;
        m_postProcessData.edgeStrength = postProcess.edgeStrength;
        m_postProcessData.depthThreshold = postProcess.depthThreshold;
        m_postProcessData.normalThreshold = postProcess.normalThreshold;
        m_postProcessData.darkenAmount = postProcess.darkenAmount;
        m_postProcessData.lightenAmount = postProcess.lightenAmount;
        m_postProcessData.normalBias = postProcess.normalBias;
        m_postProcessData.edgeWidth = postProcess.edgeWidth;

        m_postProcessData.enableQuantization = postProcess.enableQuantization;
        m_postProcessData.quantizationLevels = postProcess.quantizationLevels;
        m_postProcessData.enableDithering = postProcess.enableDithering;
        m_postProcessData.ditherStrength = postProcess.ditherStrength;

        m_postProcessUBO->Bind();
        m_postProcessUBO->BufferSubData(0, sizeof(PostProcessUBO), &m_postProcessData);
        m_postProcessUBO->Unbind();
    }

    void RenderLayer::SetupShaders()
    {
        ResourceLibrary::LoadShader("mesh_geometry", "shaders/mesh_geometry.vert", "shaders/mesh_geometry.frag");
        ResourceLibrary::LoadShader("foliage_geometry", "shaders/foliage_geometry.vert", "shaders/mesh_geometry.frag");
        ResourceLibrary::LoadShader("parallax_geometry", "shaders/parallax_geometry.vert", "shaders/mesh_geometry.frag");
        ResourceLibrary::LoadShader("mesh_lighting", "shaders/frame_buffer.vert", "shaders/mesh_lighting.frag");
        ResourceLibrary::LoadShader("post_process", "shaders/frame_buffer.vert", "shaders/post_process.frag");
        ResourceLibrary::LoadShader("final_buffer", "shaders/frame_buffer.vert", "shaders/final_buffer.frag");
        ResourceLibrary::LoadShader("shadow_depth", "shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
        ResourceLibrary::LoadShader("skeletal_shadow_depth", "shaders/skeletal_shadow_depth.vert", "shaders/skeletal_shadow_depth.frag");
        ResourceLibrary::LoadShader("ambient_occlusion", "shaders/frame_buffer.vert", "shaders/ambient_occlusion.frag");
        ResourceLibrary::LoadShader("spiral_blur", "shaders/frame_buffer.vert", "shaders/spiral_blur.frag");
        ResourceLibrary::LoadShader("grass_shell", "shaders/grass_shell.vert", "shaders/grass_shell.frag");
        ResourceLibrary::LoadShader("world_geometry", "shaders/world_geometry.vert", "shaders/world_geometry.frag");
        ResourceLibrary::LoadShader("debug_buffer", "shaders/frame_buffer.vert", "shaders/debug_buffer.frag");
        ResourceLibrary::LoadShader("point_light", "shaders/light_volume.vert", "shaders/point_light.frag");
        ResourceLibrary::LoadShader("spot_light", "shaders/light_volume.vert", "shaders/spot_light.frag");
        ResourceLibrary::LoadShader("mesh_position", "shaders/mesh_depth.vert", "shaders/mesh_position.frag");
        ResourceLibrary::LoadShader("skeletal_geometry", "shaders/skeletal_geometry.vert", "shaders/mesh_geometry.frag");
        ResourceLibrary::LoadShader("bloom_extract", "shaders/frame_buffer.vert", "shaders/bloom_extract.frag");
        ResourceLibrary::LoadShader("bloom_kawase_down", "shaders/frame_buffer.vert", "shaders/bloom_kawase.frag");
        ResourceLibrary::LoadShader("bloom_kawase_up", "shaders/frame_buffer.vert", "shaders/bloom_kawase_up.frag");
        ResourceLibrary::LoadShader("cloud_volume", "shaders/frame_buffer.vert", "shaders/cloud_volume.frag");
        ResourceLibrary::LoadShader("bilateral_upsample", "shaders/frame_buffer.vert", "shaders/bilateral_upsample.frag");
        ResourceLibrary::LoadShader("xray", "shaders/xray.vert", "shaders/xray.frag");


    }

    void RenderLayer::SetupComputeShaders()
    {

    }

    void RenderLayer::SetupTextures()
    {
        m_cloudNoiseTexture = Noise::Generator::CreateOrLoadCloudNoiseTexture3D(512, "Cache/CloudNoise512.fn2t");

        ResourceLibrary::LoadTexture("white_texture", "textures/white.png");
        ResourceLibrary::LoadTexture("black_texture", "textures/black.png");
        ResourceLibrary::LoadTexture("normal_texture", "textures/normal.png");

        ResourceLibrary::LoadTexture("stars_texture", "textures/stars.tga");
        ResourceLibrary::LoadTexture("brick_normal", "textures/brick_normal.tga");
        ResourceLibrary::LoadTexture("stone_pavement_albedo", "textures/stone_pavement_albedo.tga");
        ResourceLibrary::LoadTexture("stone_pavement_normal", "textures/stone_pavement_normal.tga");
        ResourceLibrary::LoadTexture("brick_wall_albedo", "textures/brick_wall_basecolour.tga");
        ResourceLibrary::LoadTexture("brick_wall_normal", "textures/brick_wall_normal.tga");

        ResourceLibrary::LoadTexture("wood_plain_albedo", "textures/wood_plain_albedo.tga");
        ResourceLibrary::LoadTexture("wood_plain_normal", "textures/wood_plain_normal.tga");
        ResourceLibrary::LoadTexture("wood_plain_rough", "textures/wood_plain_rough.tga");

        ResourceLibrary::LoadTexture("wood_planks_albedo", "textures/wood_planks_albedo.tga");
        ResourceLibrary::LoadTexture("wood_planks_normal", "textures/wood_planks_normal.tga");
        ResourceLibrary::LoadTexture("wood_planks_rough", "textures/wood_planks_rough.tga");


        ResourceLibrary::LoadTexture("roof_tiles_albedo", "textures/roof_tiles_albedo.tga");
        ResourceLibrary::LoadTexture("roof_tiles_normal", "textures/roof_tiles_normal.tga");
        ResourceLibrary::LoadTexture("roof_tiles_rough", "textures/roof_tiles_rough.tga");

        ResourceLibrary::LoadTexture("brick_plain_albedo", "textures/brick_plain_albedo.tga");
        ResourceLibrary::LoadTexture("brick_plain_normal", "textures/brick_plain_normal.tga");
        ResourceLibrary::LoadTexture("brick_plain_rough", "textures/brick_plain_rough.tga");

        ResourceLibrary::LoadTexture("brick_plaster_albedo", "textures/brick_plaster_albedo.tga");
        ResourceLibrary::LoadTexture("brick_plaster_normal", "textures/brick_plaster_normal.tga");
        ResourceLibrary::LoadTexture("brick_plaster_rough", "textures/brick_plaster_rough.tga");

        ResourceLibrary::LoadTexture("rock_plain_albedo", "textures/rock_plain_albedo.tga");
        ResourceLibrary::LoadTexture("rock_plain_normal", "textures/rock_plain_normal.tga");
        ResourceLibrary::LoadTexture("rock_plain_rough", "textures/rock_plain_rough.tga");

        ResourceLibrary::LoadTexture("rock_wall_albedo", "textures/rock_wall_albedo.tga");
        ResourceLibrary::LoadTexture("rock_wall_normal", "textures/rock_wall_normal.tga");
        ResourceLibrary::LoadTexture("rock_wall_rough", "textures/rock_wall_rough.tga");

        ResourceLibrary::LoadTexture("plaster_plain_albedo", "textures/plaster_plain_albedo.tga");
        ResourceLibrary::LoadTexture("plaster_plain_normal", "textures/plaster_plain_normal.tga");
        ResourceLibrary::LoadTexture("plaster_plain_rough", "textures/plaster_plain_rough.tga");

        ResourceLibrary::LoadTexture("dirt_plain_albedo", "textures/dirt_plain_albedo.tga");
        ResourceLibrary::LoadTexture("dirt_plain_normal", "textures/dirt_plain_normal.tga");
        ResourceLibrary::LoadTexture("dirt_plain_rough", "textures/dirt_plain_rough.tga");

        ResourceLibrary::LoadTexture("moss_plain_albedo", "textures/moss_plain_albedo.tga");
        ResourceLibrary::LoadTexture("moss_plain_normal", "textures/moss_plain_normal.tga");
        ResourceLibrary::LoadTexture("moss_plain_rough", "textures/moss_plain_rough.tga");


        ResourceLibrary::LoadTexture("rope_albedo", "textures/rope_albedo.tga");
        ResourceLibrary::LoadTexture("rope_normal", "textures/rope_normal.tga");

        ResourceLibrary::LoadTexture("atlas_albedo", "textures/atlas_albedo_64.png");
        ResourceLibrary::LoadTexture("atlas_albedo_b", "textures/atlas_albedo_b_64.png");
        ResourceLibrary::LoadTexture("atlas_albedo_c", "textures/atlas_albedo_c.png");
        ResourceLibrary::LoadTexture("atlas_albedo_d", "textures/atlas_albedo_d.png");
        ResourceLibrary::LoadTexture("atlas_albedo_e", "textures/atlas_albedo_e.png");
        ResourceLibrary::LoadTexture("atlas_albedo_f", "textures/atlas_albedo_f.png");
        ResourceLibrary::LoadTexture("atlas_rough", "textures/atlas_rough_64.png");
        ResourceLibrary::LoadTexture("atlas_metal", "textures/atlas_metal_64.png");
        ResourceLibrary::LoadTexture("atlas_emissive", "textures/atlas_emissive_64.png");

        ResourceLibrary::LoadTexture("background_albedo", "textures/background_albedo.png");
        ResourceLibrary::LoadTexture("background_normal", "textures/background_normal.png");

        ResourceLibrary::LoadTexture("gravity_well_albedo", "textures/gravity_well_albedo.png");
        ResourceLibrary::LoadTexture("gravity_well_normal", "textures/gravity_well_normal.png");
        ResourceLibrary::LoadTexture("gravity_well_a_albedo", "textures/gravity_well_a_albedo.png");
        ResourceLibrary::LoadTexture("gravity_well_b_albedo", "textures/gravity_well_b_albedo.png");

        ResourceLibrary::LoadTexture("spirit_tree_albedo", "textures/spirit_tree_albedo.png");
        ResourceLibrary::LoadTexture("spirit_tree_colour", "textures/spirit_tree_colour.png");
        ResourceLibrary::LoadTexture("spirit_tree_emissive", "textures/spirit_tree_emissive.png");

        ResourceLibrary::LoadTexture("tree_imposter_albedo", "textures/tree_imposter_albedo.png");
        ResourceLibrary::LoadTexture("tree_imposter_normal", "textures/tree_imposter_normal.png");

        ResourceLibrary::LoadTexture("the_canyon_albedo", "textures/the_canyon_albedo.png");
        ResourceLibrary::LoadTexture("the_canyon_normal", "textures/the_canyon_normal.png");

        ResourceLibrary::LoadTexture("stained_glass", "textures/stained_glass.png");

        ResourceLibrary::LoadTexture("icon_atlas", "textures/icon_atlas.png");
        ResourceLibrary::LoadTexture("dither_texture", "textures/bayer_matrix.tga");
        ResourceLibrary::LoadTexture("bayer_2", "textures/bayer_2.png");
        ResourceLibrary::LoadTexture("bayer_4", "textures/bayer_4.png");
        ResourceLibrary::LoadTexture("bayer_8", "textures/bayer_8.png");
        ResourceLibrary::LoadTexture("bayer_16", "textures/bayer_16.png");
        ResourceLibrary::LoadTexture("grid_texture", "textures/grid_texture.tga");
        ResourceLibrary::LoadTexture("paper_texture", "textures/white_paper.tga");
        ResourceLibrary::LoadTexture("hatch_texture", "textures/line_hatch.tga");
        ResourceLibrary::LoadTexture("watercolour_texture", "textures/water_colour.tga");
        ResourceLibrary::LoadTexture("blue_noise_texture", "textures/blue_noise_512.png");
        ResourceLibrary::LoadTexture("blue_noise_blur", "textures/blue_noise_blur.png");

        ResourceLibrary::LoadTexture("pixel_ui", "textures/pixel_ui.png");
        ResourceLibrary::LoadTexture("pixel_ui_512", "textures/pixel_ui_512.png");
        ResourceLibrary::LoadTexture("pixel_ui_fullscreen", "textures/pixel_ui_512.png");

        ResourceLibrary::LoadTexture("titlecard", "textures/tumulttitlecard.png");
    }

    void RenderLayer::SetupMaterials()
    {
        auto meshShader = ResourceLibrary::GetShader("mesh_geometry");
        auto worldShader = ResourceLibrary::GetShader("world_geometry");
        auto parallaxShader = ResourceLibrary::GetShader("parallax_geometry");
        auto foliageShader = ResourceLibrary::GetShader("foliage_geometry");

        ResourceLibrary::LoadMaterial("glass_material", meshShader);
        auto glass_material = ResourceLibrary::GetMaterial("glass_material");
        glass_material->SetTexture("albedoMap", ResourceLibrary::GetTexture("stained_glass"));
        glass_material->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        glass_material->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        glass_material->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        glass_material->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        glass_material->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("default_material", meshShader);
        auto defaultMat = ResourceLibrary::GetMaterial("default_material");
        defaultMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        defaultMat->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        defaultMat->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        defaultMat->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        defaultMat->SetVec3("colourTint", { 0.4f, 0.4f, 0.4f });

        ResourceLibrary::LoadMaterial("foliage_material", foliageShader);
        auto foliageMaterial = ResourceLibrary::GetMaterial("foliage_material");
        foliageMaterial->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo"));
        foliageMaterial->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        foliageMaterial->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        foliageMaterial->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));
        foliageMaterial->SetTexture("emissiveMap", ResourceLibrary::GetTexture("atlas_emissive"));
        foliageMaterial->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });

        ResourceLibrary::LoadMaterial("foliage_material_b", foliageShader);
        auto foliage_material_b = ResourceLibrary::GetMaterial("foliage_material_b");
        foliage_material_b->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo_b"));
        foliage_material_b->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        foliage_material_b->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        foliage_material_b->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));
        foliage_material_b->SetTexture("emissiveMap", ResourceLibrary::GetTexture("atlas_emissive"));
        foliage_material_b->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });

        ResourceLibrary::LoadMaterial("parallax_middle", parallaxShader);
        auto parallaxMat = ResourceLibrary::GetMaterial("parallax_middle");
        parallaxMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        parallaxMat->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        parallaxMat->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        parallaxMat->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        parallaxMat->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });
        parallaxMat->SetFloat("parallaxFactor", 0.5f);

        ResourceLibrary::LoadMaterial("parallax_back", parallaxShader);
        auto parallaxBack = ResourceLibrary::GetMaterial("parallax_back");
        parallaxBack->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo"));
        parallaxBack->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        parallaxBack->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        parallaxBack->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));
        parallaxBack->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });
        parallaxBack->SetFloat("parallaxFactor", 0.5f);

        auto imposterMat = ResourceLibrary::LoadMaterial("tree_imposter", parallaxShader);
        imposterMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("tree_imposter_albedo"));
        imposterMat->SetTexture("normalMap", ResourceLibrary::GetTexture("tree_imposter_normal"));
        imposterMat->SetFloat("parallaxFactor", 0.5f);

        auto canyonMat = ResourceLibrary::LoadMaterial("the_canyon", parallaxShader);

        canyonMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("the_canyon_albedo"));
        canyonMat->SetTexture("normalMap", ResourceLibrary::GetTexture("the_canyon_normal"));
        canyonMat->SetFloat("parallaxFactor", 0.5f);

        ResourceLibrary::LoadMaterial("gravity_well", meshShader);
        auto gravityWellMat = ResourceLibrary::GetMaterial("gravity_well");
        gravityWellMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("gravity_well_albedo"));
        gravityWellMat->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        gravityWellMat->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        gravityWellMat->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        gravityWellMat->SetTexture("emissiveMap", ResourceLibrary::GetTexture("black_texture"));
        gravityWellMat->SetVec3("colourTint", { 1.0f, 0.8f, 0.6f });

        ResourceLibrary::LoadMaterial("spirit_tree", meshShader);
        auto spiritTreeMat = ResourceLibrary::GetMaterial("spirit_tree");
        spiritTreeMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("spirit_tree_colour"));
        spiritTreeMat->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        spiritTreeMat->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        spiritTreeMat->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        spiritTreeMat->SetTexture("emissiveMap", ResourceLibrary::GetTexture("spirit_tree_emissive"));
        spiritTreeMat->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });

        auto starMat = ResourceLibrary::LoadMaterial("star_material", parallaxShader);
        starMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("stars_texture"));
        starMat->SetTexture("emissiveMap", ResourceLibrary::GetTexture("stars_texture"));
        starMat->SetFloat("parallaxFactor", 0.5f);

        auto worldMat = ResourceLibrary::LoadMaterial("world_material", worldShader);
        worldMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        worldMat->SetTexture("normalMap", ResourceLibrary::GetTexture("brick_normal"));
        worldMat->SetFloat("textureScale", 0.125f);
        worldMat->SetVec3("colourTint", { 0.35f, 0.35f, 0.35f });

        auto woodPlainMat = ResourceLibrary::LoadMaterial("M_wood_plain", worldShader);
        woodPlainMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("wood_plain_albedo"));
        woodPlainMat->SetTexture("normalMap", ResourceLibrary::GetTexture("wood_plain_normal"));
        woodPlainMat->SetTexture("roughMap", ResourceLibrary::GetTexture("wood_plain_rough"));
        woodPlainMat->SetFloat("textureScale", 0.16f);
        woodPlainMat->SetVec3("colourTint", { 0.75f, 0.75f, 0.75f });


        auto woodPlanksMat = ResourceLibrary::LoadMaterial("M_wood_planks", worldShader);
        woodPlanksMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("wood_planks_albedo"));
        woodPlanksMat->SetTexture("normalMap", ResourceLibrary::GetTexture("wood_planks_normal"));
        woodPlanksMat->SetTexture("roughMap", ResourceLibrary::GetTexture("wood_planks_rough"));
        woodPlanksMat->SetFloat("textureScale", 0.16f);


        auto rockPlainMat = ResourceLibrary::LoadMaterial("M_ground_rock", worldShader);
        rockPlainMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("rock_plain_albedo"));
        rockPlainMat->SetTexture("normalMap", ResourceLibrary::GetTexture("rock_plain_normal"));
        rockPlainMat->SetTexture("roughMap", ResourceLibrary::GetTexture("rock_plain_rough"));
        rockPlainMat->SetFloat("textureScale", 0.16f);


        auto dirtPlainMat = ResourceLibrary::LoadMaterial("M_ground_dirt", worldShader);
        dirtPlainMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("dirt_plain_albedo"));
        dirtPlainMat->SetTexture("normalMap", ResourceLibrary::GetTexture("dirt_plain_normal"));
        dirtPlainMat->SetTexture("roughMap", ResourceLibrary::GetTexture("dirt_plain_rough"));
        dirtPlainMat->SetFloat("textureScale", 0.08f);


        auto mossPlainMat = ResourceLibrary::LoadMaterial("M_ground_moss", worldShader);
        mossPlainMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("moss_plain_albedo"));
        mossPlainMat->SetTexture("normalMap", ResourceLibrary::GetTexture("moss_plain_normal"));
        mossPlainMat->SetTexture("roughMap", ResourceLibrary::GetTexture("moss_plain_rough"));
        mossPlainMat->SetFloat("textureScale", 0.16f);


        auto plasterPlainMat = ResourceLibrary::LoadMaterial("M_wall_plaster", worldShader);
        plasterPlainMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("plaster_plain_albedo"));
        plasterPlainMat->SetTexture("normalMap", ResourceLibrary::GetTexture("plaster_plain_normal"));
        plasterPlainMat->SetTexture("roughMap", ResourceLibrary::GetTexture("plaster_plain_rough"));
        plasterPlainMat->SetFloat("textureScale", 0.16f);


        auto brickPlainMat = ResourceLibrary::LoadMaterial("M_wall_brick", worldShader);
        brickPlainMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("brick_plain_albedo"));
        brickPlainMat->SetTexture("normalMap", ResourceLibrary::GetTexture("brick_plain_normal"));
        brickPlainMat->SetTexture("roughMap", ResourceLibrary::GetTexture("brick_plain_rough"));
        brickPlainMat->SetFloat("textureScale", 0.24f);


        auto brickPlasterMat = ResourceLibrary::LoadMaterial("M_wall_brick_plaster", worldShader);
        brickPlasterMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("brick_plaster_albedo"));
        brickPlasterMat->SetTexture("normalMap", ResourceLibrary::GetTexture("brick_plaster_normal"));
        brickPlasterMat->SetTexture("roughMap", ResourceLibrary::GetTexture("brick_plaster_rough"));
        brickPlasterMat->SetFloat("textureScale", 0.24f);


        auto roofTilesMat = ResourceLibrary::LoadMaterial("M_roof_tiles", meshShader);
        roofTilesMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("roof_tiles_albedo"));
        roofTilesMat->SetTexture("normalMap", ResourceLibrary::GetTexture("roof_tiles_normal"));
        roofTilesMat->SetTexture("roughMap", ResourceLibrary::GetTexture("roof_tiles_rough"));
        roofTilesMat->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        roofTilesMat->SetFloat("textureScale", 0.24f);


        auto rockWallMat = ResourceLibrary::LoadMaterial("M_wall_rock", worldShader);
        rockWallMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("rock_wall_albedo"));
        rockWallMat->SetTexture("normalMap", ResourceLibrary::GetTexture("rock_wall_normal"));
        rockWallMat->SetTexture("roughMap", ResourceLibrary::GetTexture("rock_wall_rough"));
        rockWallMat->SetFloat("textureScale", 0.16f);



        auto ropeMat = ResourceLibrary::LoadMaterial("rope_material", meshShader);
        ropeMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("rope_albedo"));
        ropeMat->SetTexture("normalMap", ResourceLibrary::GetTexture("rope_normal"));
        ropeMat->SetVec3("colourTint", { 0.6f, 0.4f, 0.2f });

        auto atlasMat = ResourceLibrary::LoadMaterial("atlas_material", meshShader);
        atlasMat->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo"));
        atlasMat->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        atlasMat->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));
        atlasMat->SetTexture("emissiveMap", ResourceLibrary::GetTexture("atlas_emissive"));


        auto atlasB = ResourceLibrary::LoadMaterial("atlas_b", meshShader);
        atlasB->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo_b"));
        atlasB->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        atlasB->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));
        atlasB->SetTexture("emissiveMap", ResourceLibrary::GetTexture("atlas_emissive"));

        auto atlasC = ResourceLibrary::LoadMaterial("atlas_c", meshShader);
        atlasC->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo_c"));
        atlasC->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        atlasC->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));

        auto atlasD = ResourceLibrary::LoadMaterial("atlas_d", meshShader);
        atlasD->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo_d"));
        atlasD->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        atlasD->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));

        auto atlasE = ResourceLibrary::LoadMaterial("atlas_e", meshShader);
        atlasE->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo_e"));
        atlasE->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        atlasE->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));

        auto atlasF = ResourceLibrary::LoadMaterial("atlas_f", meshShader);
        atlasF->SetTexture("albedoMap", ResourceLibrary::GetTexture("atlas_albedo_f"));
        atlasF->SetTexture("roughMap", ResourceLibrary::GetTexture("atlas_rough"));
        atlasF->SetTexture("metalMap", ResourceLibrary::GetTexture("atlas_metal"));



        ResourceLibrary::LoadMaterial("M_rune_1", meshShader);
        auto colourMat_r1 = ResourceLibrary::GetMaterial("M_rune_1");
        colourMat_r1->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r1->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r1->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r1->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r1->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r1->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("M_rune_2", meshShader);
        auto colourMat_r2 = ResourceLibrary::GetMaterial("M_rune_2");
        colourMat_r2->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r2->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r2->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r2->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r2->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r2->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("M_rune_3", meshShader);
        auto colourMat_r3 = ResourceLibrary::GetMaterial("M_rune_3");
        colourMat_r3->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r3->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r3->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r3->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r3->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r3->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("M_rune_4", meshShader);
        auto colourMat_r4 = ResourceLibrary::GetMaterial("M_rune_4");
        colourMat_r4->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r4->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r4->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r4->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r4->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r4->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("M_rune_5", meshShader);
        auto colourMat_r5 = ResourceLibrary::GetMaterial("M_rune_5");
        colourMat_r5->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r5->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r5->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r5->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r5->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r5->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("M_rune_6", meshShader);
        auto colourMat_r6 = ResourceLibrary::GetMaterial("M_rune_6");
        colourMat_r6->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r6->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r6->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r6->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r6->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r6->SetVec3("colourTint", { 1.0f, 1.0f, 1.0f });


        ResourceLibrary::LoadMaterial("M_humble_hole", meshShader);
        auto colourMat_hole = ResourceLibrary::GetMaterial("M_humble_hole");
        colourMat_hole->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hole->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hole->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hole->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hole->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hole->SetVec3("colourTint", { 1.0f, 0.1f, 0.0f });

        ResourceLibrary::LoadMaterial("M_humble_red", meshShader);
        auto colourMat_hr = ResourceLibrary::GetMaterial("M_humble_red");
        colourMat_hr->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hr->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hr->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hr->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hr->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hr->SetVec3("colourTint", { 1.0f, 0.1f, 0.0f });

        ResourceLibrary::LoadMaterial("M_humble_fire", meshShader);
        auto colourMat_hf = ResourceLibrary::GetMaterial("M_humble_fire");
        colourMat_hf->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hf->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hf->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hf->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hf->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hf->SetVec3("colourTint", { 1.0f, .4f, 0.0f });

        ResourceLibrary::LoadMaterial("M_humble_ghost", meshShader);
        auto colourMat_hgh = ResourceLibrary::GetMaterial("M_humble_ghost");
        colourMat_hgh->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hgh->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hgh->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hgh->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hgh->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hgh->SetVec3("colourTint", { 0.33f, 0.35f, 0.85f });


        ResourceLibrary::LoadMaterial("M_humble_blue", meshShader);
        auto colourMat_hb = ResourceLibrary::GetMaterial("M_humble_blue");
        colourMat_hb->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hb->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hb->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hb->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hb->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hb->SetVec3("colourTint", { 0.33f, 0.35f, 1.0f });

        ResourceLibrary::LoadMaterial("M_humble_green", meshShader);
        auto colourMat_hg = ResourceLibrary::GetMaterial("M_humble_green");
        colourMat_hg->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hg->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hg->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hg->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hg->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hg->SetVec3("colourTint", { 0.30f, 1.0f, 0.40f });

        ResourceLibrary::LoadMaterial("M_humble_orange", meshShader);
        auto colourMat_ho = ResourceLibrary::GetMaterial("M_humble_orange");
        colourMat_ho->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_ho->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_ho->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_ho->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_ho->SetVec3("colourTint", { 0.85f, 0.43f, 0.30f });

        ResourceLibrary::LoadMaterial("M_humble_purple", meshShader);
        auto colourMat_hp = ResourceLibrary::GetMaterial("M_humble_purple");
        colourMat_hp->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hp->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hp->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hp->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hp->SetTexture("emissiveMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hp->SetVec3("colourTint", { 0.45f, 0.33f, 1.0f });


        ResourceLibrary::LoadMaterial("M_humble_pink", meshShader);
        auto colourMat_hpi = ResourceLibrary::GetMaterial("M_humble_pink");
        colourMat_hpi->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hpi->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hpi->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hpi->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hpi->SetVec3("colourTint", { 0.85f, 0.33f, 0.85f });


        ResourceLibrary::LoadMaterial("M_humble_yellow", meshShader);
        auto colourMat_hy = ResourceLibrary::GetMaterial("M_humble_yellow");
        colourMat_hy->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hy->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hy->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hy->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hy->SetVec3("colourTint", { 0.85f, 0.83f, 0.35f });


        ResourceLibrary::LoadMaterial("M_humble_gold", meshShader);
        auto colourMat_hgo = ResourceLibrary::GetMaterial("M_humble_gold");
        colourMat_hgo->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hgo->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hgo->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hgo->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hgo->SetVec3("colourTint", { 0.85f, 0.65f, 0.13f });

        ResourceLibrary::LoadMaterial("M_humble_silver", meshShader);
        auto colourMat_hs = ResourceLibrary::GetMaterial("M_humble_silver");
        colourMat_hs->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hs->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hs->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hs->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hs->SetVec3("colourTint", { 0.70f, 0.72f, 0.75f });

        ResourceLibrary::LoadMaterial("M_humble_copper", meshShader);
        auto colourMat_hc = ResourceLibrary::GetMaterial("M_humble_copper");
        colourMat_hc->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hc->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_hc->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_hc->SetTexture("metalMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_hc->SetVec3("colourTint", { 0.75f, 0.57f, 0.40f });

        ResourceLibrary::LoadMaterial("M_solid_yellow", meshShader);
        auto colourMat_y = ResourceLibrary::GetMaterial("M_solid_yellow");
        colourMat_y->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_y->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_y->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_y->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_y->SetVec3("colourTint", { 1.0f, 1.0f, 0.0f });

        ResourceLibrary::LoadMaterial("M_solid_green", meshShader);
        auto colourMat_g = ResourceLibrary::GetMaterial("M_solid_green");
        colourMat_g->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_g->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_g->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_g->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_g->SetVec3("colourTint", { 0.04f, 1.0f, 0.0f });

        ResourceLibrary::LoadMaterial("M_solid_darkGreen", meshShader);
        auto colourMat_dg = ResourceLibrary::GetMaterial("M_solid_darkGreen");
        colourMat_dg->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_dg->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_dg->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_dg->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_dg->SetVec3("colourTint", { 0.1f, 0.38f, 0.0f });

        ResourceLibrary::LoadMaterial("M_solid_red", meshShader);
        auto colourMat_r = ResourceLibrary::GetMaterial("M_solid_red");
        colourMat_r->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_r->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_r->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_r->SetVec3("colourTint", { 1.0f, 0.0f, 0.0f });

        ResourceLibrary::LoadMaterial("M_solid_blue", meshShader);
        auto colourMat_b = ResourceLibrary::GetMaterial("M_solid_blue");
        colourMat_b->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_b->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_b->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_b->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_b->SetVec3("colourTint", { 0.0f, 0.45f, 1.0f });

        ResourceLibrary::LoadMaterial("M_solid_purple", meshShader);
        auto colourMat_p = ResourceLibrary::GetMaterial("M_solid_purple");
        colourMat_p->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_p->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_p->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_p->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_p->SetVec3("colourTint", { 0.56f, 0.02f, 1.0f });


        ResourceLibrary::LoadMaterial("M_solid_pink", meshShader);
        auto colourMat_pi = ResourceLibrary::GetMaterial("M_solid_pink");
        colourMat_pi->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_pi->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_pi->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_pi->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_pi->SetVec3("colourTint", { 1.0f, 0.01f, 0.84f });

        ResourceLibrary::LoadMaterial("M_solid_orange", meshShader);
        auto colourMat_o = ResourceLibrary::GetMaterial("M_solid_orange");
        colourMat_o->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_o->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_o->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_o->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_o->SetVec3("colourTint", { 1.0f, 0.21f, 0.01f });

        ResourceLibrary::LoadMaterial("M_solid_darkBlue", meshShader);
        auto colourMat_db = ResourceLibrary::GetMaterial("M_solid_darkBlue");
        colourMat_db->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_db->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_db->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_db->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_db->SetVec3("colourTint", { 0.04f, 0.03f, 0.63f });

        ResourceLibrary::LoadMaterial("M_solid_limeGreen", meshShader);
        auto colourMat_lg = ResourceLibrary::GetMaterial("M_solid_limeGreen");
        colourMat_lg->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
        colourMat_lg->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
        colourMat_lg->SetTexture("roughMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_lg->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
        colourMat_lg->SetVec3("colourTint", { 0.33f, 1.0f, 0.01f });
    }

    void RenderLayer::SetupMeshes()
    {
        ResourceLibrary::LoadMesh("box_mesh");
        ResourceLibrary::LoadMesh("sphere_mesh");
        ResourceLibrary::LoadMesh("plane_mesh");
        ResourceLibrary::LoadMesh("capsule_mesh");
        ResourceLibrary::LoadMesh("cone_mesh");
        ResourceLibrary::LoadMesh("cylinder_mesh");

        ResourceLibrary::GetMesh("box_mesh")->CreateBox(1.0f, 1.0f, 1.0f);
        ResourceLibrary::GetMesh("sphere_mesh")->CreateSphere(1.0f, 32, 32);
        ResourceLibrary::GetMesh("plane_mesh")->CreatePlane(1.0f, 1.0f, 1, 1);
        ResourceLibrary::GetMesh("capsule_mesh")->CreateCapsule(0.4f, 1.6f, 32);
        ResourceLibrary::GetMesh("cone_mesh")->CreateCone(1.0f, 1.0f, 32);
        ResourceLibrary::GetMesh("cylinder_mesh")->CreateCylinder(0.5f, 1.0f, 8);

        ResourceLibrary::LoadMesh("sword_mesh", "models/sword.obj");

        ResourceLibrary::LoadMesh("harpoon_mesh", "models/metal/harpoon.obj");

        ResourceLibrary::LoadMesh("grave_stone_a", "models/grave_stone_a.obj");

        ResourceLibrary::LoadMesh("metal_brace_a", "models/metal/metal_brace.obj");
        ResourceLibrary::LoadMesh("shield", "models/shield.obj");


        ResourceLibrary::LoadMesh("wood_crate", "models/wood/wood_crate.obj");
        ResourceLibrary::LoadMesh("wood_platform", "models/wood/wood_platform.obj");
        ResourceLibrary::LoadMesh("wood_beam", "models/wood/wood_beam_4m.obj");
        ResourceLibrary::LoadMesh("wood_beam_2m", "models/wood/wood_beam_2m.obj");
        ResourceLibrary::LoadMesh("wood_stairs", "models/wood/wood_stairs.obj");
        ResourceLibrary::LoadMesh("wood_plank_a", "models/wood/wood_plank_a.obj");
        ResourceLibrary::LoadMesh("wood_plank_b", "models/wood/wood_plank_b.obj");
        ResourceLibrary::LoadMesh("wood_plank_c", "models/wood/wood_plank_c.obj");

        ResourceLibrary::LoadMesh("stone_wall", "models/stone/stone_wall.obj");
        ResourceLibrary::LoadMesh("stone_window", "models/stone/stone_window.obj");
        ResourceLibrary::LoadMesh("stone_door", "models/stone/stone_door.obj");
        ResourceLibrary::LoadMesh("stone_fence", "models/stone/stone_fence.obj");
        ResourceLibrary::LoadMesh("stone_floor", "models/stone/stone_floor.obj");
        ResourceLibrary::LoadMesh("stone_dome", "models/stone/stone_dome.obj");
        ResourceLibrary::LoadMesh("stone_pillar", "models/stone/stone_pillar_a.obj");
        ResourceLibrary::LoadMesh("stone_pillar_thick", "models/stone/stone_pillar_b.obj");
        ResourceLibrary::LoadMesh("stone_circle", "models/stone/stone_circle.obj");
        ResourceLibrary::LoadMesh("stone_circle_hole", "models/stone/stone_circle_hole.obj");
        ResourceLibrary::LoadMesh("stone_foundation", "models/stone/stone_foundation.obj");
        ResourceLibrary::LoadMesh("stone_stairs", "models/stone/stone_stairs_4m.obj");
        ResourceLibrary::LoadMesh("stone_stairs_2m", "models/stone/stone_stairs_2m.obj");
        ResourceLibrary::LoadMesh("stone_bridge", "models/stone/stone_bridge.obj");

        ResourceLibrary::LoadMesh("stone_revolver", "models/stone/stone_revolver.obj");
        ResourceLibrary::LoadMesh("stone_tower_base", "models/stone/stone_tower_base.obj");
        ResourceLibrary::LoadMesh("stone_revolver_floor", "models/stone/stone_revolver_floor.obj");
        ResourceLibrary::LoadMesh("stone_revolver_rim", "models/stone/stone_revolver_rim.obj");
        ResourceLibrary::LoadMesh("stone_revolver_ivy", "models/stone/stone_revolver_ivy.obj");

        ResourceLibrary::LoadMesh("tower_revolver", "models/tower/tower_revolver.obj");
        ResourceLibrary::LoadMesh("tower_floor", "models/tower/tower_floor.obj");
        ResourceLibrary::LoadMesh("tower_ivy", "models/tower/tower_ivy.obj");
        ResourceLibrary::LoadMesh("tower_ivy_small", "models/tower/tower_ivy_small.obj");
        ResourceLibrary::LoadMesh("tower_glass", "models/tower/tower_glass.obj");
        ResourceLibrary::LoadMesh("tower_metal", "models/tower/tower_metal.obj");
        ResourceLibrary::LoadMesh("tower_layer_plaster_plain", "models/tower/tower_layer_plaster_plain.obj");
        ResourceLibrary::LoadMesh("tower_layer_plaster_window", "models/tower/tower_layer_plaster_window.obj");
        ResourceLibrary::LoadMesh("tower_layer_stone", "models/tower/tower_layer_stone.obj");
        ResourceLibrary::LoadMesh("tower_a", "models/tower/tower_a.obj");
        ResourceLibrary::LoadMesh("tower_b", "models/tower/tower_b.obj");
        ResourceLibrary::LoadMesh("tower_c", "models/tower/tower_c.obj");
        ResourceLibrary::LoadMesh("tower_d", "models/tower/tower_d.obj");
        ResourceLibrary::LoadMesh("tower_e", "models/tower/tower_e.obj");
        ResourceLibrary::LoadMesh("tower_f", "models/tower/tower_f.obj");
        ResourceLibrary::LoadMesh("tower_g", "models/tower/tower_g.obj");
        ResourceLibrary::LoadMesh("tower_h", "models/tower/tower_h.obj");
        ResourceLibrary::LoadMesh("tower_roof", "models/tower/tower_roof.obj");

        ResourceLibrary::LoadMesh("stone_revolver_opt", "textures/stone_revolver.obj");
        ResourceLibrary::LoadMesh("the_canyon", "textures/the_canyon.obj");

        ResourceLibrary::LoadMesh("rock_a", "models/nature/rock_a.obj");
        ResourceLibrary::LoadMesh("rock_b", "models/nature/rock_b.obj");
        ResourceLibrary::LoadMesh("rock_c", "models/nature/rock_c.obj");
        ResourceLibrary::LoadMesh("rock_a_smooth", "models/nature/rock_b_smooth.obj");
        ResourceLibrary::LoadMesh("rock_b_smooth", "models/nature/rock_a_smooth.obj");

        ResourceLibrary::LoadMesh("dirt_a", "models/nature/dirt_a.obj");

        ResourceLibrary::LoadMesh("mushroom", "models/nature/mushroom.obj");

        ResourceLibrary::LoadMesh("tree_pine", "models/nature/tree_pine.obj");
        ResourceLibrary::LoadMesh("tree_fir", "models/nature/tree_fir.obj");
        ResourceLibrary::LoadMesh("tree_imposter", "models/nature/tree_imposter.obj");

        ResourceLibrary::LoadMesh("cloud_small_a", "models/nature/cloud_small_a.obj");
        ResourceLibrary::LoadMesh("cloud_small_b", "models/nature/cloud_small_b.obj");
        ResourceLibrary::LoadMesh("cloud_large_a", "models/nature/cloud_large_a.obj");


        ResourceLibrary::LoadMesh("block_dirt", "models/block/block_dirt.obj");
        ResourceLibrary::LoadMesh("block_sand", "models/block/block_sand.obj");
        ResourceLibrary::LoadMesh("block_stone", "models/block/block_stone.obj");
        ResourceLibrary::LoadMesh("block_ice", "models/block/block_ice.obj");

        ResourceLibrary::LoadMesh("floor_base", "models/primitives/floor_base.obj");
        ResourceLibrary::LoadMesh("floor_corner", "models/primitives/floor_corner.obj");

        ResourceLibrary::LoadMesh("background_terrain", "models/background_terrain.obj");

        ResourceLibrary::LoadMesh("world_dice", "models/world_dice.obj");

        ResourceLibrary::LoadMesh("revolver_arena", "Models/RevolverArena.obj");

        ResourceLibrary::LoadMesh("arena1", "Models/Arena1.obj");
        ResourceLibrary::LoadMesh("arena2", "Models/Arena2.obj");


        ResourceLibrary::LoadMesh("blunderbuss", "Models/Blunderbuss.obj");
        ResourceLibrary::LoadMesh("chest_body", "Models/ChestBody.obj");
        ResourceLibrary::LoadMesh("chest_lid", "Models/ChestLid.obj");
        ResourceLibrary::LoadMesh("fence_curved", "Models/GraveyardFenceCurve.obj");
        ResourceLibrary::LoadMesh("fence_straight", "Models/GraveyardFenceStraight.obj");
        ResourceLibrary::LoadMesh("lamppost", "Models/Lamppost.obj");
        ResourceLibrary::LoadMesh("vine_lamppost", "Models/ProcVinesLamppost.obj");
        ResourceLibrary::LoadMesh("archway", "Models/Archway.obj");
        ResourceLibrary::LoadMesh("vine_archway", "Models/ProcVinesArch.obj");
        ResourceLibrary::LoadMesh("telescope", "Models/Scope.obj");
        ResourceLibrary::LoadMesh("ballista2", "Models/Ballista2.obj");
        ResourceLibrary::LoadMesh("ballista1", "Models/Ballista1.obj");
        ResourceLibrary::LoadMesh("SM_Arrow", "Models/Arrow.obj");
        ResourceLibrary::LoadMesh("Ship", "Models/Ship.obj");
        ResourceLibrary::LoadMesh("cannon", "Models/Cannon.obj");
        ResourceLibrary::LoadMesh("post", "Models/post.obj");
        ResourceLibrary::LoadMesh("shield2", "Models/shield2.obj");
        ResourceLibrary::LoadMesh("WindDragon", "Models/WindDragon.obj");
        ResourceLibrary::LoadMesh("Scaffold", "Models/Scaffold.obj");
        ResourceLibrary::LoadMesh("Scaffold_2", "Models/Scaffold_2.obj");
        ResourceLibrary::LoadMesh("Ladder", "Models/Ladder.obj");


        ResourceLibrary::LoadMesh("WindmillBase", "Models/WindmillBase.obj");
        ResourceLibrary::LoadMesh("WindmillFan", "Models/WindmillFan.obj");
        ResourceLibrary::LoadMesh("WindmillFan2", "Models/WindmillFan2.obj");
        ResourceLibrary::LoadMesh("WindmillFlags", "Models/WindmillFlags.obj");
        ResourceLibrary::LoadMesh("WindmillMetal", "Models/WindmillMetal.obj");
        ResourceLibrary::LoadMesh("WindmillRoof", "Models/WindmillRoof.obj");
        ResourceLibrary::LoadMesh("WindmillTower", "Models/WindmillTower.obj");
        ResourceLibrary::LoadMesh("WindmillWood", "Models/WindmillWood.obj");

        ResourceLibrary::LoadMesh("TrainingDummy_1", "Models/TrainingDummy_1.obj");
        ResourceLibrary::LoadMesh("TrainingDummy_2", "Models/TrainingDummy_2.obj");
        ResourceLibrary::LoadMesh("TrainingDummy_3", "Models/TrainingDummy_3.obj");

        ResourceLibrary::LoadMesh("CastleStairs", "Models/CastleStairs.obj");
        ResourceLibrary::LoadMesh("Props", "Models/Props.obj");

        // Primitive meshes
        ResourceLibrary::LoadMesh("SM_Cone", "models/primitives/cone.obj");
        ResourceLibrary::LoadMesh("SM_Cube", "models/primitives/cube.obj");
        ResourceLibrary::LoadMesh("SM_Cylinder", "models/primitives/cylinder.obj");
        ResourceLibrary::LoadMesh("SM_IcoSphere", "models/primitives/icosphere.obj");
        ResourceLibrary::LoadMesh("SM_Plane", "models/primitives/plane.obj");
        ResourceLibrary::LoadMesh("SM_UVSphere", "models/primitives/uvsphere.obj");
        ResourceLibrary::LoadMesh("SM_InverseSphere", "models/primitives/inverseuvsphere.obj");

        //Runes
        ResourceLibrary::LoadMesh("SM_Rune_a", "Models/runes/SM_Rune_a.obj");
        ResourceLibrary::LoadMesh("SM_Rune_b", "Models/runes/SM_Rune_b.obj");
        ResourceLibrary::LoadMesh("SM_Rune_c", "Models/runes/SM_Rune_c.obj");
        ResourceLibrary::LoadMesh("SM_Rune_d", "Models/runes/SM_Rune_d.obj");
        ResourceLibrary::LoadMesh("SM_Rune_e", "Models/runes/SM_Rune_e.obj");
        ResourceLibrary::LoadMesh("SM_Rune_f", "Models/runes/SM_Rune_f.obj");
        ResourceLibrary::LoadMesh("SM_Rune_g", "Models/runes/SM_Rune_g.obj");
        ResourceLibrary::LoadMesh("SM_Rune_h", "Models/runes/SM_Rune_h.obj");

        // Douglas Test Meshes
        ResourceLibrary::LoadMesh("SM_BoxScaleTest", "models/douglastestmesh/SM_BoxScaleTest.obj");
        ResourceLibrary::LoadMesh("SM_BoxWithHole", "models/douglastestmesh/SM_BoxWithHole.obj");
        ResourceLibrary::LoadMesh("SM_GroundBlockOut", "models/douglastestmesh/SM_GroundBlockOut.obj");
        ResourceLibrary::LoadMesh("SM_PotionBottle", "Models/douglastestmesh/SM_PotionBottle.obj");
        ResourceLibrary::LoadMesh("SM_LevelTest", "Models/douglastestmesh/levelTest2.obj");
        ResourceLibrary::LoadMesh("SM_LanterCandle", "Models/douglastestmesh/SM_LanternCandle.obj");

        //Nature Objects
        ResourceLibrary::LoadMesh("SM_BlueBell", "Models/nature/SM_BlueBell.obj");
        ResourceLibrary::LoadMesh("SM_Daisy", "Models/nature/SM_Daisy.obj");

        //Misc Obejects
        ResourceLibrary::LoadMesh("SM_Barrel", "Models/wood/SM_Barrel.obj");
        ResourceLibrary::LoadMesh("SM_Lanttern", "Models/metal/SM_Lanttern.obj");
        ResourceLibrary::LoadMesh("SM_Plaque", "Models/stone/SM_Plaque.obj");


        //Split Floors
        ResourceLibrary::LoadMesh("SM_sotne_floor_circle_a", "Models/douglastestmesh/stonefloorpieces/sotne_floor_circle_a.obj");
        ResourceLibrary::LoadMesh("SM_sotne_floor_circle_b", "Models/douglastestmesh/stonefloorpieces/sotne_floor_circle_b.obj");
        ResourceLibrary::LoadMesh("SM_sotne_floor_circle_c", "Models/douglastestmesh/stonefloorpieces/sotne_floor_circle_c.obj");
        ResourceLibrary::LoadMesh("SM_sotne_floor_circle_d", "Models/douglastestmesh/stonefloorpieces/sotne_floor_circle_d.obj");
        ResourceLibrary::LoadMesh("SM_sotne_floor_circle_e", "Models/douglastestmesh/stonefloorpieces/sotne_floor_circle_e.obj");

        ResourceLibrary::LoadMesh("SM_gravity_well", "Models/gravity_well.obj");
        ResourceLibrary::LoadMesh("SM_gravity_well_a", "Models/gravity_well_a.obj");
        ResourceLibrary::LoadMesh("SM_gravity_well_b", "Models/gravity_well_b.obj");

        ResourceLibrary::LoadMesh("SM_spirit_tree", "Models/spirit_tree.obj");


        /* ResourceLibrary::LoadSkeletalMesh(
             "character",
             "animation/arnaud.fbx",
             "animation/skeleton.ozz",
             .01f
         );

         ResourceLibrary::LoadSkeletalAnimation(
             "character",
             "idle",
             "animation/animation.ozz"
         );*/


         /*  ResourceLibrary::LoadSkeletalAnimation(
               "bone_man",
               "idle_additive",
               "animation/idle_additive.ozz"
           );*/

        ResourceLibrary::LoadSkeletalMesh(
            "bone_man_2.0",
            "animation/bone_man_2.0.gltf",
            "animation/skeleton_2.0.ozz",
            1.0f
        );

        ResourceLibrary::LoadSkeletalMesh(
            "bone_man",
            "animation/bone_man_4.0.gltf",
            "animation/skeleton_2.0.ozz",
            1.0f
        );

        ResourceLibrary::LoadSkeletalMesh(
            "bone_man_blue",
            "animation/bone_man_blue.gltf",
            "animation/skeleton_2.0.ozz",
            1.0f
        );

        ResourceLibrary::LoadSkeletalMesh(
            "bone_man_orange",
            "animation/bone_man_orange.gltf",
            "animation/skeleton_2.0.ozz",
            1.0f
        );

        ResourceLibrary::LoadSkeletalMesh(
            "bone_man_purple",
            "animation/bone_man_purple.gltf",
            "animation/skeleton_2.0.ozz",
            1.0f
        );

        ResourceLibrary::LoadSkeletalMesh(
            "bone_man_green",
            "animation/bone_man_green.gltf",
            "animation/skeleton_2.0.ozz",
            1.0f
        );



        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man",
            "idle",
            "animation/idle_2.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man",
            "walk",
            "animation/walk_4.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_blue",
            "idle_blue",
            "animation/idle_2.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_blue",
            "walk_blue",
            "animation/walk_4.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_orange",
            "idle_orange",
            "animation/idle_2.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_orange",
            "walk_orange",
            "animation/walk_4.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_purple",
            "idle_purple",
            "animation/idle_2.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_purple",
            "walk_purple",
            "animation/walk_4.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_green",
            "idle_green",
            "animation/idle_2.0.ozz"
        );

        ResourceLibrary::LoadSkeletalAnimation(
            "bone_man_green",
            "walk_green",
            "animation/walk_4.0.ozz"
        );
    }

    void RenderLayer::SetupAO()
    {
        std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
        std::default_random_engine rng;

        m_aoKernel.clear();
        m_aoKernel.reserve(m_aoKernelSize);
        for (int i = 0; i < m_aoKernelSize; ++i)
        {
            glm::vec3 sample(
                randomFloats(rng) * 2.0f - 1.0f,
                randomFloats(rng) * 2.0f - 1.0f,
                randomFloats(rng) // z in [0,1] hemisphere
            );
            sample = glm::normalize(sample);
            sample *= randomFloats(rng);
            float scale = float(i) / float(m_aoKernelSize);
            scale = glm::mix(0.1f, 1.0f, scale * scale);
            sample *= scale;
            sample.z = sample.z;
            m_aoKernel.push_back(sample);
        }

        auto shader = ResourceLibrary::GetShader("ambient_occlusion");

        shader->Use();
        shader->SetIntUniform("positionTexture", 0);
        shader->SetIntUniform("normalTexture", 1);
        shader->SetIntUniform("noiseTexture", 2);
        for (int i = 0; i < m_aoKernelSize; ++i)
        {
            std::string uniformName = "samples[" + std::to_string(i) + "]";
            shader->SetVec3Uniform(uniformName, m_aoKernel[i]);
        }
        // shader->SetFloatUniform("radius", m_aoRadius);
        // shader->SetFloatUniform("bias", m_aoBias);
        // shader->SetFloatUniform("power", m_aoPower);

    }

    void RenderLayer::SetupStencilView()
    {
        const Texture& depthStencil = m_gBuffer->GetDepthAttachment();

        glGenTextures(1, &m_stencilView);
        glTextureView(m_stencilView, GL_TEXTURE_2D, depthStencil.GetHandle(),
            GL_STENCIL_INDEX8, 0, 1, 0, 1);
    }

    void RenderLayer::RenderShadowPass()//const glm::mat4& lightSpaceMatrix)
    {
        m_shadowBuffer->Bind();
        {
            m_rendererModule->RenderShadows();
            m_ropeModule->RenderShadows();//lightSpaceMatrix);
        }
        m_shadowBuffer->Unbind();
    }

    void RenderLayer::RenderGeometryPass()
    {
        auto camera = m_cameraModule->GetMainCamera();

        m_gBuffer->Bind();
        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glEnable(GL_STENCIL_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilMask(0xFF);
            glClearStencil(0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            m_rendererModule->Render();

            if (m_ropeModule) m_ropeModule->Render();
            if (m_caribbeanModule) m_caribbeanModule->Render();

            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(3, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(4, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); 

            m_physicsModule->RenderDebug();

            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(2, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(3, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(4, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            glDisable(GL_STENCIL_TEST);
        }
        m_gBuffer->Unbind();
    }

    void RenderLayer::RenderBackfacePass()
    {
        auto camera = m_cameraModule->GetMainCamera();

        m_backfaceBuffer->Bind();
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            //glEnable(GL_DEPTH_TEST);
            //glDepthFunc(GL_LESS);
            //glDepthMask(GL_TRUE);

            auto shader = ResourceLibrary::GetShader("mesh_position");
            //glm::mat4 view = m_cameraModule->GetViewMatrix(camera);
            //glm::mat4 projection = m_cameraModule->GetProjectionMatrix(camera);

            m_rendererModule->RenderBackfaces();

            if (m_ropeModule) m_ropeModule->RenderBackfaces();

            glCullFace(GL_BACK);
            //glDepthFunc(GL_LESS);
            //glDepthMask(GL_TRUE);

        }
        m_backfaceBuffer->Unbind();
    }

    void RenderLayer::RenderAOPass()
    {
        auto camera = m_cameraModule->GetMainCamera();
        auto camComp = camera.try_get_mut<CameraComponent>();

        auto shader = ResourceLibrary::GetShader("ambient_occlusion");

        m_aoBuffer->Bind();
        {
            glClear(GL_COLOR_BUFFER_BIT);

            shader->Use();

            const Texture& positionTexture = m_gBuffer->GetColourAttachment(2);
            const Texture& normalTexture = m_gBuffer->GetColourAttachment(1);
            Texture* noiseTex = ResourceLibrary::GetTexture("blue_noise_texture");

            positionTexture.Bind(0);
            shader->SetIntUniform("positionTexture", 0);
            normalTexture.Bind(1);
            shader->SetIntUniform("normalTexture", 1);
            noiseTex->Bind(2);
            shader->SetIntUniform("noiseTexture", 2);

            glm::mat4 view = m_cameraModule->GetViewMatrix(camera);
            glm::mat4 proj = m_cameraModule->GetProjectionMatrix(camera);
            shader->SetMat4Uniform("view", view);
            shader->SetMat4Uniform("proj", proj);

            glm::vec2 screenSize((float)m_aoBuffer->GetWidth(), (float)m_aoBuffer->GetHeight());
            shader->SetVec2Uniform("noiseScale", glm::vec2(screenSize.x / 512.0f, screenSize.y / 512.0f));

            for (int i = 0; i < m_aoKernelSize; ++i)
            {
                std::string uniformName = "samples[" + std::to_string(i) + "]";
                shader->SetVec3Uniform(uniformName, m_aoKernel[i]);
            }

            shader->SetFloatUniform("radius", m_aoRadius);
            shader->SetFloatUniform("bias", m_aoBias);
            shader->SetFloatUniform("power", m_aoPower);

            m_screenTarget->Render();
        }
        m_aoBuffer->Unbind();
    }

    void RenderLayer::RenderBlurPass()
    {
        auto shader = ResourceLibrary::GetShader("spiral_blur");

        m_blurBuffer->Bind();
        {
            glClear(GL_COLOR_BUFFER_BIT);
            shader->Use();

            const Texture& ssaoTex = m_aoBuffer->GetColourAttachment(0);
            ssaoTex.Bind(0);
            shader->SetIntUniform("aoInput", 0);
            glm::vec2 screenSize((float)m_blurBuffer->GetWidth(), (float)m_blurBuffer->GetHeight());
            shader->SetVec2Uniform("screenSize", screenSize);

            m_screenTarget->Render();
        }
        m_blurBuffer->Unbind();
    }

    void RenderLayer::RenderLightingPass()//const glm::mat4& lightSpaceMatrix, const glm::vec3& lightDirection)
    {
        m_lightingBuffer->Bind();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gBuffer->GetHandle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_lightingBuffer->GetHandle());
        glBlitFramebuffer(0, 0, m_gBuffer->GetWidth(), m_gBuffer->GetHeight(),
            0, 0, m_lightingBuffer->GetWidth(), m_lightingBuffer->GetHeight(),
            GL_STENCIL_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, m_lightingBuffer->GetHandle());

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);

        glDisable(GL_BLEND);
        RenderDirectionalLight();

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        RenderPointLights();
        RenderSpotLights();

        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_lightingBuffer->Unbind();
    }

    void RenderLayer::RenderXrayPass()
    {
        m_lightingBuffer->Bind();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gBuffer->GetHandle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_lightingBuffer->GetHandle());
        glBlitFramebuffer(0, 0, m_gBuffer->GetWidth(), m_gBuffer->GetHeight(),
            0, 0, m_lightingBuffer->GetWidth(), m_lightingBuffer->GetHeight(),
            GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, m_lightingBuffer->GetHandle());

        glEnable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 2, 0xFF);
        glStencilMask(0xFF);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        m_rendererModule->RenderXray();
        if (m_ropeModule) m_ropeModule->RenderXray();

        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_NOTEQUAL, 2, 0xFF);
        glStencilMask(0x00);

        glDisable(GL_DEPTH_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_rendererModule->RenderXray();
        if (m_ropeModule) m_ropeModule->RenderXray();

        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);

        m_lightingBuffer->Unbind();
    }

    void RenderLayer::RenderBloomPass()
    {
        m_bloomExtractBuffer->Bind();
        {
            glClear(GL_COLOR_BUFFER_BIT);
            auto extractShader = ResourceLibrary::GetShader("bloom_extract");
            extractShader->Use();

            const Texture& lightingTex = m_lightingBuffer->GetColourAttachment(0);
            lightingTex.Bind(0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            extractShader->SetIntUniform("inputTexture", 0);
            extractShader->SetFloatUniform("threshold", m_bloomThreshold);
            extractShader->SetFloatUniform("knee", m_bloomKnee);

            m_screenTarget->Render();

            lightingTex.Bind(0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        m_bloomExtractBuffer->Unbind();



        auto downShader = ResourceLibrary::GetShader("bloom_kawase_down");
        downShader->Use();
        downShader->SetIntUniform("inputTexture", 0);

        m_bloomMipChain[0]->Bind();
        {
            glClear(GL_COLOR_BUFFER_BIT);
            m_bloomExtractBuffer->GetColourAttachment(0).Bind(0);
            m_screenTarget->Render();
        }
        m_bloomMipChain[0]->Unbind();


        for (int i = 1; i < m_bloomMipLevels; ++i)
        {
            m_bloomMipChain[i]->Bind();
            {
                glClear(GL_COLOR_BUFFER_BIT);
                m_bloomMipChain[i - 1]->GetColourAttachment(0).Bind(0);
                m_screenTarget->Render();
            }
            m_bloomMipChain[i]->Unbind();
        }

        auto upShader = ResourceLibrary::GetShader("bloom_kawase_up");
        upShader->Use();
        upShader->SetIntUniform("inputTexture", 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        for (int i = m_bloomMipLevels - 2; i >= 0; --i)
        {
            m_bloomMipChain[i]->Bind();
            {
                m_bloomMipChain[i + 1]->GetColourAttachment(0).Bind(0);
                upShader->SetFloatUniform("radius", 1.0f);
                m_screenTarget->Render();
            }
            m_bloomMipChain[i]->Unbind();
        }

        glDisable(GL_BLEND);
    }

    void RenderLayer::RenderDirectionalLight()
    {
        auto camera = m_cameraModule->GetMainCamera();
        auto* mainLightComp = m_lightingModule->GetMainLightComponent();

        if (!mainLightComp || !mainLightComp->enabled) return;

        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);


        auto shader = ResourceLibrary::GetShader("mesh_lighting");
        shader->Use();

        const Texture& colourTexture = m_gBuffer->GetColourAttachment(0);
        const Texture& normalTexture = m_gBuffer->GetColourAttachment(1);
        const Texture& positionTexture = m_gBuffer->GetColourAttachment(2);
        const Texture& materialTexture = m_gBuffer->GetColourAttachment(3);
        const Texture& shadowTexture = m_shadowBuffer->GetDepthAttachment();
        const Texture& aoTexture = m_blurBuffer->GetColourAttachment(0);

        colourTexture.Bind(0);
        shader->SetIntUniform("colourTexture", 0);
        normalTexture.Bind(1);
        shader->SetIntUniform("normalTexture", 1);
        positionTexture.Bind(2);
        shader->SetIntUniform("positionTexture", 2);
        materialTexture.Bind(3);
        shader->SetIntUniform("materialTexture", 3);
        shadowTexture.Bind(4);
        shader->SetIntUniform("shadowMap", 4);

        Texture* ditherTexture = ResourceLibrary::GetTexture("bayer_4");
        ditherTexture->Bind(5);
        shader->SetIntUniform("ditherTexture", 5);

        aoTexture.Bind(6);
        shader->SetIntUniform("aoTexture", 6);

        shader->SetFloatUniform("aoStrength", m_aoPower);

        m_screenTarget->Render();
    }

    void RenderLayer::RenderPointLights()
    {
        auto pointLights = m_lightingModule->GetActivePointLights();
        if (pointLights.empty()) return;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GEQUAL);
        glDepthMask(GL_FALSE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        auto shader = ResourceLibrary::GetShader("point_light");
        shader->Use();

        m_gBuffer->GetColourAttachment(0).Bind(0);
        shader->SetIntUniform("gAlbedo", 0);
        m_gBuffer->GetColourAttachment(1).Bind(1);
        shader->SetIntUniform("gNormal", 1);
        m_gBuffer->GetColourAttachment(2).Bind(2);
        shader->SetIntUniform("gPosition", 2);
        m_gBuffer->GetColourAttachment(3).Bind(3);
        shader->SetIntUniform("gMaterial", 3);
        m_gBuffer->GetDepthAttachment().Bind(4);
        shader->SetIntUniform("gDepth", 4);

        m_backfaceBuffer->GetColourAttachment(0).Bind(5);
        shader->SetIntUniform("gBackPosition", 5);

        auto camera = m_cameraModule->GetMainCamera();
        glm::vec2 screenSize((float)m_lightingBuffer->GetWidth(), (float)m_lightingBuffer->GetHeight());
        shader->SetVec2Uniform("screenSize", screenSize);
        shader->SetVec3Uniform("cameraPosition", camera.try_get<TransformComponent>()->position);

        shader->SetIntUniform("lightingLevels", m_lightingLevels);

        glm::mat4 view = m_cameraModule->GetViewMatrix(camera);
        glm::mat4 projection = m_cameraModule->GetProjectionMatrix(camera);
        glm::mat4 viewProjMatrix = projection * view;
        shader->SetMat4Uniform("viewProjMatrix", viewProjMatrix);

        for (auto& lightEntity : pointLights)
        {
            auto* transform = lightEntity.try_get<TransformComponent>();
            auto* pointLight = lightEntity.try_get<PointLightComponent>();

            shader->SetVec3Uniform("lightColour", pointLight->colour);
            shader->SetFloatUniform("lightIntensity", pointLight->intensity);
            shader->SetFloatUniform("lightAttenuation", pointLight->attenuation);
            shader->SetVec3Uniform("lightPosition", Math::GetWorldPosition(lightEntity));
            shader->SetFloatUniform("lightRange", pointLight->range);

            shader->SetBoolUniform("enableShadows", pointLight->castShadows);
            shader->SetBoolUniform("enableThicknessAware", pointLight->thicknessAware);
            shader->SetIntUniform("shadowSteps", pointLight->shadowSteps);
            shader->SetFloatUniform("shadowBias", pointLight->shadowBias);

            glm::mat4 modelMatrix = Math::LocalToWorldMat(lightEntity);
            modelMatrix = glm::scale(modelMatrix, glm::vec3(pointLight->range));
            glm::mat4 mvp = projection * view * modelMatrix;
            shader->SetMat4Uniform("mvpMatrix", mvp);

            auto sphereMesh = ResourceLibrary::GetMesh("sphere_mesh");
            sphereMesh->Render();
        }

        glCullFace(GL_BACK);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
    }

    void RenderLayer::RenderSpotLights()
    {
        auto spotLights = m_lightingModule->GetActiveSpotLights();
        if (spotLights.empty()) return;

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GEQUAL);
        glDepthMask(GL_FALSE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        auto shader = ResourceLibrary::GetShader("spot_light");
        shader->Use();

        m_gBuffer->GetColourAttachment(0).Bind(0);
        shader->SetIntUniform("gAlbedo", 0);
        m_gBuffer->GetColourAttachment(1).Bind(1);
        shader->SetIntUniform("gNormal", 1);
        m_gBuffer->GetColourAttachment(2).Bind(2);
        shader->SetIntUniform("gPosition", 2);
        m_gBuffer->GetColourAttachment(3).Bind(3);
        shader->SetIntUniform("gMaterial", 3);
        m_gBuffer->GetDepthAttachment().Bind(4);
        shader->SetIntUniform("gDepth", 4);

        m_backfaceBuffer->GetColourAttachment(0).Bind(5);
        shader->SetIntUniform("gBackPosition", 5);

        auto camera = m_cameraModule->GetMainCamera();
        glm::vec2 screenSize((float)m_lightingBuffer->GetWidth(), (float)m_lightingBuffer->GetHeight());
        shader->SetVec2Uniform("screenSize", screenSize);
        shader->SetVec3Uniform("cameraPosition", camera.try_get<TransformComponent>()->position);

        shader->SetIntUniform("lightingLevels", m_lightingLevels);

        glm::mat4 view = m_cameraModule->GetViewMatrix(camera);
        glm::mat4 projection = m_cameraModule->GetProjectionMatrix(camera);
        glm::mat4 viewProjMatrix = projection * view;
        shader->SetMat4Uniform("viewProjMatrix", viewProjMatrix);

        for (auto& lightEntity : spotLights)
        {
            auto* transform = lightEntity.try_get<TransformComponent>();
            auto* spotLight = lightEntity.try_get<SpotLightComponent>();

            shader->SetVec3Uniform("lightColour", spotLight->colour);
            shader->SetFloatUniform("lightIntensity", spotLight->intensity);
            shader->SetFloatUniform("lightAttenuation", spotLight->attenuation);
            shader->SetFloatUniform("innerConeAngle", spotLight->innerConeAngle);
            shader->SetFloatUniform("outerConeAngle", spotLight->outerConeAngle);
            shader->SetVec3Uniform("lightPosition", Math::GetWorldPosition(lightEntity));
            shader->SetFloatUniform("lightRange", spotLight->range);

            glm::vec3 lightDirection = m_lightingModule->GetDirectionFromTransform(lightEntity);
            shader->SetVec3Uniform("lightDirection", lightDirection);

            shader->SetBoolUniform("enableShadows", spotLight->castShadows);
            shader->SetBoolUniform("enableThicknessAware", spotLight->thicknessAware);
            shader->SetIntUniform("shadowSteps", spotLight->shadowSteps);
            shader->SetFloatUniform("shadowBias", spotLight->shadowBias);
            glm::mat4 model = Math::LocalToWorldMat(lightEntity);

            float coneRadius = spotLight->range * tan(glm::radians(spotLight->outerConeAngle));


            glm::mat4 coneTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));


            coneTransform = glm::rotate(coneTransform, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));


            coneTransform = glm::scale(coneTransform, glm::vec3(coneRadius, spotLight->range, coneRadius));


            model = model * coneTransform;

            glm::mat4 mvp = projection * view * model;
            shader->SetMat4Uniform("mvpMatrix", mvp);
            auto coneMesh = ResourceLibrary::GetMesh("cone_mesh");
            coneMesh->Render();
        }

        glCullFace(GL_BACK);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
    }

    void RenderLayer::RenderCloudPass()
    {
        m_cloudBuffer->Bind();
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            auto shader = ResourceLibrary::GetShader("cloud_volume");
            shader->Use();

            m_gBuffer->GetColourAttachment(0).Bind(0);
            shader->SetIntUniform("colourTexture", 0);

            m_gBuffer->GetColourAttachment(2).Bind(1);
            shader->SetIntUniform("positionTexture", 1);

            m_gBuffer->GetDepthAttachment().Bind(2);
            shader->SetIntUniform("depthTexture", 2);

            m_lightingBuffer->GetColourAttachment(0).Bind(3);
            shader->SetIntUniform("inputTexture", 3);

            Texture* blueNoiseTex = ResourceLibrary::GetTexture("blue_noise_texture");
            if (blueNoiseTex)
            {
                blueNoiseTex->Bind(4);
                shader->SetIntUniform("blueNoiseTexture", 4);
            }

            if (m_cloudNoiseTexture)
            {
                m_cloudNoiseTexture->Bind(5);
                shader->SetIntUniform("noiseTexture3D", 5);
                //shader->SetBoolUniform("use3DNoiseTexture", true);
            }

            //shader->SetBoolUniform("enableClouds", true);
            //shader->SetFloatUniform("cloudHeight", 50.0f);
            //shader->SetFloatUniform("cloudThickness", 500.0f);
            //shader->SetIntUniform("cloudSteps", 8);
            //shader->SetFloatUniform("cloudDensity", 1.0f);
            //shader->SetFloatUniform("cloudPhaseForward", 0.8f);
            //shader->SetFloatUniform("cloudPhaseBackward", -0.3f);
            //shader->SetFloatUniform("cloudPhaseMix", 0.7f);
            //shader->SetFloatUniform("cloudAbsorption", 0.3f);
            //shader->SetFloatUniform("cloudLightMultiplier", 0.8f);
            //shader->SetFloatUniform("cloudAmbientAmount", 0.15f);
            //shader->SetFloatUniform("cloudJitterAmount", 0.3f);

            m_screenTarget->Render();

            glEnable(GL_DEPTH_TEST);
        }
        m_cloudBuffer->Unbind();

        m_cloudBufferUpsampled->Bind();
        {
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);

            auto upsampleShader = ResourceLibrary::GetShader("bilateral_upsample");
            upsampleShader->Use();

            m_cloudBuffer->GetColourAttachment(0).Bind(0);
            upsampleShader->SetIntUniform("lowResTexture", 0);

            m_gBuffer->GetDepthAttachment().Bind(1);
            upsampleShader->SetIntUniform("highResDepth", 1);

            m_gBuffer->GetColourAttachment(1).Bind(2);
            upsampleShader->SetIntUniform("highResNormal", 2);

            glm::vec2 lowResSize(m_cloudBuffer->GetWidth(), m_cloudBuffer->GetHeight());
            glm::vec2 highResSize(m_cloudBufferUpsampled->GetWidth(), m_cloudBufferUpsampled->GetHeight());

            upsampleShader->SetVec2Uniform("lowResSize", lowResSize);
            upsampleShader->SetVec2Uniform("highResSize", highResSize);

            // upsampleShader->SetFloatUniform("depthSigma", 0.1f);
            // upsampleShader->SetFloatUniform("normalSigma", 0.3f);
            // upsampleShader->SetFloatUniform("spatialSigma", 1.0f);

            m_screenTarget->Render();
            glEnable(GL_DEPTH_TEST);
        }
        m_cloudBufferUpsampled->Unbind();
    }

    void RenderLayer::RenderPostProcessPass()
    {
        m_postProcessBuffer->Bind();
        {
            auto shader = ResourceLibrary::GetShader("post_process");
            shader->Use();

            const Texture& litSceneTexture = m_lightingBuffer->GetColourAttachment(0);
            const Texture& lightingTexture = m_lightingBuffer->GetColourAttachment(1);
            const Texture& colourTexture = m_gBuffer->GetColourAttachment(0);
            const Texture& normalTexture = m_gBuffer->GetColourAttachment(1);
            const Texture& positionTexture = m_gBuffer->GetColourAttachment(2);
            const Texture& depthTexture = m_gBuffer->GetDepthAttachment();

            m_bloomMipChain[0]->GetColourAttachment(0).Bind(7);
            shader->SetIntUniform("bloomTexture", 7);
            shader->SetFloatUniform("bloomIntensity", m_bloomIntensity);

            litSceneTexture.Bind(0);
            shader->SetIntUniform("inputTexture", 0);
            colourTexture.Bind(1);
            shader->SetIntUniform("colourTexture", 1);
            normalTexture.Bind(2);
            shader->SetIntUniform("normalTexture", 2);
            depthTexture.Bind(3);
            shader->SetIntUniform("depthTexture", 3);
            lightingTexture.Bind(4);
            shader->SetIntUniform("lightingTexture", 4);
            positionTexture.Bind(5);
            shader->SetIntUniform("positionTexture", 5);

            Texture* watercolourTex = ResourceLibrary::GetTexture("watercolour_texture");
            if (watercolourTex)
            {
                watercolourTex->Bind(8);
                shader->SetIntUniform("watercolourTexture", 8);
            }

            Texture* paperTex = ResourceLibrary::GetTexture("paper_texture");
            if (paperTex)
            {
                paperTex->Bind(9);
                shader->SetIntUniform("paperTexture", 9);
            }

            Texture* hatchTex = ResourceLibrary::GetTexture("hatch_texture");
            if (hatchTex)
            {
                hatchTex->Bind(10);
                shader->SetIntUniform("hatchTexture", 10);
            }

            m_cloudBufferUpsampled->GetColourAttachment(0).Bind(11);
            shader->SetIntUniform("cloudTexture", 11);
            shader->SetBoolUniform("hasCloudTexture", true);

            auto camera = m_cameraModule->GetMainCamera();

            //PostProcessComponent* postProcess = nullptr;
            //if (camera.has<PostProcessComponent>())
            //{
            //    postProcess = camera.try_get_mut<PostProcessComponent>();
            //}
            auto postProcess = camera.try_get<PostProcessComponent>();
            if (!postProcess)
            {
                auto childQuery = m_world.query_builder<PostProcessComponent>()
                    .with(flecs::ChildOf, camera)
                    .build();

                childQuery.each([&postProcess](flecs::entity e, PostProcessComponent& pp)
                    {
                        if (pp.enabled) postProcess = &pp;
                    });
            }


            // This is causing significant drop in framerate. Switch to PostProcess UBO later.
            if (postProcess && postProcess->enabled)
            {
                // Triggers on observers.
                //UpdatePostProcessUBO(*postProcess);

                //shader->SetBoolUniform("enableFog", postProcess->enableFog);
                //shader->SetVec3Uniform("fogExtinction", postProcess->fogExtinction);
                //shader->SetVec3Uniform("fogInscatter", postProcess->fogInscatter);
                //shader->SetVec3Uniform("fogBaseColour", postProcess->fogBaseColour);
                //shader->SetVec3Uniform("fogSunColour", postProcess->fogSunColour);
                //shader->SetFloatUniform("fogSunPower", postProcess->fogSunPower);
                //shader->SetFloatUniform("fogHeight", postProcess->fogHeight);
                //shader->SetFloatUniform("fogHeightFalloff", postProcess->fogHeightFalloff);
                //shader->SetFloatUniform("fogHeightDensity", postProcess->fogHeightDensity);

                //shader->SetFloatUniform("colourFactor", postProcess->colourFactor);
                //shader->SetFloatUniform("brightnessFactor", postProcess->brightnessFactor);
                //shader->SetBoolUniform("enableHDR", postProcess->enableHDR);
                //shader->SetFloatUniform("exposure", postProcess->exposure);

                //shader->SetBoolUniform("enableHSV", postProcess->enableHSV);
                //shader->SetFloatUniform("hueShift", postProcess->hueShift);
                //shader->SetFloatUniform("saturationAdjust", postProcess->saturationAdjust);
                //shader->SetFloatUniform("valueAdjust", postProcess->valueAdjust);

                //shader->SetBoolUniform("enableEdgeDetection", postProcess->enableEdgeDetection);
                //shader->SetBoolUniform("showEdgesOnly", postProcess->showEdgesOnly);
                //shader->SetFloatUniform("edgeStrength", postProcess->edgeStrength);
                //shader->SetFloatUniform("depthThreshold", postProcess->depthThreshold);
                //shader->SetFloatUniform("normalThreshold", postProcess->normalThreshold);
                //shader->SetFloatUniform("darkenAmount", postProcess->darkenAmount);
                //shader->SetFloatUniform("lightenAmount", postProcess->lightenAmount);
                //shader->SetVec3Uniform("normalBias", postProcess->normalBias);
                //shader->SetFloatUniform("edgeWidth", postProcess->edgeWidth);

                //shader->SetBoolUniform("enableQuantization", postProcess->enableQuantization);
                //shader->SetIntUniform("quantizationLevels", postProcess->quantizationLevels);
                //shader->SetBoolUniform("enableDithering", postProcess->enableDithering);
                //shader->SetFloatUniform("ditherStrength", postProcess->ditherStrength);

                if (postProcess->enableDithering && !postProcess->ditherTexture.empty())
                {
                    Texture* ditherTex = ResourceLibrary::GetTexture(postProcess->ditherTexture.c_str());
                    if (ditherTex)
                    {
                        ditherTex->Bind(6);
                        shader->SetIntUniform("ditherTexture", 6);
                    }
                }
                else if (postProcess->enableDithering)
                {
                    Texture* defaultDitherTex = ResourceLibrary::GetTexture("bayer_4");
                    if (defaultDitherTex)
                    {
                        defaultDitherTex->Bind(6);
                        shader->SetIntUniform("ditherTexture", 6);
                    }
                }
            }

            m_screenTarget->Render();
        }
        m_postProcessBuffer->Unbind();
    }

    void RenderLayer::RenderFinalPass()
    {
        Application& app = Application::Get();
        glm::vec2 screenSize = app.GetWindowSize();

        auto camera = m_cameraModule->GetMainCamera();
        auto camComp = camera.try_get_mut<CameraComponent>();

        glm::vec2 resolution = camComp->resolution;
        //glm::vec2 resolution = app.GetGameViewportSize();

        if (camComp->smoothDisplay)
        {
            glm::vec2 pixelOffset = m_cameraModule->GetPixelOffset(camera);
            glViewport((GLint)pixelOffset.x, (GLint)pixelOffset.y, (GLsizei)resolution.x, (GLsizei)resolution.y);
        }
        else
        {
            glViewport(0, 0, (GLsizei)resolution.x, (GLsizei)resolution.y);
        }

        if (m_isGameView)
        {
            m_physicsModule->SetDebugDrawEnabled(false);

            if (InputSystem::Inst() != nullptr && m_postProcessBuffer != nullptr)
            {
                InputSystem::Inst()->SetGameWindowInfo(
                    0,
                    0,
                    (int)screenSize.x,
                    (int)screenSize.y,
                    m_postProcessBuffer->GetWidth(),
                    m_postProcessBuffer->GetHeight()
                );
            }

            if (m_debugVisualisation == BufferVisualisation::Final)
            {

                auto shader = ResourceLibrary::GetShader("final_buffer");
                shader->Use();
                const Texture& postProcessTexture = m_postProcessBuffer->GetColourAttachment(0);
                postProcessTexture.Bind(0);
                shader->SetIntUniform("inputTexture", 0);
            }
            else
            {
                RenderDebugBuffer();
            }

            m_screenTarget->Render();
            glViewport(0, 0, (GLsizei)resolution.x, (GLsizei)resolution.y);
        }
    }

    void RenderLayer::RenderDebugBuffer()
    {
        auto shader = ResourceLibrary::GetShader("debug_buffer");
        shader->Use();

        shader->SetIntUniform("visualisationMode", (int)m_debugVisualisation);
        shader->SetIntUniform("channelMode", m_bufferVisInfo.visualiseChannel);
        shader->SetFloatUniform("depthScale", m_bufferVisInfo.depthVisualisationScale);
        shader->SetFloatUniform("positionScale", m_bufferVisInfo.positionVisualisationScale);
        shader->SetBoolUniform("normaliseValues", m_bufferVisInfo.normaliseVisualisation);
        shader->SetBoolUniform("colouriseIDs", m_bufferVisInfo.colouriseEntityIDs);

        switch (m_debugVisualisation)
        {
        case BufferVisualisation::GBuffer_Albedo:
            m_gBuffer->GetColourAttachment(0).Bind(0);
            break;

        case BufferVisualisation::GBuffer_Normal:
            m_gBuffer->GetColourAttachment(1).Bind(0);
            break;

        case BufferVisualisation::GBuffer_Position:
            m_gBuffer->GetColourAttachment(2).Bind(0);
            break;

        case BufferVisualisation::GBuffer_Material:
            m_gBuffer->GetColourAttachment(3).Bind(0);
            break;

        case BufferVisualisation::GBuffer_EntityID:
            m_gBuffer->GetColourAttachment(4).Bind(0);
            shader->SetIntUniform("inputTextureUInt", 0);
            break;

        case BufferVisualisation::GBuffer_Depth:
            m_gBuffer->GetDepthAttachment().Bind(0);
            break;

        case BufferVisualisation::GBuffer_Stencil:
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_stencilView);
            break;

        case BufferVisualisation::Backface_Position:
            m_backfaceBuffer->GetColourAttachment(0).Bind(0);
            break;

        case BufferVisualisation::Lighting_Colour:
            m_lightingBuffer->GetColourAttachment(0).Bind(0);
            break;

        case BufferVisualisation::Lighting_Data:
            m_lightingBuffer->GetColourAttachment(1).Bind(0);
            break;

        case BufferVisualisation::AO:
            m_aoBuffer->GetColourAttachment(0).Bind(0);
            break;

        case BufferVisualisation::AO_Blurred:
            m_blurBuffer->GetColourAttachment(0).Bind(0);
            break;

        case BufferVisualisation::Shadow:
            m_shadowBuffer->GetDepthAttachment().Bind(0);
            break;

        case BufferVisualisation::PostProcess:
            m_postProcessBuffer->GetColourAttachment(0).Bind(0);
            break;

        default:
            m_gBuffer->GetColourAttachment(0).Bind(0);
            break;
        }

        shader->SetIntUniform("inputTexture", 0);
    }

    flecs::entity RenderLayer::PickEntity(int mouseX, int mouseY)
    {
        if (!m_gBuffer) return flecs::entity();

        int bufferWidth = m_gBuffer->GetWidth();
        int bufferHeight = m_gBuffer->GetHeight();

        if (mouseX < 0 || mouseX >= bufferWidth || mouseY < 0 || mouseY >= bufferHeight)
        {
            return flecs::entity();
        }

        glm::uvec2 ReadEntityId = { 0, 0 };

        m_gBuffer->Bind();
        glReadBuffer(GL_COLOR_ATTACHMENT4);

        glReadPixels(mouseX, mouseY, 1, 1, GL_RG_INTEGER, GL_UNSIGNED_INT, &ReadEntityId);

        m_gBuffer->Unbind();

        uint64_t entityId = ((uint64_t)ReadEntityId.y << 32) | (uint64_t)ReadEntityId.x;

        if (entityId != 0)
        {
            return m_world.entity(static_cast<flecs::entity_t>(entityId));
        }

        return flecs::entity();
    }

    ImVec2 RenderLayer::GetBufferSize(BufferVisualisation vis)
    {
        switch (vis)
        {
        case BufferVisualisation::Shadow:
            return ImVec2(SHADOW_WIDTH, SHADOW_HEIGHT);
        default:
            return ImVec2(m_gBuffer->GetWidth(), m_gBuffer->GetHeight());
        }
    }

    const char* RenderLayer::GetBufferFormat(BufferVisualisation vis)
    {
        switch (vis)
        {
        case BufferVisualisation::GBuffer_Albedo:
        case BufferVisualisation::GBuffer_Material:
            return "RGBA8 UNORM";
        case BufferVisualisation::GBuffer_Normal:
        case BufferVisualisation::GBuffer_Position:
        case BufferVisualisation::Backface_Position:
            return "RGBA16F";
        case BufferVisualisation::GBuffer_EntityID:
            return "RG32UI";
        case BufferVisualisation::GBuffer_Depth:
            return "D24S8 (Depth)";
        case BufferVisualisation::GBuffer_Stencil:
            return "D24S8 (Stencil)";
        case BufferVisualisation::Shadow:
            return "D24S8";
        case BufferVisualisation::AO:
        case BufferVisualisation::AO_Blurred:
            return "R16F";
        default:
            return "Unknown";
        }
    }

    const Texture* RenderLayer::GetDebugTexture()
    {
        if (m_debugVisualisation == BufferVisualisation::Final)
        {
            return &m_postProcessBuffer->GetColourAttachment(0);
        }

        m_debugBuffer->Bind();
        glClear(GL_COLOR_BUFFER_BIT);

        RenderDebugBuffer();
        m_screenTarget->Render();

        m_debugBuffer->Unbind();

        return &m_debugBuffer->GetColourAttachment(0);
    }

    void SetWireframeMode(bool enabled)
    {
        if (enabled)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
}