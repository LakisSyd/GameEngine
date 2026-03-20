#pragma once
#include <DirectXMath.h>


using namespace DirectX;

class ViewPointClass
{
public:
    void SetPosition(float x, float y, float z);
    void SetLookAt(float x, float y, float z);
    void SetProjectionParameters(float fov, float aspect, float nearZ, float farZ);

    void GenerateViewMatrix();
    void GenerateProjectionMatrix();

    void GetViewMatrix(XMMATRIX& out);
    void GetProjectionMatrix(XMMATRIX& out);

private:
    XMMATRIX m_viewMatrix = XMMatrixIdentity();
    XMMATRIX m_projectionMatrix = XMMatrixIdentity();
    XMFLOAT3 m_position = { 0.0f, 0.0f, -1.0f };
    XMFLOAT3 m_lookAt = { 0.0f, 0.0f,  0.0f };
    float    m_fov = XM_PIDIV4;
    float    m_aspect = 1.0f;
    float    m_nearZ = 0.1f;
    float    m_farZ = 100.0f;
  
};