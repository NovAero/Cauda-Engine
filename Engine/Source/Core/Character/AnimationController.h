// AnimationController.h
#pragma once
#include <ThirdParty/flecs.h>
#include <glm/glm.hpp>
#include <string>

class AnimationModule;
class PhysicsModule;
class EditorModule;
struct AnimationComponent;
struct SkeletalMeshComponent;
struct CharacterComponent;
struct TransformComponent;

enum class CharacterAnimState
{
    Idle,
    Walking,
    Running,
    Jumping,
    Falling,
    Landing
};

struct FootIKData
{
    int twoBoneIKChainIndex = -1;
    int aimIKChainIndex = -1;

    bool hit = false;
    glm::vec3 rayStart = glm::vec3(0.0f);
    glm::vec3 hitPoint = glm::vec3(0.0f);
    glm::vec3 hitNormal = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 ankleTargetWS = glm::vec3(0.0f);
    glm::vec3 smoothedPosition = glm::vec3(0.0f);

    float smoothingSpeed = 15.0f;
};

struct CharacterAnimationComponent
{
	// Serialised
    std::string idleAnimation = "idle";
    std::string walkAnimation = "walk";
    std::string runAnimation = "walk";
    std::string jumpAnimation = "idle";
    std::string fallAnimation = "idle";
    std::string landAnimation = "idle";

    float walkThreshold = 1.0f;
    float runThreshold = 15.0f;
    float fallThreshold = -2.0f;
    float landingDuration = 0.3f;

    // Runtime
    CharacterAnimState currentState = CharacterAnimState::Idle;
    CharacterAnimState previousState = CharacterAnimState::Idle;
    float stateTime = 0.0f;

    int locomotionLayerIndex = -1;
    int jumpLayerIndex = -1;

    int hipsAimIKIndex = -1;
    std::string hipsBoneName = "pelvis";
    int hipsJointIndex = -1;

    int spineAimIKIndex = -1;
    std::string spineBoneName = "spine_1";
    int spineJointIndex = -1;

    float lowerBodyAimWeight = .2f;
    float upperBodyAimWeight = 1.0f;



    bool enableFootIK = true;
    FootIKData leftFoot;
    FootIKData rightFoot;

    std::string leftThighBone = "thigh_l";
    std::string leftCalfBone = "calf_l";
    std::string leftFootBone = "foot_l";
    std::string rightThighBone = "thigh_r";
    std::string rightCalfBone = "calf_r";
    std::string rightFootBone = "foot_r";

    float footIKRadius = 0.08f;
    float footIKDistance = 0.6f;
    float footIKGroundOffset = 0.05f;
    float maxFootOffset = 0.5f;
    float footHeight = 0.12f;

    bool enableAnkleRotation = true;
    glm::vec3 ankleForwardAxis = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 ankleUpAxis = glm::vec3(0.0f, 1.0f, 0.0f);

    float ikWeight = 1.0f;
    float ikSoften = 0.97f;
    glm::vec3 kneePoleDirection = glm::vec3(0.0f, 0.0f, 1.0f);
    float poleVectorDistance = 1.0f;

    bool initialised = false;
};

class AnimationController
{
public:
    AnimationController(flecs::world& world);
    ~AnimationController();

    void Update(float deltaTime);

    void SetupDefaultAnimations(flecs::entity entity);

    void SetAnimationState(flecs::entity entity, CharacterAnimState state);
    CharacterAnimState GetAnimationState(flecs::entity entity) const;

    void EnableFootIK(flecs::entity entity, bool enable);
    void SetFootIKSettings(flecs::entity entity, float radius, float distance,
        float groundOffset, float maxOffset, float footHeight);
    void SetFootBoneNames(flecs::entity entity,
        const std::string& leftThigh, const std::string& leftCalf, const std::string& leftFoot,
        const std::string& rightThigh, const std::string& rightCalf, const std::string& rightFoot);
    void EnableAnkleRotation(flecs::entity entity, bool enable);

    void SetSpineBone(flecs::entity entity, const std::string& boneName);

    void SetHipsBone(flecs::entity entity, const std::string& boneName);

private:
    flecs::world& m_world;
    AnimationModule* m_animationModule = nullptr;
    PhysicsModule* m_physicsModule = nullptr;
    EditorModule* m_editorModule = nullptr;

    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void RegisterWithEditor();

    bool InitialiseCharacterAnimation(flecs::entity entity,
        CharacterAnimationComponent& charAnim);

    void UpdateCharacterAnimationState(flecs::entity entity,
        CharacterAnimationComponent& charAnim,
        const CharacterComponent& character,
        float deltaTime);

    void TransitionToState(flecs::entity entity,
        CharacterAnimationComponent& charAnim,
        CharacterAnimState newState);

    void UpdateFootIK(flecs::entity entity,
        CharacterAnimationComponent& charAnim,
        AnimationComponent& anim,
        const SkeletalMeshComponent& meshComp,
        const TransformComponent& transform,
        float deltaTime);

    bool RaycastLegs(flecs::entity entity,
        CharacterAnimationComponent& charAnim,
        const AnimationComponent& anim,
        const SkeletalMeshComponent& meshComp,
        const TransformComponent& transform);

    bool UpdateAnklesTarget(CharacterAnimationComponent& charAnim);

    bool ApplyFootIK(flecs::entity entity,
        CharacterAnimationComponent& charAnim,
        float deltaTime);

    void SetupFootIKChains(flecs::entity entity, CharacterAnimationComponent& charAnim);

    void ApplySpineAim(flecs::entity entity, CharacterAnimationComponent& charAnim,
        flecs::entity animEntity, const CharacterComponent* characterComp);

    void DrawCharacterAnimInspector(flecs::entity entity, CharacterAnimationComponent& component);
};