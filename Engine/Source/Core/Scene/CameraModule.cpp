#include "cepch.h"
#include "CameraModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Math/TransformModule.h"
#include "Renderer/ShaderProgram.h"
#include "Platform/Win32/Application.h"

#include <iostream>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

CameraModule::CameraModule(flecs::world& world) : m_world(world)
{
    m_world.module<CameraModule>();

    m_editorModule = m_world.try_get_mut<EditorModule>();

    SetupComponents();
    SetupSystems();
    SetupObservers();
    SetupQueries();
    RegisterWithEditor();

    std::string defaultFollow = "Root";
    auto [it, inserted] = m_persistentEntityNames.insert(defaultFollow);

    m_input = InputSystem::Inst();
    if (m_input)
    {
        m_input->AddKeyListener(this);
        m_input->AddMouseListener(this);
    }

    Logger::PrintLog("CameraModule initialised successfully");
}

CameraModule::~CameraModule()
{
    if (m_input)
    {
        m_input->RemoveKeyListener(this);
        m_input->RemoveMouseListener(this);
    }
}

void CameraModule::ResolveFollowTargets()
{
    m_world.each<CameraComponent>([this](flecs::entity e, CameraComponent& camera)
    {
        if (!camera.followTarget.is_alive())
        {
            camera.followTarget = e.world().lookup(camera.followTargetName.c_str());
            if (camera.followTarget.is_alive())
            {
                std::cout << "(2)Resolved camera follow target: " << camera.followTargetName << '\n';
            }
            else
            {
                std::cout << "(2)Didnt resolve follow target\n";
            }
        }
    });
}

void CameraModule::SetPitchYaw(flecs::entity camera, float pitch, float yaw)
{
    auto* camComp = camera.try_get_mut<CameraComponent>();
    auto* transform = camera.try_get_mut<TransformComponent>();
    if (!camComp || !transform) return;

    transform->rotation = glm::quat(1, 0, 0, 0);
    transform->cachedEuler = glm::vec3(0, 0, 0);

    ApplyPitchYawRotation(camera, pitch, yaw);
}

void CameraModule::ApplyPitchYawRotation(flecs::entity camera, float deltaPitch, float deltaYaw)
{
    auto* camComp = camera.try_get_mut<CameraComponent>();
    auto* transform = camera.try_get_mut<TransformComponent>();
    if (!camComp || !transform) return;

    float alignment = glm::dot(transform->rotation * glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    deltaPitch = alignment > 0.99f && deltaPitch > 0 ? 0 : deltaPitch;
    deltaPitch = alignment < -0.99f && deltaPitch < 0 ? 0 : deltaPitch;

    float pitchRadians = glm::radians(deltaPitch);
    float yawRadians = glm::radians(deltaYaw);

    glm::quat pitchRotation = glm::quat(cos(pitchRadians * 0.5f), sin(pitchRadians * 0.5f) * glm::vec3(1, 0, 0));
    glm::quat yawRotation = glm::quat(cos(yawRadians / 2), sin(yawRadians / 2) * glm::vec3(0, 1, 0));
    
    transform->rotation = glm::normalize(yawRotation * transform->rotation * pitchRotation);
    transform->cachedEuler.x += deltaPitch;
    transform->cachedEuler.y += deltaYaw;

    camera.modified<TransformComponent>();
}

const char* CameraModule::GetPersistentEntityName(flecs::entity entity)
{
    if (!entity.is_alive() || !entity.name()) return nullptr;

    std::string fullPath = entity.path().c_str();
    auto [it, inserted] = m_persistentEntityNames.insert(fullPath);
    return it->c_str();
}

void CameraModule::SetupComponents()
{
    m_world.component<ProjectionMode>("ProjectionMode")
        .constant("PERSPECTIVE", ProjectionMode::PERSPECTIVE)
        .constant("ORTHOGRAPHIC", ProjectionMode::ORTHOGRAPHIC);

    m_world.component<CameraFollowMode>("CameraFollowMode")
        .constant("NONE", CameraFollowMode::NONE)
        .constant("TOP_DOWN", CameraFollowMode::TOP_DOWN)
        .constant("FIRST_PERSON", CameraFollowMode::FIRST_PERSON);

    m_world.component<CameraComponent>("CameraComponent")
        .member<ProjectionMode>("projectionMode")
        .member<float>("fov")
        .member<float>("nearPlane")
        .member<float>("farPlane")
        .member<float>("orthoSize")
        .member<float>("movementSpeed")
        .member<float>("turnSpeed")
        .member<float>("zoomSpeed")
        .member<bool>("enableMovement")
        .member<bool>("enableSnapping")
        .member<CameraFollowMode>("followMode")
        .member<glm::vec3>("followOffset")
        .member<float>("followSpeed")
        .member<float>("followDistance")
        .member<bool>("smoothFollow")
        .member<bool>("snapObjects")
        .member<bool>("smoothDisplay")
        .member<bool>("subPixelAtIntegerScale")
        .member<glm::vec2>("resolution")
        .member<float>("pixelScale")
        .member<std::string>("followTargetName");

    m_world.component<MainCamera>("Main Camera Tag")
        .add(flecs::Exclusive);
}

void CameraModule::SetupSystems()
{
    m_updateSystem = m_world.system<CameraComponent, TransformComponent>("CameraUpdate")
        .kind(flecs::OnUpdate)
        .each([this](flecs::iter& it, size_t row, CameraComponent& camera, TransformComponent& transform)
            {
                m_deltaTime = it.delta_system_time();
                flecs::entity entity = it.entity(row);

                UpdateCameraInput(entity, camera, transform);

                if (camera.followMode != CameraFollowMode::NONE)
                {
                    UpdateCameraFollow(entity, camera, transform);
                }

                //should implement a better solution.
                //auto& app = Cauda::Application::Get();
                //if (app.HasGameViewportResized())
                //{
                //    camera.resolution = app.GetGameViewportSize();
                //}
            });

    m_resolveFollowTargetSystem = m_world.system<CameraComponent>("ResolveFollowTarget")
        .kind(flecs::PreUpdate)
        .each([](flecs::entity e, CameraComponent& camera)
            {
                if (!camera.followTarget.is_alive())
                {
                    camera.followTarget = e.world().lookup(camera.followTargetName.c_str());

                    if (camera.followTarget.is_alive())
                    {
                        std::cout << "(1) Resolved camera follow target: " << camera.followTarget.name() << std::endl;
                    }
                    else
                    {
                        camera.followTarget = e.world().lookup("Root");
                        std::cout << "(1) Failed to resolve camera follow target: " << camera.followTarget.name() << std::endl;
                    }
                }
            });
}

void CameraModule::SetupObservers()
{
    m_world.observer<CameraComponent>()
        .event(flecs::OnSet)
        .each([&](flecs::entity e, CameraComponent& camera)
        {
            if(!camera.followTarget)
            {
                auto root = m_world.lookup(camera.followTargetName.c_str());
                std::cout << "Set follow target to: " << camera.followTargetName << '\n';
                camera.followTarget = root;
                ResolveFollowTargets();
            }
            // This is no longer necessary.
           // camera.resolution = screenSize;
        });

    m_world.observer<CameraComponent, TransformComponent>("SyncCameraToTransform")
        .event(flecs::OnSet)
        .each([this](flecs::entity e, CameraComponent& camera, TransformComponent& transform)
        {
            UpdateCachedPitchYaw(e, camera, transform);
        });


}

void CameraModule::SetupQueries()
{
    m_updateQuery = m_world.query_builder<CameraComponent, TransformComponent>().build();
    m_mainCameraQuery = m_world.query_builder<MainCamera>()
        .with<MainCamera>()
        .build();
    m_followCameraQuery = m_world.query_builder<CameraComponent, TransformComponent>()
        .with<CameraComponent>().in()
        .build();
    m_followableQuery = m_world.query_builder<TransformComponent>().build();
}

void CameraModule::RegisterWithEditor()
{
    if (!m_editorModule) return;

    m_editorModule->RegisterComponent<CameraComponent>(
        "Camera",
        "Camera",
        [&](flecs::entity entity, CameraComponent& component)
        {
            DrawCameraInspector(entity, component);
        }
    );

    m_editorModule->RegisterEntityColour([&](flecs::entity entity) -> ImVec4
        {
            return GetCameraEntityColour(entity);
        });
}

void CameraModule::RemoveCamera(flecs::entity entity)
{
    if (!entity.is_alive()) return;

    if (entity.has<MainCamera>())
    {
        entity.remove<MainCamera>();
    }

    entity.remove<CameraComponent>();
}

flecs::entity CameraModule::GetMainCamera()
{
    flecs::entity mainCamera;
    m_mainCameraQuery.each([&mainCamera](flecs::entity entity, MainCamera)
    {
        mainCamera = entity;
    });

    return mainCamera;
}

void CameraModule::SetMainCamera(flecs::entity camera)
{
    if (!camera) return;
    if (!camera.has<CameraComponent>()) return;

    m_world.remove_all<MainCamera>();

    camera.add<MainCamera>();
}

void CameraModule::SetCameraFollow(flecs::entity camera, flecs::entity target, CameraFollowMode mode)
{
    if (!camera.has<CameraComponent>() || !target.is_alive()) return;

    auto* camComp = camera.try_get_mut<CameraComponent>();
    if (camComp)
    {
        camComp->followMode = mode;
        camComp->followTarget = target;
        camComp->followTargetName = target.path().c_str();

        std::cout << "Camera " << camera.name() << " now following " << target.name() << std::endl;
    }
}

void CameraModule::ClearCameraFollow(flecs::entity camera)
{
    if (!camera.has<CameraComponent>()) return;

    auto* camComp = camera.try_get_mut<CameraComponent>();
    if (camComp)
    {
        camComp->followMode = CameraFollowMode::NONE;
        camComp->followTarget = flecs::entity::null();
    }
}

flecs::entity CameraModule::GetFollowTarget(flecs::entity camera) const
{
    auto* camComp = camera.try_get<CameraComponent>();
    if (!camComp) return flecs::entity::null();

    if (camComp->followTarget.is_alive())
    {
        return camComp->followTarget;
    }

    return flecs::entity::null();
}

void CameraModule::UpdateCachedPitchYaw(flecs::entity entity, CameraComponent& camera, const TransformComponent& transform)
{
    glm::vec3 euler = Math::GetEulerAngles(transform.rotation);
    camera.cachedPitch = euler.x;
    camera.cachedYaw = euler.y;
}



void CameraModule::UpdateCameraInput(flecs::entity entity, CameraComponent& camera, TransformComponent& transform)
{
    if (camera.enableMovement && InputSystem::Inst())
    {
        HandleCameraInput(entity, camera, transform);
    }
}

void CameraModule::UpdateCameraFollow(flecs::entity entity, CameraComponent& camera, TransformComponent& transform)
{
    if (camera.followMode == CameraFollowMode::NONE)
    {
        return;
    }

    ResolveFollowTargets();

    if (!camera.followTarget)
    {
        std::cout << "Couldn't find camera follow target, switchting to root\n";
        camera.followTarget = m_world.lookup("Root");
    }

    auto* targetTransform = camera.followTarget.try_get<TransformComponent>();
    if (!targetTransform)
    {
       // camera.followTargetName = nullptr;
       // camera.followTarget = flecs::entity::null();
       // camera.followMode = CameraFollowMode::NONE;
        return;
    }

    glm::vec3 targetPos = targetTransform->position;

    switch (camera.followMode)
    {
    case CameraFollowMode::TOP_DOWN:
        UpdateTopDown(entity, targetPos, camera, transform);
        break;
    case CameraFollowMode::FIRST_PERSON:
        UpdateFirstPerson(entity, targetPos, camera, transform);
        break;
    default:
        break;
    }
}

void CameraModule::UpdateTopDown(flecs::entity entity, const glm::vec3& targetPos, CameraComponent& camera, TransformComponent& transform)
{
    if (!entity) return;

    glm::vec3 forward = GetForwardVector(entity);

    glm::vec3 basePos = targetPos + (-glm::normalize(forward) * camera.followDistance);

    glm::vec3 targetCameraPos = basePos + camera.followOffset;

    if (camera.smoothFollow)
    {
        transform.position = glm::mix(transform.position, targetCameraPos, camera.followSpeed * m_deltaTime);
    }
    else
    {
        transform.position = targetCameraPos;
    }
}

void CameraModule::UpdateFirstPerson(flecs::entity entity, const glm::vec3& targetPos, CameraComponent& camera, TransformComponent& transform)
{
    float yawDelta = m_input->GetMouseDelta().x * camera.turnSpeed;
    float pitchDelta = m_input->GetMouseDelta().y * camera.turnSpeed;

    ApplyPitchYawRotation(entity, pitchDelta, yawDelta);
    
    glm::vec3 targetCameraPos = targetPos + camera.followOffset;
    transform.position = targetCameraPos;

    // Gonna leave this here just in case
    //camera.yaw += yawDelta;
    //camera.pitch += pitchDelta;
    //camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);

    //glm::quat yawRotation = glm::angleAxis(glm::radians(camera.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    //glm::quat pitchRotation = glm::angleAxis(glm::radians(camera.pitch), glm::vec3(1.0f, 0.0f, 0.0f));

    //transform.rotation = yawRotation * pitchRotation;
}

void CameraModule::HandleCameraInput(flecs::entity entity, CameraComponent& camera, TransformComponent& transform)
{
    if (!entity) return;
   // if (ImGui::GetIO().WantCaptureKeyboard) return;
    //if (ImGui::GetIO().WantCaptureMouse) return;

    float currentSpeed = camera.movementSpeed;
    if (m_input->IsKeyDown(PKey::LShift))
    {
        currentSpeed *= 2.5f;
    }

    glm::vec3 forward = GetForwardVector(entity);
    glm::vec3 right = GetRightVector(entity);
    glm::vec3 up = GetUpVector(entity);

    if (camera.projectionMode == ProjectionMode::PERSPECTIVE)
    {
        if (m_input->IsKeyDown(PKey::W))
            transform.position += forward * m_deltaTime * currentSpeed;
        if (m_input->IsKeyDown(PKey::S))
            transform.position -= forward * m_deltaTime * currentSpeed;
        if (m_input->IsKeyDown(PKey::D))
            transform.position += right * m_deltaTime * currentSpeed;
        if (m_input->IsKeyDown(PKey::A))
            transform.position -= right * m_deltaTime * currentSpeed;
        if (m_input->IsKeyDown(PKey::E))
            transform.position += up * m_deltaTime * currentSpeed;
        if (m_input->IsKeyDown(PKey::Q))
            transform.position -= up * m_deltaTime * currentSpeed;


        if (m_input->IsMouseButtonDown(MouseButton::RMB))
        {
            float pitchDelta = m_input->GetMouseDelta().y * camera.turnSpeed;
            float yawDelta = m_input->GetMouseDelta().x * camera.turnSpeed;

            ApplyPitchYawRotation(entity, pitchDelta, yawDelta);
        }

        if (m_input->IsMouseButtonDown(MouseButton::MMB))
        {
            glm::vec2 currentMousePos = m_input->GetMousePosition();
            glm::vec2 mouseDelta = m_input->GetMouseDelta();

            float panSensitivity = 0.05f;

            glm::vec3 rightVector = GetRightVector(entity);
            glm::vec3 upVector = GetUpVector(entity);

            glm::vec3 panMovement = rightVector * (mouseDelta.x * panSensitivity) +
                upVector * (-mouseDelta.y * panSensitivity);

            transform.position += panMovement;
        }
    }
    else
    {
        if (m_input->IsKeyDown(PKey::W))
            transform.position += up * m_deltaTime * currentSpeed * (camera.orthoSize / 20);
        if (m_input->IsKeyDown(PKey::S))
            transform.position -= up * m_deltaTime * currentSpeed * (camera.orthoSize / 20);
        if (m_input->IsKeyDown(PKey::D))
            transform.position += right * m_deltaTime * currentSpeed * (camera.orthoSize / 20);
        if (m_input->IsKeyDown(PKey::A))
            transform.position -= right * m_deltaTime * currentSpeed * (camera.orthoSize / 20);
        if (m_input->IsKeyDown(PKey::E))
            transform.position += forward * m_deltaTime * currentSpeed * (camera.orthoSize / 20);
        if (m_input->IsKeyDown(PKey::Q))
            transform.position -= forward * m_deltaTime * currentSpeed * (camera.orthoSize / 20);

        if (m_input->IsMouseButtonDown(MouseButton::MMB))
        {
            glm::vec2 currentMousePos = m_input->GetMousePosition();
            glm::vec2 mouseDelta = m_input->GetMouseDelta();

            float panSensitivity = 0.05f;
            panSensitivity *= (camera.orthoSize / 10.0f);
         
            glm::vec3 rightVector = GetRightVector(entity);
            glm::vec3 upVector = GetUpVector(entity);

            glm::vec3 panMovement = rightVector * (mouseDelta.x * panSensitivity) +
                upVector * (-mouseDelta.y * panSensitivity);

            transform.position += panMovement;
        }
    }
}

glm::mat4 CameraModule::GetViewMatrixWithoutOffset(flecs::entity camera)
{
    auto camComp = camera.try_get<CameraComponent>();
    auto transform = camera.try_get<TransformComponent>();
    if (!camComp || !transform) return glm::mat4(1.0f);

    glm::vec3 forward = GetForwardVector(camera);
    glm::vec3 up = GetUpVector(camera);

    return glm::lookAt(transform->position, transform->position + forward, up);
}

glm::mat4 CameraModule::GetViewMatrix(flecs::entity camera)
{
    auto camComp = camera.try_get<CameraComponent>();
    auto transform = camera.try_get<TransformComponent>();
    if (!camComp || !transform) return glm::mat4(1.0f);

    glm::vec3 forward = GetForwardVector(camera);
    glm::vec3 up = GetUpVector(camera);

    glm::mat4 viewMatrix = glm::lookAt(transform->position, transform->position + forward, up);

    if (camComp->enableSnapping && camComp->projectionMode == ProjectionMode::ORTHOGRAPHIC)
    {
        glm::vec2 offset = GetTexelGrid(camera);
        viewMatrix[3] -= glm::vec4(offset, 0, 0);
    }

    return viewMatrix;
}

glm::mat4 CameraModule::GetProjectionMatrix(flecs::entity camera)
{
    if (!camera.has<CameraComponent>()) return glm::mat4(1.0f);

    auto camComp = camera.try_get<CameraComponent>();

   // glm::vec2 resolution = camComp->resolution;
    glm::vec2 resolution = Cauda::Application::Get().GetGameViewportSize();

    float aspect = resolution.x / resolution.y;

    if (camComp->projectionMode == ProjectionMode::PERSPECTIVE)
    {
        return glm::perspective(glm::radians(camComp->fov), aspect, camComp->nearPlane, camComp->farPlane);
    }
    else
    {
        float halfWidth = camComp->orthoSize * aspect;
        float halfHeight = camComp->orthoSize;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight,
            camComp->nearPlane, camComp->farPlane);
    }
}

glm::mat4 CameraModule::GetCameraMatrix(flecs::entity camera)
{
    if (!camera.has<TransformComponent>())
        return glm::mat4(1.0f);

    auto* transform = camera.try_get<TransformComponent>();

    glm::mat4 cameraMatrix = glm::mat4_cast(transform->rotation);
    cameraMatrix[3] = glm::vec4(transform->position, 1.0f);

    return cameraMatrix;
}

glm::vec3 CameraModule::GetPosition(flecs::entity camera)
{
    auto* transform = camera.try_get<TransformComponent>();
    return transform ? transform->position : glm::vec3(0.0f);
}

glm::vec3 CameraModule::GetForwardVector(flecs::entity camera)
{
    auto* transform = camera.try_get<TransformComponent>();
    if (!transform) return glm::vec3(0, 0, -1);

    return transform->rotation * glm::vec3(0, 0, -1);
}

glm::vec3 CameraModule::GetRightVector(flecs::entity camera)
{
    auto* transform = camera.try_get<TransformComponent>();
    if (!transform) return glm::vec3(1, 0, 0);

    return transform->rotation * glm::vec3(1, 0, 0);
}

glm::vec3 CameraModule::GetUpVector(flecs::entity camera)
{
    // Maybe switch to using world up for view matrix?
    //return glm::vec3(0, 1, 0);

    auto* transform = camera.try_get<TransformComponent>();
    if (!transform) return glm::vec3(0, 1, 0);

    return transform->rotation * glm::vec3(0, 1, 0);
}

ProjectionMode CameraModule::GetProjectionMode(flecs::entity camera)
{
    auto* camComp = camera.try_get<CameraComponent>();
    if (!camera) return ProjectionMode();
    
    return camComp->projectionMode;
}

void CameraModule::SetPosition(flecs::entity camera, const glm::vec3& position)
{
    auto* transform = camera.try_get_mut<TransformComponent>();
    if (transform)
    {
        transform->position = position;
    }
}

void CameraModule::SetLookDirection(flecs::entity camera, const glm::vec3& forward, const glm::vec3& up)
{
    auto* transform = camera.try_get_mut<TransformComponent>();
    if (!transform) return;

    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 correctedUp = glm::cross(right, forward);

    glm::mat3 rotMatrix(right, correctedUp, -forward);
    transform->rotation = glm::quat_cast(rotMatrix);
}

void CameraModule::SetFromViewMatrix(flecs::entity camera, const glm::mat4& viewMatrix)
{
    auto* transform = camera.try_get_mut<TransformComponent>();
    auto* camComp = camera.try_get_mut<CameraComponent>();
    if (!transform || !camComp) return;

    glm::mat4 worldTransform = glm::inverse(viewMatrix);

    glm::vec3 newPosition = glm::vec3(worldTransform[3]);
    worldTransform[3] -= glm::vec4(newPosition, 0);
    glm::quat newRotation = glm::quat_cast(worldTransform);
    glm::vec3 newEuler = Math::GetEulerAngles(newRotation);
    //if (newEuler.x < -89.5f || newEuler.x > 90.f)
    //    newEuler = Math::GetEulerAngles(glm::conjugate(newRotation));

    transform->position = newPosition;
    SetPitchYaw(camera, newEuler.x, newEuler.y);

    //transform->position = glm::vec3(viewMatrix[3]);
    //transform->rotation = glm::quat_cast(viewMatrix);
    //glm::vec3 euler = glm::eulerAngles(transform->rotation);
    //camComp->cachedPitch = glm::degrees(euler.x);
    //camComp->cachedYaw = glm::degrees(euler.y);
}

float CameraModule::CalculateArmLengthToPlane(flecs::entity camera, const glm::vec3& planePoint, const glm::vec3& planeNormal)
{
    auto* transform = camera.try_get<TransformComponent>();
    if (!transform) return 10.0f; 

    glm::vec3 direction = GetForwardVector(camera);
    float denom = glm::dot(direction, planeNormal);

    if (std::abs(denom) < 1e-6)
    {
        float distanceToPlanePoint = glm::length(planePoint - transform->position);
        return glm::clamp(distanceToPlanePoint, 1.0f, 100.0f);
    }

    float t = glm::dot(planePoint - transform->position, planeNormal) / denom;

    if (t <= 0.0f)
    {
        float distanceToPlanePoint = glm::length(planePoint - transform->position);
        return glm::clamp(distanceToPlanePoint, 1.0f, 100.0f);
    }

    return glm::clamp(t, 1.0f, 100.0f);
}

void CameraModule::UpdateSnapSpace(flecs::entity entity, CameraComponent& camera, TransformComponent& transform)
{
    if (!camera.enableSnapping) return;

    glm::mat4 currentRotation = glm::mat4_cast(transform.rotation);

    if (camera.prevRotation != currentRotation)
    {
        camera.prevRotation = currentRotation;

        camera.snapSpace = currentRotation;
    }
}

glm::vec2 CameraModule::GetTexelGrid(flecs::entity camera)
{
    auto* camComp = camera.try_get_mut<CameraComponent>();
    auto* transform = camera.try_get_mut<TransformComponent>();
    if (!camComp || !transform) return glm::vec2(0.0f);

    UpdateSnapSpace(camera, *camComp, *transform);

    Cauda::Application& app = Cauda::Application::Get();
    const glm::vec2 screenSize = app.GetScreenSize();

    //glm::vec2 resolution = camComp->resolution;
    glm::vec2 resolution = Cauda::Application::Get().GetGameViewportSize();

    float pixScale = camComp->pixelScale > 0.0f ? camComp->pixelScale : 1.0f;
    pixScale *= std::min(screenSize.x / resolution.x, screenSize.y / resolution.y);
    const float targetWidth = resolution.x / pixScale;
    const float targetHeight = resolution.y / pixScale;
    float texelSize = (2.0f * camComp->orthoSize) / targetHeight;

    glm::vec3 snapSpacePosition = glm::vec3(
        glm::dot(transform->position, glm::vec3(camComp->snapSpace[0])),
        glm::dot(transform->position, glm::vec3(camComp->snapSpace[1])),
        glm::dot(transform->position, glm::vec3(camComp->snapSpace[2]))
    );

    glm::vec3 snappedSnapSpacePosition = glm::round(snapSpacePosition / texelSize) * texelSize;
    glm::vec3 snapError = snappedSnapSpacePosition - snapSpacePosition;

    return glm::vec2(snapError.x, snapError.y);
}

glm::vec2 CameraModule::GetDisplaySmoothingOffset(flecs::entity camera, glm::vec2 screenSize, glm::vec2 gameSize)
{
    auto* camComp = camera.try_get<CameraComponent>();
    if (!camComp || !camComp->smoothDisplay) return glm::vec2(0.0f);

    glm::vec2 snapError = GetTexelGrid(camera);

    float texelSize = (2.0f * camComp->orthoSize) / gameSize.y;
    glm::vec2 texelError = glm::vec2(snapError.x, snapError.y) / texelSize;

    glm::vec2 displayScale = screenSize / gameSize;
    float displayScaleMin = std::min(displayScale.x, displayScale.y);
    glm::vec2 pixelError = texelError * displayScaleMin;

    bool isIntegerScale = (abs(displayScale.x - round(displayScale.x)) < 0.001f &&
        abs(displayScale.y - round(displayScale.y)) < 0.001f);
    if (isIntegerScale && !camComp->subPixelAtIntegerScale)
    {
        pixelError = glm::round(pixelError);
    }

    return pixelError;
}

glm::vec2 CameraModule::GetPixelOffset(flecs::entity camera)
{
    auto* camComp = camera.try_get<CameraComponent>();
    if (!camComp || !camComp->enableSnapping)
        return glm::vec2(0.0f);

    glm::vec2 screenSize = Cauda::Application::Get().GetScreenSize();

    //glm::vec2 resolution = camComp->resolution;
    glm::vec2 resolution = Cauda::Application::Get().GetGameViewportSize();

    glm::vec2 snapError = GetTexelGrid(camera);
    float pixScale = camComp->pixelScale > 0.0f ? camComp->pixelScale : 1.0f;
    pixScale *= std::min(screenSize.x / resolution.x, screenSize.y / resolution.y);

    glm::vec2 gameSize = resolution / pixScale;
    float texelSize = (2.0f * camComp->orthoSize) / gameSize.y;

    glm::vec2 texelError = glm::vec2(snapError.x / texelSize, snapError.y / texelSize);
    glm::vec2 pixelError = texelError * pixScale;

    return pixelError;
}

void CameraModule::OnMouseClick(MouseButton button, int clicks)
{
}

void CameraModule::OnMouseRelease(MouseButton mouseButton)
{
}

void CameraModule::OnMouseScroll(float amount)
{
    if (m_input->IsKeyDown(PKey::LAlt))
    {
        auto mainCam = GetMainCamera();
        if (mainCam.is_alive())
        {
            auto* camComp = mainCam.try_get_mut<CameraComponent>();
            auto* transform = mainCam.try_get_mut<TransformComponent>();
            if (camComp && transform)
            {
                if (camComp->projectionMode == ProjectionMode::ORTHOGRAPHIC)
                {
                    float zoomDelta = amount > 0 ? -camComp->zoomSpeed : camComp->zoomSpeed;
                    camComp->orthoSize = glm::clamp(camComp->orthoSize + zoomDelta, 0.1f, 70.0f);
                }
                else
                {
                    glm::vec3 forward = GetForwardVector(mainCam);
                    float zoomDistance = amount > 0 ? camComp->zoomSpeed * 2.0f : -camComp->zoomSpeed * 2.0f;
                    transform->position += forward * zoomDistance;
                }
            }
        }
    }
}

void CameraModule::OnKeyPress(PKey key)
{
}

void CameraModule::OnKeyRelease(PKey key)
{
}

void CameraModule::OnImGuiRender()
{
    if (m_showCameraWindow)
    {
        ImGui::Begin("Camera System", &m_showCameraWindow);

        ImGui::Text("Active Cameras: %d", (int)m_updateQuery.count());

        ImGui::Separator();

        ImGui::End();
    }
}

void CameraModule::DrawCameraInspector(flecs::entity entity, CameraComponent& component)
{
    if (ImGui::BeginTable("##camera_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        // --- Projection Settings ---
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        bool projectionOpen = ImGui::TreeNodeEx("Projection", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns);

        if (projectionOpen)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Projection Mode");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            const char* projectionModes[] = { "Perspective", "Orthographic" };
            int currentMode = static_cast<int>(component.projectionMode);
            if (ImGui::Combo("##ProjectionMode", &currentMode, projectionModes, 2))
            {
                component.projectionMode = static_cast<ProjectionMode>(currentMode);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("FOV");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##FOV", &component.fov, 1.0f, 1.0f, 179.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Near Plane");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##NearPlane", &component.nearPlane, 0.1f, 0.01f, 10.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Far Plane");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##FarPlane", &component.farPlane, 1.0f, 10.0f, 1000.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Ortho Size");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##OrthoSize", &component.orthoSize, 0.1f, 0.1f, 100.0f);

            ImGui::TreePop();
        }

        // --- Control Settings ---
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        bool controlOpen = ImGui::TreeNodeEx("Control", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns);

        if (controlOpen)
        {
            bool shouldUpdatePitchYaw = false;
            //bool shouldUpdateYaw = false;

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Pitch");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            float displayedPitch = component.cachedPitch;
            if (ImGui::DragFloat("##Pitch", &displayedPitch, 1.0f, -89.0f, 89.0f))
            {
                //UpdatePitch(entity, displayedPitch);
                shouldUpdatePitchYaw = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Yaw");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            float displayedYaw = component.cachedYaw;
            if (ImGui::DragFloat("##Yaw", &displayedYaw, 1.0f))
            {
                //UpdateYaw(entity, displayedYaw);
                shouldUpdatePitchYaw = true;
            }

            if (shouldUpdatePitchYaw)
            {
                SetPitchYaw(entity, displayedPitch, displayedYaw);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Movement Speed");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##MovementSpeed", &component.movementSpeed, 1.0f, 0.1f, 100.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Turn Speed");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##TurnSpeed", &component.turnSpeed, 0.01f, 0.01f, 2.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Zoom Speed");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##ZoomSpeed", &component.zoomSpeed, 0.1f, 0.1f, 5.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Enable Movement");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##EnableMovement", &component.enableMovement);

            ImGui::TreePop();
        }

        // --- Pixel Perfect Settings ---
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        bool pixelPerfectOpen = ImGui::TreeNodeEx("Pixel Perfect", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns);

        if (pixelPerfectOpen)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Enable Snapping");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##EnableSnapping", &component.enableSnapping);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Snap Objects");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##SnapObjects", &component.snapObjects);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Smooth Display");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##SmoothDisplay", &component.smoothDisplay);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Sub-pixel at Integer Scale");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##SubPixelAtIntegerScale", &component.subPixelAtIntegerScale);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Pixel Scale");
            ImGui::TableNextColumn();

            ImGui::SetNextItemWidth(80.0f);
            
            if (ImGui::DragFloat("##PixelScale", &component.pixelScale, 0.01f, 0.1f, 10.0f))
            {
               // entity.modified<CameraComponent>();
                component.isResolutionDirty = true;
            }

            ImGui::SameLine();
            {
                if (ImGui::Button("1x")) component.pixelScale = 1.0f;
                //entity.modified<CameraComponent>();
                component.isResolutionDirty = true; 
            }
            ImGui::SameLine();
            {
                if (ImGui::Button("2x")) component.pixelScale = 2.0f;
                //entity.modified<CameraComponent>();
                component.isResolutionDirty = true;
            }
            ImGui::SameLine();
            {
                if (ImGui::Button("3x")) component.pixelScale = 3.0f;
                //entity.modified<CameraComponent>();
                component.isResolutionDirty = true;
            }
            ImGui::SameLine();
            {
                if (ImGui::Button("4x")) component.pixelScale = 4.0f;
                //entity.modified<CameraComponent>();
                component.isResolutionDirty = true;
            }

            ImGui::TreePop();
        }

        // --- Follow Settings ---
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        bool followOpen = ImGui::TreeNodeEx("Camera Follow", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns);

        if (followOpen)
        {
            std::vector<const char*> followableNames;
            std::vector<flecs::entity> followableEntities;
            m_followableQuery.each([&](flecs::entity e, TransformComponent&)
                {
                    if (e != entity)
                    {
                        followableEntities.push_back(e);
                        followableNames.push_back(e.name() ? e.name().c_str() : "Unnamed Entity");
                    }
                });

            const char* previewName = !component.followTarget.is_alive() ? "Root" : component.followTarget.name();
            int currentItem = 0;
            if (component.followTarget && component.followTarget.is_alive()) {
                previewName = component.followTarget.name();
                for (size_t i = 0; i < followableEntities.size(); ++i) {
                    if (followableEntities[i] == component.followTarget) {
                        currentItem = i;
                        previewName = followableNames[currentItem];
                        break;
                    }
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Follow Target");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##FollowTarget", previewName))
            {
                for (int i = 0; i < followableNames.size(); ++i) {
                    if (ImGui::Selectable(followableNames[i], currentItem == i)) {
                        std::cout << "Selected cam follow: " << followableEntities[i].name() << '\n';
                        if (i == 0) ClearCameraFollow(entity);
                        else SetCameraFollow(entity, followableEntities[i], component.followMode);
                    }
                    if (currentItem == i) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            bool hasTarget = component.followTarget && component.followTarget.is_alive();
            if (!hasTarget) ImGui::BeginDisabled();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Follow Mode");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            const char* followModes[] = { "None", "Top-Down", "First-Person" };
            int currentFollowMode = static_cast<int>(component.followMode);
            if (ImGui::Combo("##FollowMode", &currentFollowMode, followModes, 3)) {
                component.followMode = static_cast<CameraFollowMode>(currentFollowMode);
                if (component.followMode == CameraFollowMode::NONE) ClearCameraFollow(entity);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Follow Offset");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat3("##FollowOffset", glm::value_ptr(component.followOffset), 0.1f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Follow Speed");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##FollowSpeed", &component.followSpeed, 1.0f, 0.1f, 100.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Follow Distance");
            ImGui::TableNextColumn(); ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##FollowDistance", &component.followDistance, 1.0f, 1.0f, 200.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Smooth Follow");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##SmoothFollow", &component.smoothFollow);

            if (!hasTarget) ImGui::EndDisabled();

            ImGui::TreePop();
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button("Set as Main Camera", ImVec2(-FLT_MIN, 0)))
    {
        SetMainCamera(entity);
    }
}

ImVec4 CameraModule::GetCameraEntityColour(flecs::entity entity)
{
    if (entity.has<CameraComponent>())
    {
        if (entity.has<MainCamera>())
        {
            return ImVec4(0.85f, 0.5f, 0.0f, 1.0f);
        }
        return ImVec4(0.65f, 0.65f, 0.85f, 1.0f);
    }
    return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}