#pragma once

#include "Core/Input/InputSystem.h"
#include "Core/Components/Components.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "imgui.h"
#include <ThirdParty/flecs.h>
#include <memory>
#include <string>
#include <unordered_set>

class EditorModule;
struct TransformComponent;
class ShaderProgram;

enum ProjectionMode
{
    PERSPECTIVE,
    ORTHOGRAPHIC
};

enum CameraFollowMode
{
    NONE,
    TOP_DOWN,
    FIRST_PERSON
};

struct CameraComponent
{
    ProjectionMode projectionMode = ProjectionMode::ORTHOGRAPHIC;
    float fov = 60.0f;
    float nearPlane = 0.3f;
    float farPlane = 200.0f;
    float orthoSize = 10.0f;

    float movementSpeed = 20.0f;
    float turnSpeed = 0.2f;
    float zoomSpeed = 0.5f;
    bool enableMovement = true;
    bool enableSnapping = true;

    CameraFollowMode followMode = CameraFollowMode::NONE;
    glm::vec3 followOffset = glm::vec3(0, 0, 0);
    float followSpeed = 50.0f;
    float followDistance = 50.0f;
    bool smoothFollow = false;


    bool snapObjects = true;
    bool smoothDisplay = true;
    bool subPixelAtIntegerScale = true;

    glm::vec2 resolution = glm::vec2(1920.0f, 1080.0f);
    float pixelScale = 1.0f;

    std::string followTargetName = "Root";

    //Non-Serialised
    bool isResolutionDirty = true;

    /*float yaw = 0.0f;
    float pitch = 0.0f;*/
    float cachedPitch = 0.0f;
    float cachedYaw = 0.0f;

    flecs::entity followTarget = flecs::entity::null();
    glm::mat4 prevRotation = glm::mat4(1.0f);
    glm::mat4 snapSpace = glm::mat4(1.0f);
    glm::vec2 offset = glm::vec2(0.0f);
};

struct MainCamera {};

class CameraModule : public IKeyListener, public IMouseListener
{
public:
    CameraModule(flecs::world& world);
    ~CameraModule();

    CameraModule(const CameraModule&) = delete;
    CameraModule& operator=(const CameraModule&) = delete;
    CameraModule(CameraModule&&) = delete;
    CameraModule& operator=(CameraModule&&) = delete;

    //void Update(float deltaTime) {}
    void OnImGuiRender();

    void RemoveCamera(flecs::entity entity);

    flecs::entity GetMainCamera();
    void SetMainCamera(flecs::entity camera);

    void SetCameraFollow(flecs::entity camera, flecs::entity target, CameraFollowMode mode = CameraFollowMode::TOP_DOWN);
    void ClearCameraFollow(flecs::entity camera);
    flecs::entity GetFollowTarget(flecs::entity camera) const;
    void ResolveFollowTargets();

    void SetPitchYaw(flecs::entity camera, float pitch, float yaw);
    void ApplyPitchYawRotation(flecs::entity camera, float deltaPitch, float deltaYaw);

    glm::mat4 GetViewMatrixWithoutOffset(flecs::entity camera);
    glm::mat4 GetViewMatrix(flecs::entity camera);
    glm::mat4 GetProjectionMatrix(flecs::entity camera);
    glm::mat4 GetCameraMatrix(flecs::entity camera);

    glm::vec3 GetPosition(flecs::entity camera);
    glm::vec3 GetForwardVector(flecs::entity camera);
    glm::vec3 GetRightVector(flecs::entity camera);
    glm::vec3 GetUpVector(flecs::entity camera);

    ProjectionMode GetProjectionMode(flecs::entity camera);

    void SetPosition(flecs::entity camera, const glm::vec3& position);
    void SetLookDirection(flecs::entity camera, const glm::vec3& forward, const glm::vec3& up);
    void SetFromViewMatrix(flecs::entity camera, const glm::mat4& viewMatrix);

    float CalculateArmLengthToPlane(flecs::entity camera, const glm::vec3& planePoint, const glm::vec3& planeNormal);
    glm::vec2 GetTexelGrid(flecs::entity camera);

    glm::vec2 GetDisplaySmoothingOffset(flecs::entity camera, glm::vec2 screenSize, glm::vec2 gameSize);

    glm::vec2 GetPixelOffset(flecs::entity camera);

    virtual void OnMouseClick(MouseButton button, int clicks) override;
    virtual void OnMouseRelease(MouseButton mouseButton)  override;
    virtual void OnMouseScroll(float amount)  override;

    virtual void OnKeyPress(PKey key) override;
    virtual void OnKeyRelease(PKey key) override;

private:
    flecs::world& m_world;
    InputSystem* m_input;
    EditorModule* m_editorModule = nullptr;

    float m_deltaTime = 0.0f;
    bool m_showCameraWindow = true;

    std::unordered_set<std::string> m_persistentEntityNames;

    flecs::system m_updateSystem;
    flecs::system m_resolveFollowTargetSystem;
    flecs::observer m_cleanupFollowTargetObserver;
    flecs::query<CameraComponent, TransformComponent> m_updateQuery;
    flecs::query<MainCamera> m_mainCameraQuery;
    flecs::query<CameraComponent, TransformComponent> m_followCameraQuery;
    flecs::query<TransformComponent> m_followableQuery;

    const char* GetPersistentEntityName(flecs::entity entity);

    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();
    void RegisterWithEditor();

    void UpdateCachedPitchYaw(flecs::entity entity, CameraComponent& camera, const TransformComponent& transform);

    void UpdateCameraInput(flecs::entity entity, CameraComponent& camera, TransformComponent& transform);
    void UpdateCameraFollow(flecs::entity entity, CameraComponent& camera, TransformComponent& transform);
    void UpdateSnapSpace(flecs::entity entity, CameraComponent& camera, TransformComponent& transform);

    void UpdateTopDown(flecs::entity entity, const glm::vec3& targetPos, CameraComponent& camera, TransformComponent& transform);
    void UpdateFirstPerson(flecs::entity entity, const glm::vec3& targetPos, CameraComponent& camera, TransformComponent& transform);

    void HandleCameraInput(flecs::entity entity, CameraComponent& camera, TransformComponent& transform);

    void DrawCameraInspector(flecs::entity entity, CameraComponent& component);
    ImVec4 GetCameraEntityColour(flecs::entity entity);
};