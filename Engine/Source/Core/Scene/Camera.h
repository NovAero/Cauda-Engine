#pragma once
#include "Renderer/Graphics.h"
#include "Core/Input/Input.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

class InputSystem;

class Camera : IKeyListener, IMouseListener {
public:
    enum class ProjectionMode {
        PERSPECTIVE,
        ORTHOGRAPHIC
    };

    Camera(glm::vec3 position = glm::vec3(-10.0f, 10.0f, -10.0f), float yaw = 45.0f, float pitch = -30.0f, unsigned int screenWidth = 1280, unsigned int screenHeight = 720);

    void Update(float deltaTime);
    void HandleInput();
    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(float width, float height);

    void SetPosition(glm::vec3 position) { m_position = position; }
    void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
    void SetTurnSpeed(float speed) { m_turnSpeed = speed; }
    void SetZoomSpeed(float speed) { m_zoomSpeed = speed; }
    void SetProjectionMode(ProjectionMode mode) { m_projectionMode = mode; }
    void SetFOV(float fov) { m_fov = fov; }
    void SetNearPlane(float nearPlane) { m_nearPlane = nearPlane; }
    void SetFarPlane(float farPlane) { m_farPlane = farPlane; }
    void SetOrthographicSize(float size) { m_orthoSize = size; }
    void SetScreenSize(unsigned int width, unsigned int height) { m_screenWidth = width; m_screenHeight = height; }

    glm::mat4 GetCameraMatrix() const;

    void SetFromMatrix(const glm::mat4& matrix);

    void SetLookDirection(const glm::vec3& forward, const glm::vec3& up);
    float CalculateArmLengthToPlane(const glm::vec3& planePoint, const glm::vec3& planeNormal) const;

    glm::vec3 GetPosition() const { return m_position; }
    glm::vec3 GetForwardVector() const;
    glm::vec3 GetRightVector() const;
    glm::vec3 GetUpVector() const;
    void UpdateSnapSpace();
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    ProjectionMode GetProjectionMode() const { return m_projectionMode; }
    float GetFOV() const { return m_fov; }
    float GetOrthographicSize() const { return m_orthoSize; }
    glm::vec2 GetTexelGrid();


    virtual void OnMouseClick(MouseButton button, int clicks) override;
    virtual void OnMouseRelease(MouseButton mouseButton)  override;
    virtual void OnMouseScroll(float amount)  override;

    virtual void OnKeyPress(PKey key) override;
    virtual void OnKeyRelease(PKey key) override;

private:
    glm::mat4 m_prevRotation;
    glm::mat4 m_snapSpace;
    glm::vec3 m_position;
    glm::vec3 m_snappedPosition;
    float m_yaw;                  // Horizontal 
    float m_pitch;                // Vertical 
    bool m_enableSnapping = true;
    bool m_enableMovement = false;

    float m_movementSpeed = 20.0f;
    float m_turnSpeed = 0.1f;
    float m_zoomSpeed = 0.1f;
    ProjectionMode m_projectionMode = ProjectionMode::ORTHOGRAPHIC;

    float m_fov = 90.0f;
    float m_nearPlane = .3f;
    float m_farPlane = 200.0f;

    float m_orthoSize = 10.0f;
    unsigned int m_screenWidth;
    unsigned int m_screenHeight;

    glm::vec2 m_offset;
    float m_resolutionScale = 25.f;
    float m_deltaTime;
    InputSystem* m_input;
};