#include "ViewPointClass.h"

void ViewPointClass::SetPosition(float x, float y, float z)
{
    m_position = { x, y, z };
}

void ViewPointClass::SetLookAt(float x, float y, float z)
{
    m_lookAt = { x, y, z };
}

void ViewPointClass::SetProjectionParameters(float fov, float aspect, float nearZ, float farZ)
{
    m_fov = fov;
    m_aspect = aspect;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

void ViewPointClass::GenerateViewMatrix()
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR lookAt = XMLoadFloat3(&m_lookAt);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    m_viewMatrix = XMMatrixLookAtLH(pos, lookAt, up);
}

void ViewPointClass::GenerateProjectionMatrix()
{
    m_projectionMatrix = XMMatrixPerspectiveFovLH(m_fov, m_aspect, m_nearZ, m_farZ);
}

void ViewPointClass::GetViewMatrix(XMMATRIX& out)
{
    out = m_viewMatrix;
}

void ViewPointClass::GetProjectionMatrix(XMMATRIX& out)
{
    out = m_projectionMatrix;
}