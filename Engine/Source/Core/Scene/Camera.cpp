#include "cepch.h"
#include "Camera.h"
#include "Core/Input/InputSystem.h"

#include <iostream>

Camera::Camera(glm::vec3 position, float theta, float phi, unsigned int screenWidth, unsigned int screenHeight)
    : m_position(position), m_yaw(theta), m_pitch(phi), m_screenWidth(screenWidth), m_screenHeight(screenHeight)
{
    m_input = InputSystem::Inst();

    if (m_input) 
    {
        m_input->AddKeyListener(this);
        m_input->AddMouseListener(this);
    }
}

void Camera::Update(float deltaTime)
{
    m_deltaTime = deltaTime;

    if (m_input)
    {
        HandleInput();
    }
}

void Camera::HandleInput()
{
    if (m_enableMovement)
    {
        float currentSpeed = m_movementSpeed;
        if (m_input->IsKeyDown(PKey::LShift))
        {
            currentSpeed *= 2.5f;
        }

        glm::vec3 forward = GetForwardVector();
        glm::vec3 right = GetRightVector();
        glm::vec3 up = GetUpVector();

        if (m_projectionMode == ProjectionMode::PERSPECTIVE)
        {
            if (m_input->IsKeyDown(PKey::W))
                m_position += forward * m_deltaTime * currentSpeed;
            if (m_input->IsKeyDown(PKey::S))
                m_position -= forward * m_deltaTime * currentSpeed;
            if (m_input->IsKeyDown(PKey::D))
                m_position += right * m_deltaTime * currentSpeed;
            if (m_input->IsKeyDown(PKey::A))
                m_position -= right * m_deltaTime * currentSpeed;
            if (m_input->IsKeyDown(PKey::E))
                m_position += up * m_deltaTime * currentSpeed;
            if (m_input->IsKeyDown(PKey::Q))
                m_position -= up * m_deltaTime * currentSpeed;
        }
        else
        {
            if (m_input->IsKeyDown(PKey::W))
                m_position += up * m_deltaTime * currentSpeed * (m_orthoSize / 20);
            if (m_input->IsKeyDown(PKey::S))
                m_position -= up * m_deltaTime * currentSpeed * (m_orthoSize / 20);
            if (m_input->IsKeyDown(PKey::D))
                m_position += right * m_deltaTime * currentSpeed * (m_orthoSize / 20);
            if (m_input->IsKeyDown(PKey::A))
                m_position -= right * m_deltaTime * currentSpeed * (m_orthoSize / 20);
            if (m_input->IsKeyDown(PKey::E))
                m_position += forward * m_deltaTime * currentSpeed * (m_orthoSize / 20);
            if (m_input->IsKeyDown(PKey::Q))
                m_position -= forward * m_deltaTime * currentSpeed * (m_orthoSize / 20);
        }

        glm::vec2 mouseDelta = InputSystem::GetMouseDelta();

        if (m_input->IsMouseButtonDown(MouseButton::RMB)) 
        {
            m_yaw += m_turnSpeed * mouseDelta.x;
            m_pitch -= m_turnSpeed * mouseDelta.y;
            m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
        }
    }
}

glm::mat4 Camera::GetViewMatrix()
{
    glm::mat4 viewMatrix = glm::lookAt(
        m_position,
        m_position + GetForwardVector(),
        glm::vec3(0, 1, 0)
    );

    if (m_enableSnapping && m_projectionMode == ProjectionMode::ORTHOGRAPHIC) 
    {
        glm::vec2 m_offset = GetTexelGrid();
        viewMatrix[3] -= glm::vec4(m_offset, 0, 0);
        // 0 1 2 3
        // 0 0 0 x
        // 0 0 0 y
        // 0 0 0 0
        // 0 0 0 0
    }

    return viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix(float width, float height) 
{
    float aspectRatio = width / height;

    if (m_projectionMode == ProjectionMode::PERSPECTIVE) 
    {
        return glm::perspective(glm::radians(m_fov), aspectRatio, m_nearPlane, m_farPlane);
    }
    else 
    {
        float orthoWidth = m_orthoSize * aspectRatio;
        return glm::ortho(-orthoWidth, orthoWidth, -m_orthoSize, m_orthoSize, m_nearPlane, m_farPlane);
    }
}

glm::mat4 Camera::GetCameraMatrix() const
{
    glm::mat4 cameraMatrix = glm::mat4(1.0f);

    cameraMatrix[0] = glm::vec4(GetRightVector(), 0.0f);
    cameraMatrix[1] = glm::vec4(GetUpVector(), 0.0f);
    cameraMatrix[2] = glm::vec4(GetForwardVector(), 0.0f); 

    cameraMatrix[3] = glm::vec4(m_position, 1.0f);

    return cameraMatrix;
}

void Camera::SetFromMatrix(const glm::mat4& matrix)
{
    m_position = glm::vec3(matrix[3]);

    glm::vec3 right = glm::normalize(glm::vec3(matrix[0]));
    glm::vec3 up = glm::normalize(glm::vec3(matrix[1]));
    glm::vec3 forward = -glm::normalize(glm::vec3(matrix[2])); 

    m_pitch = glm::degrees(asin(forward.y));
    m_yaw = glm::degrees(atan2(forward.z, forward.x));
}

void Camera::SetLookDirection(const glm::vec3& forward, const glm::vec3& up)
{
    glm::vec3 normForward = glm::normalize(forward);
    m_pitch = glm::degrees(asin(normForward.y));
    m_yaw = glm::degrees(atan2(normForward.z, normForward.x));
}

float Camera::CalculateArmLengthToPlane(const glm::vec3& planePoint, const glm::vec3& planeNormal) const
{
    glm::vec3 direction = GetForwardVector();
    float denom = glm::dot(direction, planeNormal);

    if (std::abs(denom) < 1e-6) {
        if (std::abs(glm::dot(m_position - planePoint, planeNormal)) < 1e-6) {
            return 0.0f;
        }
        else {
            return INFINITY;
        }
    }

    float t = glm::dot(planePoint - m_position, planeNormal) / denom;

    if (t < 0) {
        return INFINITY;
    }

    return t;
}

glm::vec3 Camera::GetForwardVector() const
{
    float thetaR = glm::radians(m_yaw);
    float phiR = glm::radians(m_pitch);
    return glm::normalize(glm::vec3(
        cos(phiR) * cos(thetaR),
        sin(phiR),
        cos(phiR) * sin(thetaR)
    ));
}

glm::vec3 Camera::GetRightVector() const 
{
    float thetaR = glm::radians(m_yaw);
    return glm::normalize(glm::vec3(-sin(thetaR), 0, cos(thetaR)));
}

glm::vec3 Camera::GetUpVector() const 
{
    return glm::normalize(glm::cross(GetRightVector(), GetForwardVector()));
}

void Camera::UpdateSnapSpace()
{
    glm::mat4 currentRotation = glm::mat4(1.0f);
    currentRotation = glm::rotate(currentRotation, glm::radians(m_yaw), glm::vec3(0, 1, 0));
    currentRotation = glm::rotate(currentRotation, glm::radians(m_pitch), glm::vec3(1, 0, 0));

    if (m_prevRotation != currentRotation) 
    {
        m_prevRotation = currentRotation;

        m_snapSpace = glm::mat4(1.0f);
        m_snapSpace[0] = glm::vec4(GetRightVector(), 0.0f);
        m_snapSpace[1] = glm::vec4(GetUpVector(), 0.0f);
        m_snapSpace[2] = glm::vec4(GetForwardVector(), 0.0f);
    }
}

glm::vec2 Camera::GetTexelGrid()
{
    UpdateSnapSpace();

    const float targetWidth = 2560.f;
    const float targetHeight = 1440.f;
    float texelSize = (2.0f * m_orthoSize) / targetHeight;

    glm::vec3 snapSpacePosition = glm::vec3(
        glm::dot(m_position, glm::vec3(m_snapSpace[0])),
        glm::dot(m_position, glm::vec3(m_snapSpace[1])),
        glm::dot(m_position, glm::vec3(m_snapSpace[2]))
    );

    glm::vec3 snappedSnapSpacePosition = glm::round(snapSpacePosition / texelSize) * texelSize;

    glm::vec3 snapError = snappedSnapSpacePosition - snapSpacePosition;

    return glm::vec2(snapError.x, snapError.y);
}

void Camera::OnMouseClick(MouseButton button, int clicks)
{
}

void Camera::OnMouseRelease(MouseButton mouseButton)
{
}

void Camera::OnMouseScroll(float amount)
{
    float scrollDelta = amount;
    if (scrollDelta != 0.0f)
    {
        if (m_projectionMode == ProjectionMode::PERSPECTIVE)
        {
            m_fov -= scrollDelta * m_zoomSpeed * 5.0f;
            m_fov = glm::clamp(m_fov, 10.0f, 120.0f);
        }
        else
        {
            m_orthoSize -= scrollDelta * m_zoomSpeed * m_orthoSize;
            m_orthoSize = glm::clamp(m_orthoSize, 0.1f, 20.0f);
        }
    }
}
void Camera::OnKeyPress(PKey key)
{  
    if (key == PKey::P)
    {
         m_projectionMode = (m_projectionMode == ProjectionMode::PERSPECTIVE) ?
         ProjectionMode::ORTHOGRAPHIC : ProjectionMode::PERSPECTIVE;
    }
    else if (key == PKey::F)
	{
		m_enableMovement = !m_enableMovement;
	}
}

void Camera::OnKeyRelease(PKey key)
{
}
