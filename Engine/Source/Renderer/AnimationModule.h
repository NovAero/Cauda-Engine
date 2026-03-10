#pragma once
#include "Renderer/Graphics.h"
#include <ThirdParty/flecs.h>
#include <glm/glm.hpp>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/ik_two_bone_job.h>
#include <ozz/animation/runtime/ik_aim_job.h>
#include <ozz/base/maths/simd_quaternion.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>

class EditorModule;
struct SkeletalMeshComponent;

using AnimationEventCallback = std::function<void(flecs::entity, const std::string&)>;

struct LayerTransition
{
    std::string targetAnimation;
    float duration = 0.3f;
    float currentTime = 0.0f;
    bool isActive = false;

    std::string sourceAnimation;
    float sourceTime = 0.0f;
};

struct AnimationLayer
{
    std::string name;

    std::string currentAnimation;
    float animationTime = 0.0f;
    bool isPlaying = true;
    bool isLooping = true;
    float playbackSpeed = 1.0f;

    float weight = 1.0f;
    bool isAdditive = false;

    LayerTransition transition;

    std::unique_ptr<ozz::animation::SamplingJob::Context> currentContext;
    std::unique_ptr<ozz::vector<ozz::math::SoaTransform>> currentTransforms;
    std::unique_ptr<ozz::animation::SamplingJob::Context> transitionContext;
    std::unique_ptr<ozz::vector<ozz::math::SoaTransform>> transitionTransforms;
    std::unique_ptr<ozz::vector<ozz::math::SimdFloat4>> jointWeights;

    AnimationLayer() = default;
    AnimationLayer(const AnimationLayer& other);
    AnimationLayer& operator=(const AnimationLayer& other);
    AnimationLayer(AnimationLayer&& other) noexcept = default;
    AnimationLayer& operator=(AnimationLayer&& other) noexcept = default;
};

struct IKChain
{
    std::string name;
    int startJoint = -1;
    int midJoint = -1;
    int endJoint = -1;
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 poleVector = glm::vec3(0.0f, 1.0f, 0.0f);
    float weight = 1.0f;
    bool enabled = false;
};

struct AimIK
{
    std::string name;
    int joint = -1;
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float weight = 1.0f;
    bool enabled = false;
};

struct AnimationComponent
{
    std::vector<AnimationLayer> layers;

    std::unique_ptr<ozz::vector<ozz::math::SoaTransform>> blendedTransforms;
    std::unique_ptr<ozz::vector<ozz::math::Float4x4>> modelMatrices;

    std::vector<glm::mat4> boneTransforms;
    GLuint boneSSBO = 0;

    std::vector<IKChain> ikChains;
    std::vector<AimIK> aimIKs;

    float ikWeight = 1.0f;
    float ikSoften = 0.97f;
    glm::vec3 kneePoleDirection = glm::vec3(0.0f, 0.0f, 1.0f);
    float poleVectorDistance = 1.0f;

    bool initialised = false;
    bool needsReinit = false;

    AnimationComponent() = default;
    AnimationComponent(const AnimationComponent& other);
    AnimationComponent& operator=(const AnimationComponent& other);
    ~AnimationComponent();
};

class AnimationModule
{
public:
    AnimationModule(flecs::world& world);
    ~AnimationModule();

    AnimationModule(const AnimationModule&) = delete;
    AnimationModule& operator=(const AnimationModule&) = delete;
    AnimationModule(AnimationModule&&) = default;
    AnimationModule& operator=(AnimationModule&&) = default;

    //void Update(float deltaTime) {}

    int AddLayer(flecs::entity entity, const std::string& name);
    void RemoveLayer(flecs::entity entity, int layerIndex);
    void SetLayerWeight(flecs::entity entity, int layerIndex, float weight);
    void SetLayerAdditive(flecs::entity entity, int layerIndex, bool additive);
    void SetLayerSpeed(flecs::entity entity, int layerIndex, float speed);

    void PlayAnimation(flecs::entity entity, int layerIndex, const std::string& animName,
        bool loop = true, float transitionTime = 0.3f);
    void PauseLayer(flecs::entity entity, int layerIndex);
    void ResumeLayer(flecs::entity entity, int layerIndex);
    void StopLayer(flecs::entity entity, int layerIndex);

    int AddIKChain(flecs::entity entity, const std::string& name,
        const std::string& startJoint, const std::string& midJoint, const std::string& endJoint);
    void SetIKTarget(flecs::entity entity, int chainIndex, const glm::vec3& target);
    void SetIKPoleVector(flecs::entity entity, int chainIndex, const glm::vec3& pole);
    void SetIKWeight(flecs::entity entity, int chainIndex, float weight);
    void EnableIK(flecs::entity entity, int chainIndex, bool enabled);

    int AddAimIK(flecs::entity entity, const std::string& name, const std::string& joint);
    void SetAimTarget(flecs::entity entity, int aimIndex, const glm::vec3& target);
    void SetAimWeight(flecs::entity entity, int aimIndex, float weight);
    void EnableAim(flecs::entity entity, int aimIndex, bool enabled);

    bool HasAnimation(flecs::entity entity, const std::string& name) const;
    std::vector<std::string> GetAnimationNames(flecs::entity entity) const;
    float GetAnimationDuration(flecs::entity entity, const std::string& animName) const;
    int GetLayerCount(flecs::entity entity) const;

    glm::vec3 GetBoneWorldPosition(const std::string& boneName,
        const AnimationComponent& anim,
        const SkeletalMeshComponent& meshComp,
        const TransformComponent& transform);

private:
    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;

    flecs::system m_animationSystem;
    flecs::observer m_animationRemoveObserver;
    //flecs::query<AnimationComponent, const SkeletalMeshComponent> m_animationQuery;

private:
    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();

    void RegisterWithEditor();

    void InitialiseAnimationState(flecs::entity entity, AnimationComponent& state,
        const SkeletalMeshComponent& meshComp);
    void ReinitialiseLayer(AnimationLayer& layer, const ozz::animation::Skeleton* skeleton);
    void InitialiseRestPose(AnimationComponent& state, const ozz::animation::Skeleton* skeleton);

    void UpdateAnimation(flecs::entity entity, AnimationComponent& state,
        const SkeletalMeshComponent& meshComp, float deltaTime);
    void UpdateLayer(AnimationLayer& layer, const SkeletalMeshComponent& meshComp, float deltaTime);
    void SampleAnimation(const std::string& animName, float time,
        ozz::animation::SamplingJob::Context* context,
        ozz::vector<ozz::math::SoaTransform>* output,
        const SkeletalMeshComponent& meshComp);

    void BlendLayers(AnimationComponent& state, const SkeletalMeshComponent& meshComp);
    void ApplyIK(AnimationComponent& state, const SkeletalMeshComponent& meshComp);
    void UpdateBoneTransforms(AnimationComponent& state, const SkeletalMeshComponent& meshComp);
    void UploadBoneTransforms(AnimationComponent& state);

    void DrawAnimationInspector(flecs::entity entity, AnimationComponent& component);
};