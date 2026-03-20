#pragma once
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "MaterialClass.h"
using namespace DirectX;

struct TransformComponent {
    XMFLOAT3 position = { 0,0,0 };
    XMFLOAT3 rotation = { 0,0,0 };
    XMFLOAT3 scale = { 1,1,1 };
    XMMATRIX GetWorldMatrix() const
    {
        XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
        XMMATRIX rotX = XMMatrixRotationX(rotation.x);
        XMMATRIX rotY = XMMatrixRotationY(rotation.y);
        XMMATRIX rotZ = XMMatrixRotationZ(rotation.z);
        XMMATRIX rotationMatrix = rotX * rotY * rotZ;
        XMMATRIX translationMatrix = XMMatrixTranslation(position.x, position.y, position.z);
        return scaleMatrix * rotationMatrix * translationMatrix;
    }



};


struct LightComponent {
    XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // RGB + интенсивность (w=0 → выкл)
    float     radius = 10.0f;                        // радиус влияния (для будущего attenuation)
    bool      enabled = true;
};
struct MeshRendererComponent {
	AssimpModelClass* model = nullptr; // Указатель на модель
	MaterialClass* material = nullptr; // Указатель на материал
};

struct ActiveComponent {
    

};

