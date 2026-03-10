// AnimationController.cpp
#include "cepch.h"
#include "AnimationController.h"
#include "Renderer/AnimationModule.h"
#include "Renderer/SkeletalMesh.h"
#include "Renderer/RendererModule.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Character/InputComponent.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include <iostream>

AnimationController::AnimationController(flecs::world& world)
    : m_world(world)
{
    m_animationModule = m_world.try_get_mut<AnimationModule>();
    m_physicsModule = m_world.try_get_mut<PhysicsModule>();
    m_editorModule = m_world.try_get_mut<EditorModule>();

    SetupComponents();
    SetupSystems();
    SetupObservers();
    RegisterWithEditor();
}

AnimationController::~AnimationController()
{
}

void AnimationController::SetupComponents()
{
    m_world.component<CharacterAnimationComponent>("CharacterAnimationComponent")
        .member<std::string>("idleAnimation")
        .member<std::string>("walkAnimation")
        .member<std::string>("runAnimation")
        .member<std::string>("jumpAnimation")
        .member<std::string>("fallAnimation")
        .member<std::string>("landAnimation")
        .member<float>("walkThreshold")
        .member<float>("runThreshold")
        .member<float>("fallThreshold")
        .member<float>("landingDuration");
}

void AnimationController::SetupSystems()
{
    m_world.system<CharacterAnimationComponent>()
        .kind(flecs::OnLoad)
        .each([this](flecs::entity entity, CharacterAnimationComponent& charAnim)
            {
                if (charAnim.initialised) return;

                InitialiseCharacterAnimation(entity, charAnim);
            });

    m_world.system<CharacterAnimationComponent,
        const CharacterComponent>()
        .kind(flecs::OnValidate)
        .each([this](flecs::entity entity,
            CharacterAnimationComponent& charAnim,
            const CharacterComponent& character)
            {
                UpdateCharacterAnimationState(entity, charAnim, character, m_world.delta_time());

                entity.children([&](flecs::entity child)
                    {
                        if (child.has<AnimationComponent>())
                        {
                            UpdateCharacterAnimationState(entity, charAnim, character, m_world.delta_time());
                        }
                    });
            });

    m_world.system<CharacterAnimationComponent, const TransformComponent, const CharacterComponent>()
        .kind(flecs::PostUpdate)
        .each([this](flecs::entity entity, CharacterAnimationComponent& charAnim,
            const TransformComponent& transform, const CharacterComponent& character)
            {
                if (entity.has<AnimationComponent>() && entity.has<SkeletalMeshComponent>())
                {
                    auto* anim = entity.try_get_mut<AnimationComponent>();
                    auto* meshComp = entity.try_get_mut<SkeletalMeshComponent>();

                    if (charAnim.enableFootIK)
                    {
                        UpdateFootIK(entity, charAnim, *anim, *meshComp, transform, m_world.delta_time());
                    }

                   // ApplySpineAim(entity, charAnim, entity, &character);
                }

                entity.children([&](flecs::entity child)
                    {
                        if (child.has<AnimationComponent>() && child.has<SkeletalMeshComponent>())
                        {
                            auto* anim = child.try_get_mut<AnimationComponent>();
                            auto* meshComp = child.try_get_mut<SkeletalMeshComponent>();

                            if (charAnim.enableFootIK)
                            {
                                UpdateFootIK(entity, charAnim, *anim, *meshComp, transform, m_world.delta_time());
                            }

                           // ApplySpineAim(entity, charAnim, child, &character);
                        }
                    });
            });
}

void AnimationController::SetupObservers()
{
}

void AnimationController::Update(float deltaTime)
{
}

bool AnimationController::InitialiseCharacterAnimation(flecs::entity entity,
    CharacterAnimationComponent& charAnim)
{
    flecs::entity targetEntity = flecs::entity::null();

    bool parentHasAnim = entity.has<AnimationComponent>();
    bool parentHasMesh = entity.has<SkeletalMeshComponent>();

    if (parentHasAnim && parentHasMesh)
    {
        targetEntity = entity;
    }
    else
    {
        entity.children([&](flecs::entity child) {
            if (child.has<AnimationComponent>() && child.has<SkeletalMeshComponent>())
            {
                targetEntity = child;
            }
            });
    }

    if (!targetEntity)
        return false;

    if (charAnim.locomotionLayerIndex < 0)
    {
        charAnim.locomotionLayerIndex = m_animationModule->AddLayer(targetEntity, "Locomotion");
    }

    if (charAnim.jumpLayerIndex < 0)
    {
        charAnim.jumpLayerIndex = m_animationModule->AddLayer(targetEntity, "Jump");
    }

    if (charAnim.locomotionLayerIndex == -1 || charAnim.jumpLayerIndex == -1)
    {
        return false;
    }

    m_animationModule->SetLayerAdditive(targetEntity, charAnim.jumpLayerIndex, false);
    m_animationModule->SetLayerWeight(targetEntity, charAnim.jumpLayerIndex, 0.0f);

    auto* meshComp = targetEntity.try_get<SkeletalMeshComponent>();
    if (meshComp)
    {
        SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
        if (mesh)
        {
            charAnim.hipsJointIndex = mesh->GetJointIndex(charAnim.hipsBoneName);
            if (charAnim.hipsJointIndex >= 0)
            {
                charAnim.hipsAimIKIndex = m_animationModule->AddAimIK(
                    targetEntity, "HipsAim", charAnim.hipsBoneName
                );
                if (charAnim.hipsAimIKIndex >= 0)
                {
                    m_animationModule->SetAimWeight(targetEntity, charAnim.hipsAimIKIndex, charAnim.lowerBodyAimWeight);
                }
            }

            charAnim.spineJointIndex = mesh->GetJointIndex(charAnim.spineBoneName);
            if (charAnim.spineJointIndex >= 0)
            {
                charAnim.spineAimIKIndex = m_animationModule->AddAimIK(
                    targetEntity, "SpineAim", charAnim.spineBoneName
                );
                if (charAnim.spineAimIKIndex >= 0)
                {
                    m_animationModule->SetAimWeight(targetEntity, charAnim.spineAimIKIndex, charAnim.upperBodyAimWeight);
                }
            }
        }
    }

    if (charAnim.enableFootIK)
    {
        SetupFootIKChains(targetEntity, charAnim);
    }

    m_animationModule->PlayAnimation(targetEntity, charAnim.locomotionLayerIndex, charAnim.idleAnimation, true, 0.0f);

    charAnim.initialised = true;
    charAnim.currentState = CharacterAnimState::Idle;
    charAnim.stateTime = 0.0f;

    return true;
}

void AnimationController::SetupFootIKChains(flecs::entity entity, CharacterAnimationComponent& charAnim)
{
    if (!m_animationModule)
        return;

    if (charAnim.leftFoot.twoBoneIKChainIndex < 0)
    {
        charAnim.leftFoot.twoBoneIKChainIndex = m_animationModule->AddIKChain(
            entity, "LeftLeg_FootIK",
            charAnim.leftThighBone,
            charAnim.leftCalfBone,
            charAnim.leftFootBone
        );

        if (charAnim.leftFoot.twoBoneIKChainIndex >= 0)
        {
            m_animationModule->SetIKWeight(entity, charAnim.leftFoot.twoBoneIKChainIndex, charAnim.ikWeight);
        }
    }

    if (charAnim.rightFoot.twoBoneIKChainIndex < 0)
    {
        charAnim.rightFoot.twoBoneIKChainIndex = m_animationModule->AddIKChain(
            entity, "RightLeg_FootIK",
            charAnim.rightThighBone,
            charAnim.rightCalfBone,
            charAnim.rightFootBone
        );

        if (charAnim.rightFoot.twoBoneIKChainIndex >= 0)
        {
            m_animationModule->SetIKWeight(entity, charAnim.rightFoot.twoBoneIKChainIndex, charAnim.ikWeight);
        }
    }

    if (charAnim.enableAnkleRotation)
    {
        if (charAnim.leftFoot.aimIKChainIndex < 0)
        {
            charAnim.leftFoot.aimIKChainIndex = m_animationModule->AddAimIK(
                entity, "LeftAnkle_FootIK", charAnim.leftFootBone
            );
            if (charAnim.leftFoot.aimIKChainIndex >= 0)
            {
                m_animationModule->SetAimWeight(entity, charAnim.leftFoot.aimIKChainIndex, charAnim.ikWeight);
            }
        }

        if (charAnim.rightFoot.aimIKChainIndex < 0)
        {
            charAnim.rightFoot.aimIKChainIndex = m_animationModule->AddAimIK(
                entity, "RightAnkle_FootIK", charAnim.rightFootBone
            );
            if (charAnim.rightFoot.aimIKChainIndex >= 0)
            {
                m_animationModule->SetAimWeight(entity, charAnim.rightFoot.aimIKChainIndex, charAnim.ikWeight);
            }
        }
    }
}

void AnimationController::ApplySpineAim(flecs::entity entity,
    CharacterAnimationComponent& charAnim,
    flecs::entity animEntity,
    const CharacterComponent* characterComp)
{
    if (!m_animationModule)
        return;

    auto* parentTransform = entity.try_get<TransformComponent>();
    if (!parentTransform || !characterComp)
        return;

    auto* childTransform = animEntity.try_get<TransformComponent>();
    if (!childTransform)
        return;

    glm::vec3 movementDir = characterComp->velocity;
    movementDir.y = 0;

    bool hasMovement = glm::length(movementDir) > 0.1f;

    if (charAnim.hipsAimIKIndex >= 0 && hasMovement)
    {
        movementDir = glm::normalize(movementDir);

        glm::vec3 worldHipsTarget = parentTransform->position + movementDir * 10.0f;

        glm::mat4 childWorldMatrix = glm::translate(glm::mat4(1.0f), childTransform->position) *
            glm::mat4_cast(childTransform->rotation) *
            glm::scale(glm::mat4(1.0f), childTransform->scale);

        glm::mat4 childInverseMatrix = glm::inverse(childWorldMatrix);
        glm::vec3 localHipsTarget = glm::vec3(childInverseMatrix * glm::vec4(worldHipsTarget, 1.0f));

        m_animationModule->SetAimTarget(animEntity, charAnim.hipsAimIKIndex, localHipsTarget);
        m_animationModule->EnableAim(animEntity, charAnim.hipsAimIKIndex, true);
    }
    else if (charAnim.hipsAimIKIndex >= 0)
    {
        m_animationModule->EnableAim(animEntity, charAnim.hipsAimIKIndex, false);
    }

    //if (charAnim.spineAimIKIndex >= 0)
    //{
    //    glm::vec3 parentForward = parentTransform->rotation * glm::vec3(0, 0, -1);

    //    glm::vec3 worldSpineTarget = parentTransform->position + parentForward * 10.0f;

    //    glm::mat4 childWorldMatrix = glm::translate(glm::mat4(1.0f), childTransform->position) *
    //        glm::mat4_cast(childTransform->rotation) *
    //        glm::scale(glm::mat4(1.0f), childTransform->scale);

    //    glm::mat4 childInverseMatrix = glm::inverse(childWorldMatrix);
    //    glm::vec3 localSpineTarget = glm::vec3(childInverseMatrix * glm::vec4(worldSpineTarget, 1.0f));

    //    m_animationModule->SetAimTarget(animEntity, charAnim.spineAimIKIndex, localSpineTarget);
    //    m_animationModule->EnableAim(animEntity, charAnim.spineAimIKIndex, true);
    //}
}

void AnimationController::UpdateCharacterAnimationState(flecs::entity entity,
    CharacterAnimationComponent& charAnim,
    const CharacterComponent& character,
    float deltaTime)
{
    if (!m_animationModule)
        return;

    charAnim.stateTime += deltaTime;

    float horizontalSpeed = glm::length(glm::vec2(character.velocity.x, character.velocity.z));
    float verticalSpeed = character.velocity.y;

    CharacterAnimState newState = charAnim.currentState;

    switch (charAnim.currentState)
    {
    case CharacterAnimState::Idle:
        if (!character.isGrounded)
        {
            if (verticalSpeed < charAnim.fallThreshold)
                newState = CharacterAnimState::Falling;
            else if (character.isJumping)
                newState = CharacterAnimState::Jumping;
        }
        else if (horizontalSpeed > charAnim.walkThreshold)
        {
            newState = horizontalSpeed > charAnim.runThreshold ?
                CharacterAnimState::Running : CharacterAnimState::Walking;
        }
        break;

    case CharacterAnimState::Walking:
        if (!character.isGrounded)
        {
            newState = verticalSpeed < charAnim.fallThreshold ?
                CharacterAnimState::Falling : CharacterAnimState::Jumping;
        }
        else if (horizontalSpeed <= charAnim.walkThreshold)
        {
            newState = CharacterAnimState::Idle;
        }
        else if (horizontalSpeed > charAnim.runThreshold)
        {
            newState = CharacterAnimState::Running;
        }
        break;

    case CharacterAnimState::Running:
        if (!character.isGrounded)
        {
            newState = verticalSpeed < charAnim.fallThreshold ?
                CharacterAnimState::Falling : CharacterAnimState::Jumping;
        }
        else if (horizontalSpeed <= charAnim.runThreshold)
        {
            newState = horizontalSpeed <= charAnim.walkThreshold ?
                CharacterAnimState::Idle : CharacterAnimState::Walking;
        }
        break;

    case CharacterAnimState::Jumping:
        if (verticalSpeed < charAnim.fallThreshold)
        {
            newState = CharacterAnimState::Falling;
        }
        else if (character.isGrounded)
        {
            newState = CharacterAnimState::Landing;
        }
        break;

    case CharacterAnimState::Falling:
        if (character.isGrounded)
        {
            newState = CharacterAnimState::Landing;
        }
        break;

    case CharacterAnimState::Landing:
        if (charAnim.stateTime >= charAnim.landingDuration)
        {
            newState = horizontalSpeed > charAnim.walkThreshold ?
                (horizontalSpeed > charAnim.runThreshold ? CharacterAnimState::Running : CharacterAnimState::Walking) :
                CharacterAnimState::Idle;
        }
        break;
    }

    if (newState != charAnim.currentState)
    {
        TransitionToState(entity, charAnim, newState);
    }
}

void AnimationController::TransitionToState(flecs::entity entity,
    CharacterAnimationComponent& charAnim,
    CharacterAnimState newState)
{
    if (!m_animationModule)
        return;

    charAnim.previousState = charAnim.currentState;
    charAnim.currentState = newState;
    charAnim.stateTime = 0.0f;

    std::string newLocomotionAnim = "";

    entity.children([&](flecs::entity child)
        {
            if (child.has<AnimationComponent>())
            {
                switch (newState)
                {
                case CharacterAnimState::Idle:
                    newLocomotionAnim = charAnim.idleAnimation;
                    m_animationModule->SetLayerWeight(child, charAnim.jumpLayerIndex, 0.0f);
                    break;

                case CharacterAnimState::Walking:
                    newLocomotionAnim = charAnim.walkAnimation;
                    m_animationModule->SetLayerWeight(child, charAnim.jumpLayerIndex, 0.0f);
                    break;

                case CharacterAnimState::Running:
                    newLocomotionAnim = charAnim.runAnimation;
                    m_animationModule->SetLayerWeight(child, charAnim.jumpLayerIndex, 0.0f);
                    break;

                case CharacterAnimState::Jumping:
                    if (charAnim.jumpLayerIndex >= 0)
                    {
                        m_animationModule->PlayAnimation(child, charAnim.jumpLayerIndex,
                            charAnim.jumpAnimation, false, 0.1f);
                        m_animationModule->SetLayerWeight(child, charAnim.jumpLayerIndex, 1.0f);
                    }
                    break;

                case CharacterAnimState::Falling:
                    if (charAnim.jumpLayerIndex >= 0)
                    {
                        m_animationModule->PlayAnimation(child, charAnim.jumpLayerIndex,
                            charAnim.fallAnimation, true, 0.2f);
                        m_animationModule->SetLayerWeight(child, charAnim.jumpLayerIndex, 1.0f);
                    }
                    break;

                case CharacterAnimState::Landing:
                    if (charAnim.jumpLayerIndex >= 0)
                    {
                        m_animationModule->PlayAnimation(child, charAnim.jumpLayerIndex,
                            charAnim.landAnimation, false, 0.1f);
                        m_animationModule->SetLayerWeight(child, charAnim.jumpLayerIndex, 1.0f);
                    }
                    break;
                }

                if (!newLocomotionAnim.empty() && charAnim.locomotionLayerIndex >= 0)
                {
                    m_animationModule->PlayAnimation(child, charAnim.locomotionLayerIndex, newLocomotionAnim, true, 0.2f);
                    m_animationModule->SetLayerWeight(child, charAnim.locomotionLayerIndex, 1.0f);
                }
            }
        });
}

void AnimationController::SetupDefaultAnimations(flecs::entity entity)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (!charAnim || !m_animationModule)
        return;
}

void AnimationController::SetAnimationState(flecs::entity entity, CharacterAnimState state)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    auto* anim = entity.try_get_mut<AnimationComponent>();

    if (charAnim && anim)
    {
        TransitionToState(entity, *charAnim, state);
    }
}

CharacterAnimState AnimationController::GetAnimationState(flecs::entity entity) const
{
    auto* charAnim = entity.try_get<CharacterAnimationComponent>();
    return charAnim ? charAnim->currentState : CharacterAnimState::Idle;
}

void AnimationController::EnableFootIK(flecs::entity entity, bool enable)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (!charAnim)
        return;

    bool wasEnabled = charAnim->enableFootIK;
    charAnim->enableFootIK = enable;

    if (enable && !wasEnabled)
    {
        SetupFootIKChains(entity, *charAnim);
    }
    else if (!enable && wasEnabled)
    {
        if (charAnim->leftFoot.twoBoneIKChainIndex >= 0)
        {
            m_animationModule->EnableIK(entity, charAnim->leftFoot.twoBoneIKChainIndex, false);
        }
        if (charAnim->rightFoot.twoBoneIKChainIndex >= 0)
        {
            m_animationModule->EnableIK(entity, charAnim->rightFoot.twoBoneIKChainIndex, false);
        }
        if (charAnim->leftFoot.aimIKChainIndex >= 0)
        {
            m_animationModule->EnableAim(entity, charAnim->leftFoot.aimIKChainIndex, false);
        }
        if (charAnim->rightFoot.aimIKChainIndex >= 0)
        {
            m_animationModule->EnableAim(entity, charAnim->rightFoot.aimIKChainIndex, false);
        }
    }
}

void AnimationController::SetFootIKSettings(flecs::entity entity, float radius,
    float distance, float groundOffset, float maxOffset, float footHeight)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (charAnim)
    {
        charAnim->footIKRadius = radius;
        charAnim->footIKDistance = distance;
        charAnim->footIKGroundOffset = groundOffset;
        charAnim->maxFootOffset = maxOffset;
        charAnim->footHeight = footHeight;
    }
}

void AnimationController::SetFootBoneNames(flecs::entity entity,
    const std::string& leftThigh, const std::string& leftCalf, const std::string& leftFoot,
    const std::string& rightThigh, const std::string& rightCalf, const std::string& rightFoot)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (charAnim)
    {
        charAnim->leftThighBone = leftThigh;
        charAnim->leftCalfBone = leftCalf;
        charAnim->leftFootBone = leftFoot;
        charAnim->rightThighBone = rightThigh;
        charAnim->rightCalfBone = rightCalf;
        charAnim->rightFootBone = rightFoot;

        charAnim->leftFoot.twoBoneIKChainIndex = -1;
        charAnim->leftFoot.aimIKChainIndex = -1;
        charAnim->rightFoot.twoBoneIKChainIndex = -1;
        charAnim->rightFoot.aimIKChainIndex = -1;
    }
}

void AnimationController::EnableAnkleRotation(flecs::entity entity, bool enable)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (charAnim)
    {
        charAnim->enableAnkleRotation = enable;
    }
}

void AnimationController::SetSpineBone(flecs::entity entity, const std::string& boneName)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (charAnim)
    {
        charAnim->spineBoneName = boneName;
        charAnim->spineJointIndex = -1;
    }
}

void AnimationController::SetHipsBone(flecs::entity entity, const std::string& boneName)
{
    auto* charAnim = entity.try_get_mut<CharacterAnimationComponent>();
    if (charAnim)
    {
        charAnim->hipsBoneName = boneName;
        charAnim->hipsJointIndex = -1;
    }
}

void AnimationController::UpdateFootIK(flecs::entity entity,
    CharacterAnimationComponent& charAnim,
    AnimationComponent& anim,
    const SkeletalMeshComponent& meshComp,
    const TransformComponent& transform,
    float deltaTime)
{
    if (!m_physicsModule || !m_animationModule)
        return;

    if (!RaycastLegs(entity, charAnim, anim, meshComp, transform))
        return;

    if (!UpdateAnklesTarget(charAnim))
        return;

    ApplyFootIK(entity, charAnim, deltaTime);
}

bool AnimationController::RaycastLegs(flecs::entity entity,
    CharacterAnimationComponent& charAnim,
    const AnimationComponent& anim,
    const SkeletalMeshComponent& meshComp,
    const TransformComponent& transform)
{
    if (!m_physicsModule || !m_animationModule)
        return false;

    glm::vec3 leftFootPos = m_animationModule->GetBoneWorldPosition(
        charAnim.leftFootBone, anim, meshComp, transform);
    glm::vec3 rightFootPos = m_animationModule->GetBoneWorldPosition(
        charAnim.rightFootBone, anim, meshComp, transform);

    std::vector<JPH::ObjectLayer> ignoreLayers = { 6, 3 };

    charAnim.leftFoot.rayStart = leftFootPos + glm::vec3(0, 0.5f, 0);
    auto leftHit = m_physicsModule->SphereCastWithLayerFilter(
        charAnim.footIKRadius,
        charAnim.leftFoot.rayStart,
        glm::vec3(0, -1, 0),
        charAnim.footIKDistance,
        ignoreLayers
    );

    if (leftHit.hasHit && leftHit.hitEntity != entity.id())
    {
        charAnim.leftFoot.hit = true;
        charAnim.leftFoot.hitPoint = leftHit.contactPoint2;
        charAnim.leftFoot.hitNormal = -leftHit.penetrationAxis;
    }
    else
    {
        charAnim.leftFoot.hit = false;
    }

    charAnim.rightFoot.rayStart = rightFootPos + glm::vec3(0, 0.5f, 0);
    auto rightHit = m_physicsModule->SphereCastWithLayerFilter(
        charAnim.footIKRadius,
        charAnim.rightFoot.rayStart,
        glm::vec3(0, -1, 0),
        charAnim.footIKDistance,
        ignoreLayers
    );

    if (rightHit.hasHit && rightHit.hitEntity != entity.id())
    {
        charAnim.rightFoot.hit = true;
        charAnim.rightFoot.hitPoint = rightHit.contactPoint2;
        charAnim.rightFoot.hitNormal = -rightHit.penetrationAxis;
    }
    else
    {
        charAnim.rightFoot.hit = false;
    }

    return true;
}

bool AnimationController::UpdateAnklesTarget(CharacterAnimationComponent& charAnim)
{
    auto calculateTarget = [&](FootIKData& foot)
        {
            if (!foot.hit)
                return;

            glm::vec3 AI = foot.rayStart - foot.hitPoint;
            float ABl = glm::dot(AI, foot.hitNormal);

            if (std::abs(ABl) < 0.001f)
            {
                foot.ankleTargetWS = foot.hitPoint + foot.hitNormal * charAnim.footHeight;
                return;
            }

            glm::vec3 B = foot.rayStart - foot.hitNormal * ABl;
            glm::vec3 IB = B - foot.hitPoint;
            float IBl = glm::length(IB);

            if (IBl <= 0.001f)
            {
                foot.ankleTargetWS = foot.hitPoint + foot.hitNormal * charAnim.footHeight;
            }
            else
            {
                float IHl = IBl * charAnim.footHeight / ABl;
                glm::vec3 IH = IB * (IHl / IBl);
                glm::vec3 H = foot.hitPoint + IH;
                glm::vec3 C = H + foot.hitNormal * charAnim.footHeight;

                foot.ankleTargetWS = C;
            }

            foot.ankleTargetWS += foot.hitNormal * charAnim.footIKGroundOffset;
        };

    calculateTarget(charAnim.leftFoot);
    calculateTarget(charAnim.rightFoot);

    return true;
}

bool AnimationController::ApplyFootIK(flecs::entity entity,
    CharacterAnimationComponent& charAnim,
    float deltaTime)
{
    if (!m_animationModule)
        return false;

    auto* transform = entity.try_get<TransformComponent>();
    if (!transform)
        return false;

    glm::vec3 characterForward = transform->rotation * charAnim.kneePoleDirection;

    auto applyFoot = [&](FootIKData& foot, bool isLeft)
        {
            if (!foot.hit)
            {
                if (foot.twoBoneIKChainIndex >= 0)
                {
                    m_animationModule->SetIKWeight(entity, foot.twoBoneIKChainIndex, 0.0f);
                }
                if (foot.aimIKChainIndex >= 0 && charAnim.enableAnkleRotation)
                {
                    m_animationModule->SetAimWeight(entity, foot.aimIKChainIndex, 0.0f);
                }
                return;
            }

            foot.smoothedPosition = glm::mix(
                foot.smoothedPosition,
                foot.ankleTargetWS,
                foot.smoothingSpeed * deltaTime
            );

            glm::vec3 polePosition = foot.smoothedPosition +
                characterForward * charAnim.poleVectorDistance;

            if (foot.twoBoneIKChainIndex >= 0)
            {
                m_animationModule->SetIKTarget(entity, foot.twoBoneIKChainIndex, foot.smoothedPosition);
                m_animationModule->SetIKPoleVector(entity, foot.twoBoneIKChainIndex, polePosition);
                m_animationModule->SetIKWeight(entity, foot.twoBoneIKChainIndex, charAnim.ikWeight);
            }

            if (charAnim.enableAnkleRotation && foot.aimIKChainIndex >= 0)
            {
                glm::vec3 footForward = characterForward -
                    foot.hitNormal * glm::dot(characterForward, foot.hitNormal);

                if (glm::length(footForward) > 0.001f)
                {
                    footForward = glm::normalize(footForward);
                }
                else
                {
                    footForward = characterForward;
                }

                glm::vec3 aimTarget = foot.smoothedPosition + footForward * 0.5f;

                m_animationModule->SetAimTarget(entity, foot.aimIKChainIndex, aimTarget);
                m_animationModule->SetAimWeight(entity, foot.aimIKChainIndex, charAnim.ikWeight);
            }
        };

    applyFoot(charAnim.leftFoot, true);
    applyFoot(charAnim.rightFoot, false);

    return true;
}

void AnimationController::RegisterWithEditor()
{
    if (!m_editorModule)
        return;

    m_editorModule->RegisterComponent<CharacterAnimationComponent>(
        "Character Animation",
        "Animation",
        [this](flecs::entity entity, CharacterAnimationComponent& component)
        {
            DrawCharacterAnimInspector(entity, component);
        }
    );
}

void AnimationController::DrawCharacterAnimInspector(flecs::entity entity,
    CharacterAnimationComponent& component)
{
    ImGui::PushID("CharacterAnimInspector");

    if (ImGui::CollapsingHeader("Animation State Machine", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* stateNames[] = {
            "Idle", "Walking", "Running", "Jumping", "Falling", "Landing"
        };
        int currentState = static_cast<int>(component.currentState);
        ImGui::Text("Current State: %s (%.2fs)", stateNames[currentState], component.stateTime);

        ImGui::Separator();
        ImGui::Text("Animation Names:");
        char buffer[64];

        strcpy_s(buffer, component.idleAnimation.c_str());
        if (ImGui::InputText("Idle", buffer, sizeof(buffer)))
            component.idleAnimation = buffer;

        strcpy_s(buffer, component.walkAnimation.c_str());
        if (ImGui::InputText("Walk", buffer, sizeof(buffer)))
            component.walkAnimation = buffer;

        strcpy_s(buffer, component.runAnimation.c_str());
        if (ImGui::InputText("Run", buffer, sizeof(buffer)))
            component.runAnimation = buffer;

        strcpy_s(buffer, component.jumpAnimation.c_str());
        if (ImGui::InputText("Jump", buffer, sizeof(buffer)))
            component.jumpAnimation = buffer;

        strcpy_s(buffer, component.fallAnimation.c_str());
        if (ImGui::InputText("Fall", buffer, sizeof(buffer)))
            component.fallAnimation = buffer;

        strcpy_s(buffer, component.landAnimation.c_str());
        if (ImGui::InputText("Land", buffer, sizeof(buffer)))
            component.landAnimation = buffer;

        ImGui::Separator();
        ImGui::Text("Transition Thresholds:");
        ImGui::DragFloat("Walk Speed", &component.walkThreshold, 0.1f, 0.1f, 10.0f);
        ImGui::DragFloat("Run Speed", &component.runThreshold, 0.1f, 0.1f, 20.0f);
        ImGui::DragFloat("Fall Speed", &component.fallThreshold, 0.1f, -10.0f, 0.0f);
        ImGui::DragFloat("Landing Duration", &component.landingDuration, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Twin-Stick Aim System"))
    {
        ImGui::Text("Lower Body (Hips):");
        char hipsBuf[64];
        strcpy_s(hipsBuf, component.hipsBoneName.c_str());
        if (ImGui::InputText("Hips Bone", hipsBuf, sizeof(hipsBuf)))
        {
            SetHipsBone(entity, hipsBuf);
        }

        if (component.hipsJointIndex >= 0)
        {
            ImGui::Text("Hips Joint Index: %d", component.hipsJointIndex);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: Hips joint not found!");
        }

        ImGui::SliderFloat("Lower Body Weight", &component.lowerBodyAimWeight, 0.0f, 1.0f);
        if (ImGui::IsItemDeactivatedAfterEdit() && component.hipsAimIKIndex >= 0)
        {
            entity.children([&](flecs::entity child) {
                if (child.has<AnimationComponent>())
                {
                    m_animationModule->SetAimWeight(child, component.hipsAimIKIndex, component.lowerBodyAimWeight);
                }
                });
        }

        ImGui::Separator();
        ImGui::Text("Upper Body (Spine):");
        char spineBuf[64];
        strcpy_s(spineBuf, component.spineBoneName.c_str());
        if (ImGui::InputText("Spine Bone", spineBuf, sizeof(spineBuf)))
        {
            SetSpineBone(entity, spineBuf);
        }

        if (component.spineJointIndex >= 0)
        {
            ImGui::Text("Spine Joint Index: %d", component.spineJointIndex);
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: Spine joint not found!");
        }

        ImGui::SliderFloat("Upper Body Weight", &component.upperBodyAimWeight, 0.0f, 1.0f);
        if (ImGui::IsItemDeactivatedAfterEdit() && component.spineAimIKIndex >= 0)
        {
            entity.children([&](flecs::entity child) {
                if (child.has<AnimationComponent>())
                {
                    m_animationModule->SetAimWeight(child, component.spineAimIKIndex, component.upperBodyAimWeight);
                }
                });
        }
    }

    if (ImGui::CollapsingHeader("Foot IK System"))
    {
        bool wasEnabled = component.enableFootIK;
        ImGui::Checkbox("Enable Foot IK", &component.enableFootIK);

        if (wasEnabled != component.enableFootIK)
        {
            EnableFootIK(entity, component.enableFootIK);
        }

        if (component.enableFootIK)
        {
            if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat("Sphere Radius", &component.footIKRadius, 0.01f, 0.01f, 0.5f);
                ImGui::DragFloat("Cast Distance", &component.footIKDistance, 0.1f, 0.1f, 5.0f);
                ImGui::DragFloat("Ground Offset", &component.footIKGroundOffset, 0.001f, -0.1f, 0.3f, "%.3f");
                ImGui::DragFloat("Foot Height", &component.footHeight, 0.01f, 0.0f, 0.5f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("IK Quality", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("IK Weight", &component.ikWeight, 0.0f, 1.0f);
                ImGui::SliderFloat("IK Soften", &component.ikSoften, 0.8f, 1.0f, "%.3f");
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Knee Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat3("Pole Direction", &component.kneePoleDirection.x, 0.01f, -1.0f, 1.0f);
                ImGui::DragFloat("Pole Distance", &component.poleVectorDistance, 0.1f, 0.1f, 5.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Ankle Rotation"))
            {
                bool wasAnkle = component.enableAnkleRotation;
                ImGui::Checkbox("Enable", &component.enableAnkleRotation);

                if (wasAnkle != component.enableAnkleRotation)
                {
                    EnableFootIK(entity, false);
                    EnableFootIK(entity, true);
                }

                if (component.enableAnkleRotation)
                {
                    ImGui::DragFloat3("Forward Axis", &component.ankleForwardAxis.x, 0.01f, -1.0f, 1.0f);
                    ImGui::DragFloat3("Up Axis", &component.ankleUpAxis.x, 0.01f, -1.0f, 1.0f);
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Smoothing"))
            {
                ImGui::DragFloat("Left Foot", &component.leftFoot.smoothingSpeed, 0.5f, 1.0f, 50.0f);
                ImGui::DragFloat("Right Foot", &component.rightFoot.smoothingSpeed, 0.5f, 1.0f, 50.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Debug Info"))
            {
                ImGui::Text("Left Foot: %s", component.leftFoot.hit ? "HIT" : "MISS");
                if (component.leftFoot.hit)
                {
                    ImGui::Text("  Point: (%.2f, %.2f, %.2f)",
                        component.leftFoot.hitPoint.x,
                        component.leftFoot.hitPoint.y,
                        component.leftFoot.hitPoint.z);

                    float slope = glm::degrees(glm::acos(
                        glm::clamp(glm::dot(component.leftFoot.hitNormal, glm::vec3(0, 1, 0)), -1.0f, 1.0f)));
                    ImGui::Text("  Slope: %.1f�", slope);
                }

                ImGui::Text("Right Foot: %s", component.rightFoot.hit ? "HIT" : "MISS");
                if (component.rightFoot.hit)
                {
                    ImGui::Text("  Point: (%.2f, %.2f, %.2f)",
                        component.rightFoot.hitPoint.x,
                        component.rightFoot.hitPoint.y,
                        component.rightFoot.hitPoint.z);

                    float slope = glm::degrees(glm::acos(
                        glm::clamp(glm::dot(component.rightFoot.hitNormal, glm::vec3(0, 1, 0)), -1.0f, 1.0f)));
                    ImGui::Text("  Slope: %.1f�", slope);
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Bone Names"))
            {
                char thighBuf[64], calfBuf[64], footBuf[64];

                ImGui::Text("Left Leg:");
                strcpy_s(thighBuf, component.leftThighBone.c_str());
                strcpy_s(calfBuf, component.leftCalfBone.c_str());
                strcpy_s(footBuf, component.leftFootBone.c_str());

                bool changed = false;
                changed |= ImGui::InputText("thigh_l", thighBuf, sizeof(thighBuf));
                changed |= ImGui::InputText("calf_l", calfBuf, sizeof(calfBuf));
                changed |= ImGui::InputText("foot_l", footBuf, sizeof(footBuf));

                if (changed)
                {
                    SetFootBoneNames(entity, thighBuf, calfBuf, footBuf,
                        component.rightThighBone, component.rightCalfBone, component.rightFootBone);
                }

                ImGui::Text("Right Leg:");
                strcpy_s(thighBuf, component.rightThighBone.c_str());
                strcpy_s(calfBuf, component.rightCalfBone.c_str());
                strcpy_s(footBuf, component.rightFootBone.c_str());

                changed = false;
                changed |= ImGui::InputText("thigh_r", thighBuf, sizeof(thighBuf));
                changed |= ImGui::InputText("calf_r", calfBuf, sizeof(calfBuf));
                changed |= ImGui::InputText("foot_r", footBuf, sizeof(footBuf));

                if (changed)
                {
                    SetFootBoneNames(entity,
                        component.leftThighBone, component.leftCalfBone, component.leftFootBone,
                        thighBuf, calfBuf, footBuf);
                }

                if (ImGui::Button("Reinitialise Foot IK"))
                {
                    EnableFootIK(entity, false);
                    EnableFootIK(entity, true);
                }

                ImGui::TreePop();
            }
        }
    }

    ImGui::PopID();
}