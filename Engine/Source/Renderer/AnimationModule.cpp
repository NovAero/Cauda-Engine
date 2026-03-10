#include "cepch.h"
#include "AnimationModule.h"
#include "SkeletalMesh.h"
#include "RendererModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include <iostream>

using namespace Math;

void MultiplySoATransformQuaternion(int _index, const ozz::math::SimdQuaternion& _quat,
    const ozz::span<ozz::math::SoaTransform>& _transforms)
{
    assert(_index >= 0 && static_cast<size_t>(_index) < _transforms.size() * 4);

    ozz::math::SoaTransform& soa_transform_ref = _transforms[_index / 4];
    ozz::math::SimdQuaternion aos_quats[4];
    ozz::math::Transpose4x4(&soa_transform_ref.rotation.x, &aos_quats->xyzw);

    ozz::math::SimdQuaternion& aos_quat_ref = aos_quats[_index & 3];
    aos_quat_ref = aos_quat_ref * _quat;

    ozz::math::Transpose4x4(&aos_quats->xyzw, &soa_transform_ref.rotation.x);
}

AnimationLayer::AnimationLayer(const AnimationLayer& other)
    : name(other.name)
    , currentAnimation(other.currentAnimation)
    , animationTime(other.animationTime)
    , isPlaying(other.isPlaying)
    , isLooping(other.isLooping)
    , playbackSpeed(other.playbackSpeed)
    , weight(other.weight)
    , isAdditive(other.isAdditive)
    , transition(other.transition)
{
}

AnimationLayer& AnimationLayer::operator=(const AnimationLayer& other)
{
    if (this != &other)
    {
        name = other.name;
        currentAnimation = other.currentAnimation;
        animationTime = other.animationTime;
        isPlaying = other.isPlaying;
        isLooping = other.isLooping;
        playbackSpeed = other.playbackSpeed;
        weight = other.weight;
        isAdditive = other.isAdditive;
        transition = other.transition;

        currentContext.reset();
        currentTransforms.reset();
        transitionContext.reset();
        transitionTransforms.reset();
        jointWeights.reset();
    }
    return *this;
}

AnimationComponent::AnimationComponent(const AnimationComponent& other)
    : layers(other.layers)
    , boneTransforms(other.boneTransforms)
    , boneSSBO(0)
    , ikChains(other.ikChains)
    , aimIKs(other.aimIKs)
    , initialised(false)
    , needsReinit(true)
{
}

AnimationComponent& AnimationComponent::operator=(const AnimationComponent& other)
{
    if (this != &other)
    {
        layers = other.layers;
        boneTransforms = other.boneTransforms;
        ikChains = other.ikChains;
        aimIKs = other.aimIKs;
        initialised = false;
        needsReinit = true;

        blendedTransforms.reset();
        modelMatrices.reset();
    }
    return *this;
}

AnimationComponent::~AnimationComponent()
{
    if (boneSSBO != 0)
    {
        glDeleteBuffers(1, &boneSSBO);
        boneSSBO = 0;
    }
}

AnimationModule::AnimationModule(flecs::world& world) : m_world(world)
{
    m_editorModule = m_world.try_get_mut<EditorModule>();

    SetupComponents();
    SetupSystems();
    SetupObservers();
    SetupQueries();
    RegisterWithEditor();
}

AnimationModule::~AnimationModule()
{
    if (m_animationSystem && m_animationSystem.is_alive())
        m_animationSystem.destruct();
    if (m_animationRemoveObserver && m_animationRemoveObserver.is_alive())
        m_animationRemoveObserver.destruct();

    // Shift this clearing ssbo logic to an onDelete/onRemove observer later
    //m_animationQuery.each([](flecs::entity e, AnimationComponent& state,
    //    const SkeletalMeshComponent& meshComp)
    //    {
    //        if (state.boneSSBO != 0)
    //        {
    //            glDeleteBuffers(1, &state.boneSSBO);
    //            state.boneSSBO = 0;
    //        }
    //    });
}

void AnimationModule::SetupComponents()
{
    m_world.component<AnimationComponent>("AnimationComponent");
}

void AnimationModule::SetupSystems()
{
    m_animationSystem = m_world.system<AnimationComponent, SkeletalMeshComponent>("AnimateSkeletalMeshes")
        .kind(flecs::PreStore)
        .each([this](flecs::entity entity, AnimationComponent& state, const SkeletalMeshComponent& meshComp)
            {
                if (!meshComp.visible || meshComp.handle == "") return;

                SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
                if (!mesh) return;

                if (!state.initialised || state.needsReinit)
                {
                    InitialiseAnimationState(entity, state, meshComp);
                }

                if (!state.layers.empty())
                {
                    UpdateAnimation(entity, state, meshComp, m_world.delta_time());
                }

                // Probably better to update bone transforms in PreStore phase.
                if (!state.modelMatrices) return;

                UpdateBoneTransforms(state, meshComp);
                UploadBoneTransforms(state);
            });

    //m_world.system<AnimationComponent, const SkeletalMeshComponent>()
    //    .kind(flecs::OnStore)
    //    .each([this](flecs::entity entity, AnimationComponent& state, const SkeletalMeshComponent& meshComp)
    //        {
    //            //if (!meshComp.visible || !state.modelMatrices)
    //            //    return;

    //            //UpdateBoneTransforms(state, meshComp);
    //            //UploadBoneTransforms(state);
    //        });
}

void AnimationModule::SetupObservers()
{
    m_animationRemoveObserver = m_world.observer<AnimationComponent>()
        .event(flecs::OnRemove)
        .event(flecs::OnDelete)
        .each([this](flecs::entity entity, AnimationComponent& state)
            {
                if (state.boneSSBO != 0)
                {
                    glDeleteBuffers(1, &state.boneSSBO);
                    state.boneSSBO = 0;
                }
            });
}

void AnimationModule::SetupQueries()
{
    //m_animationQuery = m_world.query_builder<AnimationComponent, const SkeletalMeshComponent>().build();
}


void AnimationModule::InitialiseAnimationState(flecs::entity entity,
    AnimationComponent& state,
    const SkeletalMeshComponent& meshComp)
{
    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh || !mesh->GetSkeleton())
        return;

    const auto* skeleton = mesh->GetSkeleton();
    const int num_joints = skeleton->num_joints();
    const int num_soa_joints = skeleton->num_soa_joints();

    state.blendedTransforms = std::make_unique<ozz::vector<ozz::math::SoaTransform>>();
    state.modelMatrices = std::make_unique<ozz::vector<ozz::math::Float4x4>>();

    state.blendedTransforms->resize(num_soa_joints);
    state.modelMatrices->resize(num_joints);

    state.boneTransforms.resize(SkeletalMesh::MAX_BONES, glm::mat4(1.0f));

    if (state.boneSSBO == 0)
    {
        glGenBuffers(1, &state.boneSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, state.boneSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
            SkeletalMesh::MAX_BONES * sizeof(glm::mat4),
            state.boneTransforms.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    if (state.layers.empty())
    {
        AnimationLayer layer;
        layer.name = "Locomotion";
        layer.weight = 1.0f;
        ReinitialiseLayer(layer, skeleton);
        state.layers.push_back(std::move(layer));
    }


    if (state.needsReinit)
    {
        for (auto& layer : state.layers)
        {
            ReinitialiseLayer(layer, skeleton);
        }
        state.needsReinit = false;
    }

    InitialiseRestPose(state, skeleton);
    state.initialised = true;
}

void AnimationModule::ReinitialiseLayer(AnimationLayer& layer,
    const ozz::animation::Skeleton* skeleton)
{
    if (!skeleton)
        return;

    const int num_joints = skeleton->num_joints();
    const int num_soa_joints = skeleton->num_soa_joints();

    layer.currentContext = std::make_unique<ozz::animation::SamplingJob::Context>();
    layer.currentTransforms = std::make_unique<ozz::vector<ozz::math::SoaTransform>>();
    layer.jointWeights = std::make_unique<ozz::vector<ozz::math::SimdFloat4>>();

    layer.currentContext->Resize(num_joints);
    layer.currentTransforms->resize(num_soa_joints);
    layer.jointWeights->resize(num_soa_joints, ozz::math::simd_float4::one());

    layer.transitionContext = std::make_unique<ozz::animation::SamplingJob::Context>();
    layer.transitionTransforms = std::make_unique<ozz::vector<ozz::math::SoaTransform>>();
    layer.transitionContext->Resize(num_joints);
    layer.transitionTransforms->resize(num_soa_joints);
}

void AnimationModule::InitialiseRestPose(AnimationComponent& state,
    const ozz::animation::Skeleton* skeleton)
{
    if (!skeleton) return;

    const int num_soa_joints = skeleton->num_soa_joints();

    if (!state.blendedTransforms)
    {
        state.blendedTransforms = std::make_unique<ozz::vector<ozz::math::SoaTransform>>();
        state.blendedTransforms->resize(num_soa_joints);
    }

    if (!state.modelMatrices)
    {
        state.modelMatrices = std::make_unique<ozz::vector<ozz::math::Float4x4>>();
        state.modelMatrices->resize(skeleton->num_joints());
    }

    const auto& rest_pose = skeleton->joint_rest_poses();
    for (int i = 0; i < num_soa_joints; ++i)
    {
        (*state.blendedTransforms)[i] = rest_pose[i];
    }

    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = skeleton;
    ltmJob.input = ozz::make_span(*state.blendedTransforms);
    ltmJob.output = ozz::make_span(*state.modelMatrices);

    if (!ltmJob.Run())
    {
        std::cerr << "Failed to compute rest pose model matrices" << std::endl;
    }
}

void AnimationModule::UpdateAnimation(flecs::entity entity,
    AnimationComponent& state,
    const SkeletalMeshComponent& meshComp,
    float deltaTime)
{
    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh || !mesh->GetSkeleton())
        return;

    const auto* skeleton = mesh->GetSkeleton();

    bool hasActiveAnimation = false;
    for (const auto& layer : state.layers)
    {
        if (layer.isPlaying && !layer.currentAnimation.empty())
        {
            hasActiveAnimation = true;
            break;
        }
    }

    if (!hasActiveAnimation || state.layers.empty())
    {
        InitialiseRestPose(state, skeleton);
        if (!state.ikChains.empty() || !state.aimIKs.empty())
        {
            ApplyIK(state, meshComp);
        }
        return;
    }

    for (auto& layer : state.layers)
    {
        if (layer.isPlaying)
        {
            UpdateLayer(layer, meshComp, deltaTime);
        }
    }

    BlendLayers(state, meshComp);

    if (!state.ikChains.empty() || !state.aimIKs.empty())
    {
        ApplyIK(state, meshComp);
    }
}

void AnimationModule::UpdateLayer(AnimationLayer& layer,
    const SkeletalMeshComponent& meshComp,
    float deltaTime)
{
    if (layer.currentAnimation.empty())
        return;


    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh)
        return;

    if (layer.transition.isActive)
    {
        layer.transition.currentTime += deltaTime;
        float t = glm::clamp(layer.transition.currentTime / layer.transition.duration, 0.0f, 1.0f);

        if (t >= 1.0f)
        {
            layer.transition.isActive = false;
        }

        SampleAnimation(layer.transition.sourceAnimation, layer.transition.sourceTime,
            layer.transitionContext.get(), layer.transitionTransforms.get(), meshComp);

        SampleAnimation(layer.currentAnimation, layer.animationTime,
            layer.currentContext.get(), layer.currentTransforms.get(), meshComp);

        const int num_soa = layer.currentTransforms->size();
        for (int i = 0; i < num_soa; ++i)
        {
            (*layer.currentTransforms)[i].translation = ozz::math::Lerp(
                (*layer.transitionTransforms)[i].translation,
                (*layer.currentTransforms)[i].translation,
                ozz::math::simd_float4::Load1(t));

            (*layer.currentTransforms)[i].rotation = ozz::math::NLerp(
                (*layer.transitionTransforms)[i].rotation,
                (*layer.currentTransforms)[i].rotation,
                ozz::math::simd_float4::Load1(t));

            (*layer.currentTransforms)[i].scale = ozz::math::Lerp(
                (*layer.transitionTransforms)[i].scale,
                (*layer.currentTransforms)[i].scale,
                ozz::math::simd_float4::Load1(t));
        }

        const ozz::animation::Animation* sourceAnim = mesh->GetAnimation(layer.transition.sourceAnimation);
        if (sourceAnim && layer.isLooping)
        {
            layer.transition.sourceTime += deltaTime * layer.playbackSpeed;
            if (layer.transition.sourceTime >= sourceAnim->duration())
            {
                layer.transition.sourceTime = fmod(layer.transition.sourceTime, sourceAnim->duration());
            }
        }
    }
    else
    {
        SampleAnimation(layer.currentAnimation, layer.animationTime,
            layer.currentContext.get(), layer.currentTransforms.get(), meshComp);
    }

    const ozz::animation::Animation* animation = mesh->GetAnimation(layer.currentAnimation);
    if (animation)
    {
        layer.animationTime += deltaTime * layer.playbackSpeed;
        const float duration = animation->duration();

        if (layer.animationTime >= duration)
        {
            if (layer.isLooping)
            {
                layer.animationTime = fmod(layer.animationTime, duration);
            }
            else
            {
                layer.animationTime = duration;
                layer.isPlaying = false;
            }
        }
    }
}

void AnimationModule::SampleAnimation(const std::string& animName, float time,
    ozz::animation::SamplingJob::Context* context,
    ozz::vector<ozz::math::SoaTransform>* output,
    const SkeletalMeshComponent& meshComp)
{
    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh)
        return;

    const ozz::animation::Animation* animation = mesh->GetAnimation(animName);
    if (!animation)
        return;

    float sample_ratio = time / animation->duration();
    sample_ratio = glm::clamp(sample_ratio, 0.0f, 1.0f);

    ozz::animation::SamplingJob samplingJob;
    samplingJob.animation = animation;
    samplingJob.context = context;
    samplingJob.ratio = sample_ratio;
    samplingJob.output = ozz::make_span(*output);

    if (!samplingJob.Run())
    {
        std::cerr << "SamplingJob failed for: " << animName << std::endl;
    }
}

void AnimationModule::BlendLayers(AnimationComponent& state,
    const SkeletalMeshComponent& meshComp)
{
    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh || !mesh->GetSkeleton())
        return;

    const auto* skeleton = mesh->GetSkeleton();

    std::vector<ozz::animation::BlendingJob::Layer> base_layers;
    std::vector<ozz::animation::BlendingJob::Layer> additive_layers;

    for (auto& layer : state.layers)
    {
        if (layer.weight <= 0.0f || !layer.isPlaying)
            continue;

        ozz::animation::BlendingJob::Layer blend_layer;
        blend_layer.transform = ozz::make_span(*layer.currentTransforms);
        blend_layer.weight = layer.weight;

        if (layer.jointWeights)
        {
            blend_layer.joint_weights = ozz::make_span(*layer.jointWeights);
        }

        if (layer.isAdditive)
        {
            additive_layers.push_back(blend_layer);
        }
        else
        {
            base_layers.push_back(blend_layer);
        }
    }

    if (base_layers.empty() && additive_layers.empty())
    {
        const int num_soa_joints = skeleton->num_soa_joints();
        const auto& rest_pose_span = skeleton->joint_rest_poses();
        for (int i = 0; i < num_soa_joints; ++i)
        {
            (*state.blendedTransforms)[i] = rest_pose_span[i];
        }
    }
    else
    {
        ozz::animation::BlendingJob blend_job;
        blend_job.threshold = 0.1f;
        blend_job.rest_pose = skeleton->joint_rest_poses();
        blend_job.output = ozz::make_span(*state.blendedTransforms);
        blend_job.layers = ozz::make_span(base_layers);
        blend_job.additive_layers = ozz::make_span(additive_layers);

        if (!blend_job.Run())
        {
            std::cerr << "Blending job failed" << std::endl;
            return;
        }
    }

    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = skeleton;
    ltmJob.input = ozz::make_span(*state.blendedTransforms);
    ltmJob.output = ozz::make_span(*state.modelMatrices);

    if (!ltmJob.Run())
    {
        std::cerr << "Local to model conversion failed" << std::endl;
    }
}

int AnimationModule::AddLayer(flecs::entity entity, const std::string& name)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    auto* meshComp = entity.try_get<SkeletalMeshComponent>();

    if (!state || !meshComp)
        return -1;

    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
    if (!mesh || !mesh->GetSkeleton())
        return -1;

    const auto* skeleton = mesh->GetSkeleton();

    AnimationLayer layer;
    layer.name = name;
    layer.weight = 1.0f;


    ReinitialiseLayer(layer, skeleton);

    state->layers.push_back(std::move(layer));

    std::cout << "Added layer: " << name << std::endl;
    return static_cast<int>(state->layers.size() - 1);
}

void AnimationModule::RemoveLayer(flecs::entity entity, int layerIndex)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    state->layers.erase(state->layers.begin() + layerIndex);
}

void AnimationModule::SetLayerWeight(flecs::entity entity, int layerIndex, float weight)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    state->layers[layerIndex].weight = glm::clamp(weight, 0.0f, 1.0f);
}

void AnimationModule::SetLayerAdditive(flecs::entity entity, int layerIndex, bool additive)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    state->layers[layerIndex].isAdditive = additive;
}

void AnimationModule::SetLayerSpeed(flecs::entity entity, int layerIndex, float speed)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    state->layers[layerIndex].playbackSpeed = speed;
}

void AnimationModule::PlayAnimation(flecs::entity entity, int layerIndex,
    const std::string& animName, bool loop, float transitionTime)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    auto& layer = state->layers[layerIndex];

    if (layer.currentAnimation == animName)
    {
        layer.animationTime = 0.0f;
        layer.isPlaying = true;
        layer.isLooping = loop;
        return;
    }

    if (!layer.currentAnimation.empty() && transitionTime > 0.0f)
    {
        layer.transition.sourceAnimation = layer.currentAnimation;
        layer.transition.sourceTime = layer.animationTime;
        layer.transition.targetAnimation = animName;
        layer.transition.duration = transitionTime;
        layer.transition.currentTime = 0.0f;
        layer.transition.isActive = true;
    }

    layer.currentAnimation = animName;
    layer.animationTime = 0.0f;
    layer.isPlaying = true;
    layer.isLooping = loop;
}

void AnimationModule::PauseLayer(flecs::entity entity, int layerIndex)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    state->layers[layerIndex].isPlaying = false;
}

void AnimationModule::ResumeLayer(flecs::entity entity, int layerIndex)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    state->layers[layerIndex].isPlaying = true;
}

void AnimationModule::StopLayer(flecs::entity entity, int layerIndex)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (!state || layerIndex < 0 || layerIndex >= state->layers.size())
        return;

    auto& layer = state->layers[layerIndex];
    layer.isPlaying = false;
    layer.animationTime = 0.0f;
    layer.transition.isActive = false;
}


void AnimationModule::ApplyIK(AnimationComponent& state, const SkeletalMeshComponent& meshComp)
{
    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh)
        return;

    const auto* skeleton = mesh->GetSkeleton();
    if (!skeleton || !state.blendedTransforms || !state.modelMatrices)
        return;

    for (auto& chain : state.ikChains)
    {
        if (!chain.enabled || chain.weight <= 0.0f)
            continue;

        ozz::math::SimdFloat4 target = ozz::math::simd_float4::Load3PtrU(&chain.target.x);
        ozz::math::SimdFloat4 pole = ozz::math::simd_float4::Load3PtrU(&chain.poleVector.x);

        ozz::animation::IKTwoBoneJob ikJob;
        ikJob.target = target;
        ikJob.pole_vector = pole;
        ikJob.mid_axis = ozz::math::simd_float4::y_axis();
        ikJob.weight = chain.weight;
        ikJob.soften = state.ikSoften;

        ikJob.start_joint = &(*state.modelMatrices)[chain.startJoint];
        ikJob.mid_joint = &(*state.modelMatrices)[chain.midJoint];
        ikJob.end_joint = &(*state.modelMatrices)[chain.endJoint];

        ozz::math::SimdQuaternion start_correction;
        ozz::math::SimdQuaternion mid_correction;
        ikJob.start_joint_correction = &start_correction;
        ikJob.mid_joint_correction = &mid_correction;

        if (!ikJob.Run())
            continue;

        MultiplySoATransformQuaternion(chain.startJoint, start_correction,
            ozz::make_span(*state.blendedTransforms));
        MultiplySoATransformQuaternion(chain.midJoint, mid_correction,
            ozz::make_span(*state.blendedTransforms));
    }

    for (auto& aim : state.aimIKs)
    {
        if (!aim.enabled || aim.weight <= 0.0f)
            continue;

        ozz::math::SimdFloat4 target = ozz::math::simd_float4::Load3PtrU(&aim.target.x);
        ozz::math::SimdFloat4 forward = ozz::math::simd_float4::Load3PtrU(&aim.forward.x);
        ozz::math::SimdFloat4 up = ozz::math::simd_float4::Load3PtrU(&aim.up.x);

        ozz::animation::IKAimJob aimJob;
        aimJob.target = target;
        aimJob.forward = forward;
        aimJob.up = up;
        aimJob.weight = aim.weight;
        aimJob.joint = &(*state.modelMatrices)[aim.joint];

        ozz::math::SimdQuaternion correction;
        aimJob.joint_correction = &correction;

        if (!aimJob.Run())
            continue;

        MultiplySoATransformQuaternion(aim.joint, correction,
            ozz::make_span(*state.blendedTransforms));
    }

    ozz::animation::LocalToModelJob ltmJob;
    ltmJob.skeleton = skeleton;
    ltmJob.input = ozz::make_span(*state.blendedTransforms);
    ltmJob.output = ozz::make_span(*state.modelMatrices);
    ltmJob.Run();
}

int AnimationModule::AddIKChain(flecs::entity entity, const std::string& name,
    const std::string& startJoint, const std::string& midJoint,
    const std::string& endJoint)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    auto* meshComp = entity.try_get<SkeletalMeshComponent>();

    if (!state || !meshComp)
        return -1;

    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
    if (!mesh)
        return -1;

    IKChain chain;
    chain.name = name;
    chain.startJoint = mesh->GetJointIndex(startJoint);
    chain.midJoint = mesh->GetJointIndex(midJoint);
    chain.endJoint = mesh->GetJointIndex(endJoint);

    if (chain.startJoint < 0 || chain.midJoint < 0 || chain.endJoint < 0)
    {
        std::cerr << "Failed to create IK chain '" << name << "': Invalid joints" << std::endl;
        return -1;
    }

    state->ikChains.push_back(chain);
    return static_cast<int>(state->ikChains.size() - 1);
}

void AnimationModule::SetIKTarget(flecs::entity entity, int chainIndex, const glm::vec3& target)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && chainIndex >= 0 && chainIndex < state->ikChains.size())
        state->ikChains[chainIndex].target = target;
}

void AnimationModule::SetIKPoleVector(flecs::entity entity, int chainIndex, const glm::vec3& pole)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && chainIndex >= 0 && chainIndex < state->ikChains.size())
        state->ikChains[chainIndex].poleVector = pole;
}

void AnimationModule::SetIKWeight(flecs::entity entity, int chainIndex, float weight)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && chainIndex >= 0 && chainIndex < state->ikChains.size())
        state->ikChains[chainIndex].weight = glm::clamp(weight, 0.0f, 1.0f);
}

void AnimationModule::EnableIK(flecs::entity entity, int chainIndex, bool enabled)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && chainIndex >= 0 && chainIndex < state->ikChains.size())
        state->ikChains[chainIndex].enabled = enabled;
}

int AnimationModule::AddAimIK(flecs::entity entity, const std::string& name,
    const std::string& joint)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    auto* meshComp = entity.try_get<SkeletalMeshComponent>();

    if (!state || !meshComp)
        return -1;

    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
    if (!mesh)
        return -1;

    AimIK aim;
    aim.name = name;
    aim.joint = mesh->GetJointIndex(joint);

    if (aim.joint < 0)
    {
        std::cerr << "Failed to create Aim IK '" << name << "': Invalid joint" << std::endl;
        return -1;
    }

    state->aimIKs.push_back(aim);
    return static_cast<int>(state->aimIKs.size() - 1);
}

void AnimationModule::SetAimTarget(flecs::entity entity, int aimIndex, const glm::vec3& target)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && aimIndex >= 0 && aimIndex < state->aimIKs.size())
        state->aimIKs[aimIndex].target = target;
}

void AnimationModule::SetAimWeight(flecs::entity entity, int aimIndex, float weight)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && aimIndex >= 0 && aimIndex < state->aimIKs.size())
        state->aimIKs[aimIndex].weight = glm::clamp(weight, 0.0f, 1.0f);
}

void AnimationModule::EnableAim(flecs::entity entity, int aimIndex, bool enabled)
{
    auto* state = entity.try_get_mut<AnimationComponent>();
    if (state && aimIndex >= 0 && aimIndex < state->aimIKs.size())
        state->aimIKs[aimIndex].enabled = enabled;
}

glm::vec3 AnimationModule::GetBoneWorldPosition(const std::string& boneName,
    const AnimationComponent& anim,
    const SkeletalMeshComponent& meshComp,
    const TransformComponent& transform)
{
    if (!anim.modelMatrices)
        return transform.position;

    SkeletalMesh* skeletalMesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!skeletalMesh)
        return transform.position;

    int boneIndex = skeletalMesh->GetJointIndex(boneName);
    if (boneIndex < 0 || boneIndex >= anim.modelMatrices->size())
        return transform.position;

    glm::mat4 boneMatrix = OzzToGlm((*anim.modelMatrices)[boneIndex]);

    glm::mat4 worldTransform = glm::translate(glm::mat4(1.0f), transform.position) *
        glm::mat4_cast(transform.rotation) *
        glm::scale(glm::mat4(1.0f), transform.scale);

    glm::vec4 worldPos = worldTransform * boneMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    return glm::vec3(worldPos);
}

void AnimationModule::UpdateBoneTransforms(AnimationComponent& state,
    const SkeletalMeshComponent& meshComp)
{
    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp.handle.c_str());
    if (!mesh || !state.modelMatrices)
        return;

    const auto& boneOffsets = mesh->GetBoneOffsets();

    for (size_t i = 0; i < state.modelMatrices->size() && i < SkeletalMesh::MAX_BONES; ++i)
    {
        glm::mat4 modelMatrix = OzzToGlm((*state.modelMatrices)[i]);
        state.boneTransforms[i] = modelMatrix * boneOffsets[i];
    }
}

void AnimationModule::UploadBoneTransforms(AnimationComponent& state)
{
    if (state.boneSSBO == 0)
        return;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, state.boneSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
        state.boneTransforms.size() * sizeof(glm::mat4),
        state.boneTransforms.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

bool AnimationModule::HasAnimation(flecs::entity entity, const std::string& name) const
{
    auto* meshComp = entity.try_get<SkeletalMeshComponent>();
    if (!meshComp)
        return false;

    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
    if (!mesh)
        return false;

    return mesh->HasAnimation(name);
}

std::vector<std::string> AnimationModule::GetAnimationNames(flecs::entity entity) const
{
    auto* meshComp = entity.try_get<SkeletalMeshComponent>();
    if (!meshComp)
        return {};

    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
    if (!mesh)
        return {};

    return mesh->GetAnimationNames();
}

float AnimationModule::GetAnimationDuration(flecs::entity entity, const std::string& animName) const
{
    auto* meshComp = entity.try_get<SkeletalMeshComponent>();
    if (!meshComp)
        return 0.0f;

    SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
    if (!mesh)
        return 0.0f;

    const auto* anim = mesh->GetAnimation(animName);
    return anim ? anim->duration() : 0.0f;
}

int AnimationModule::GetLayerCount(flecs::entity entity) const
{
    auto* state = entity.try_get<AnimationComponent>();
    return state ? static_cast<int>(state->layers.size()) : 0;
}

void AnimationModule::RegisterWithEditor()
{
    if (!m_editorModule)
        return;

    m_editorModule->RegisterComponent<AnimationComponent>(
        "Animation",
        "Animation",
        [this](flecs::entity entity, AnimationComponent& component)
        {
            DrawAnimationInspector(entity, component);
        }
    );
}

void AnimationModule::DrawAnimationInspector(flecs::entity entity, AnimationComponent& component)
{
    ImGui::PushID("AnimationInspector");

    if (ImGui::CollapsingHeader("Animation Layers", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("+ Add Layer"))
        {
            AddLayer(entity, "New Layer");
        }

        ImGui::Separator();

        for (size_t i = 0; i < component.layers.size(); ++i)
        {
            auto& layer = component.layers[i];
            ImGui::PushID(i);

            bool nodeOpen = ImGui::TreeNodeEx(layer.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            ImGui::SameLine();
            if (ImGui::SmallButton("X"))
            {
                RemoveLayer(entity, i);
                ImGui::PopID();
                break;
            }

            if (nodeOpen)
            {
                char nameBuffer[64];
                strncpy_s(nameBuffer, layer.name.c_str(), sizeof(nameBuffer) - 1);
                if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                {
                    layer.name = nameBuffer;
                }

                auto animNames = GetAnimationNames(entity);
                std::string currentName = layer.currentAnimation.empty() ? "None" : layer.currentAnimation;

                if (ImGui::BeginCombo("Animation", currentName.c_str()))
                {
                    for (const auto& name : animNames)
                    {
                        bool isSelected = (layer.currentAnimation == name);
                        if (ImGui::Selectable(name.c_str(), isSelected))
                        {
                            PlayAnimation(entity, i, name, layer.isLooping, 0.3f);
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Checkbox("Loop", &layer.isLooping);

                if (ImGui::Button("Play")) ResumeLayer(entity, i);
                ImGui::SameLine();
                if (ImGui::Button("Pause")) PauseLayer(entity, i);
                ImGui::SameLine();
                if (ImGui::Button("Stop")) StopLayer(entity, i);

                ImGui::Separator();
                ImGui::SliderFloat("Weight", &layer.weight, 0.0f, 1.0f);
                ImGui::SliderFloat("Speed", &layer.playbackSpeed, 0.0f, 3.0f);
                ImGui::Checkbox("Additive", &layer.isAdditive);

                if (!layer.currentAnimation.empty())
                {
                    float duration = GetAnimationDuration(entity, layer.currentAnimation);
                    ImGui::Text("Time: %.2f / %.2f", layer.animationTime, duration);

                    if (layer.transition.isActive)
                    {
                        ImGui::Text("Transitioning from: %s (%.1f%%)",
                            layer.transition.sourceAnimation.c_str(),
                            (1.0f - layer.transition.currentTime / layer.transition.duration) * 100.0f);
                    }
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    if (!component.ikChains.empty() && ImGui::CollapsingHeader("Manual IK Chains"))
    {
        for (size_t i = 0; i < component.ikChains.size(); ++i)
        {
            auto& chain = component.ikChains[i];

            if (chain.name.find("Leg_TwoBone") != std::string::npos)
                continue;

            ImGui::PushID(i);

            if (ImGui::TreeNode(chain.name.c_str()))
            {
                ImGui::Checkbox("Enabled", &chain.enabled);
                ImGui::SliderFloat("Weight", &chain.weight, 0.0f, 1.0f);
                ImGui::DragFloat3("Target", &chain.target.x, 0.01f);
                ImGui::DragFloat3("Pole", &chain.poleVector.x, 0.01f);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    if (!component.aimIKs.empty() && ImGui::CollapsingHeader("Manual Aim IKs"))
    {
        for (size_t i = 0; i < component.aimIKs.size(); ++i)
        {
            auto& aim = component.aimIKs[i];

            if (aim.name.find("Ankle_Aim") != std::string::npos)
                continue;

            ImGui::PushID(i);

            if (ImGui::TreeNode(aim.name.c_str()))
            {
                ImGui::Checkbox("Enabled", &aim.enabled);
                ImGui::SliderFloat("Weight", &aim.weight, 0.0f, 1.0f);
                ImGui::DragFloat3("Target", &aim.target.x, 0.01f);
                ImGui::DragFloat3("Forward", &aim.forward.x, 0.01f);
                ImGui::DragFloat3("Up", &aim.up.x, 0.01f);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    ImGui::PopID();
}