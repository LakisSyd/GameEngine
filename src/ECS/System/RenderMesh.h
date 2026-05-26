#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "assimpmodelclass.h"
#include "MaterialClass.h"
#include "GameSettings.h"
#include "lightshaderclass.h"
#include "textureshaderclass.h"
#include "LightMapShaderClass.h"
#include "MultiTextureShaderClass.h"
#include "NormalMapShaderClass.h"
#include "FogShaderClass.h"
#include "clipplaneshaderclass.h"
#include "ReflectionShaderClass.h"
#include <colorshaderclass.h>
#include "WaterShaderClass.h"
#include "GlassShaderClass.h"
#include "ProjectionShaderClass.h"
#include "PBRShaderClass.h"
using namespace DirectX;

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

inline void RenderMesh(
    ID3D11DeviceContext* context,
    AssimpModelClass* model,
    const XMMATRIX& world,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    MaterialClass* material,
    XMFLOAT4             lightColors[],
    XMFLOAT4             lightPositions[],
    const Settings::FogSettings& fog,
    bool                 selected = false,
    XMFLOAT3             cameraPos = { 0.0f, 0.0f, 0.0f },
    float                deltaTime = 0.0f, AnimationComponent* animComp = nullptr)
{
    if (!model || !material) return;

    auto* shader = material->GetShader();
    if (!shader) return;

    if (animComp && deltaTime > 0.0f && model->HasAnimations() && shader->GetType() == ShaderType::PBR)
    {
      
        model->SetBoneMatricesFromComponent(context, *animComp);
    }


    // Передаём матрицы костей в шейдер перед рендерингом
  

    model->Render(context);

    switch (shader->GetType())
    {
    case ShaderType::Reflection:
    {
        auto* s = static_cast<ReflectionShaderClass*>(shader);
        auto* textureSRV = material->GetMainTexture() ? material->GetMainTexture()->GetTexture() : nullptr;
        s->Render(context, model->GetIndexCount(),
            world, view, projection,
            material->GetReflectionMatrix(),
            textureSRV, material->GetReflectionSRV(),
            material->GetReflectivity());
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, nullSRVs);
        break;
    }
    case ShaderType::Light:
    {
        auto* s = static_cast<LightShaderClass*>(shader);
        s->SetSelectedFactor(context, selected ? 2.0f : 1.0f);

        // Приоритет: 1) материал (редактор), 2) part.texture, 3) model->GetTexture()
        auto getTexSRV = [&](ID3D11ShaderResourceView* embedded) -> ID3D11ShaderResourceView*
            {
                // 1. Слот 0 из материала (назначен в редакторе) — наивысший приоритет
                if (auto* t0 = material->GetTexture(0))
                {
                    auto* srv = t0->GetTexture();
                    if (srv) return srv;
                }
                // 2. MainTexture из материала
                if (auto* main = material->GetMainTexture())
                {
                    auto* srv = main->GetTexture();
                    if (srv) return srv;
                }
                // 3. Вшитая текстура меша (из GLB)
                if (embedded) return embedded;

                return nullptr;
            };

        const auto& parts = model->GetMeshParts();

        bool hasEmbedded = false;
        for (const auto& part : parts)
        {
            if (part.texture || part.normalMap || part.metalness || part.roughness)
            {
                hasEmbedded = true;
                break;
            }
        }

        if (hasEmbedded)
        {
            UINT stride = sizeof(AssimpModelClass::VertexType);
            UINT offset = 0;
            for (const auto& part : parts)
            {
                if (!part.vertexBuffer || !part.indexBuffer) continue;

                context->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
                context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                ID3D11ShaderResourceView* srv = getTexSRV(part.texture);
                if (!srv)
                {
                    srv = model->GetTexture();
                }

                if (!srv)
                {
                    OutputDebugStringA("LightShader (part): SRV is NULL! Skipping render for this part.\n");
                    continue;
                }

                s->Render(context, part.indexCount,
                    world, view, projection,
                    srv, lightColors, lightPositions);

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
            }
        }
        else
        {
            ID3D11ShaderResourceView* textureSRV = getTexSRV(nullptr);
            if (!textureSRV)
            {
                textureSRV = model->GetTexture();
            }

            if (!textureSRV)
            {
                OutputDebugStringA("LightShader: Final fallback SRV is NULL! Object will be invisible/white.\n");
                break;
            }

            s->Render(context, model->GetIndexCount(),
                world, view, projection,
                textureSRV, lightColors, lightPositions);

            ID3D11ShaderResourceView* nullSRV = nullptr;
            context->PSSetShaderResources(0, 1, &nullSRV);
        }
        break;
    }
    case ShaderType::PBR:
    {
        auto* s = static_cast<PBRShaderClass*>(shader);
        const float selectedFactor = selected ? 2.0f : 1.0f;

        const bool  matIsGlass = material->IsGlass();
        const float matGlassAlpha = material->GetGlassOpacity();

        // Приоритет: 1) материал (редактор), 2) вшитая текстура меша, 3) nullptr (fallback в шейдере)
        auto getTexSRV = [&](int slot, ID3D11ShaderResourceView* embedded) -> ID3D11ShaderResourceView*
            {
                // Для слота 0 (albedo) — сначала проверяем материал
                if (auto* t = material->GetTexture((size_t)slot))
                {
                    auto* srv = t->GetTexture();
                    if (srv) return srv;
                }
                if (slot == 0)
                {
                    if (auto* t = material->GetMainTexture())
                    {
                        auto* srv = t->GetTexture();
                        if (srv) return srv;
                    }
                }
                // Затем — вшитая текстура меша
                if (embedded) return embedded;
                return nullptr;
            };

        static ID3D11BlendState* s_glassBlendState = nullptr;

        auto enableGlassBlend = [&]()
            {
                if (!s_glassBlendState)
                {
                    ID3D11Device* device = nullptr;
                    context->GetDevice(&device);
                    if (device)
                    {
                        D3D11_BLEND_DESC bd = {};
                        bd.RenderTarget[0].BlendEnable = TRUE;
                        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
                        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
                        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                        device->CreateBlendState(&bd, &s_glassBlendState);
                        device->Release();
                    }
                }
                if (s_glassBlendState)
                {
                    float bf[4] = {};
                    context->OMSetBlendState(s_glassBlendState, bf, 0xFFFFFFFF);
                }
            };

        const auto& parts = model->GetMeshParts();
        const bool hasEmbedded = !parts.empty() && (
            parts.size() > 1 ||
            parts[0].texture != nullptr ||
            parts[0].normalMap != nullptr ||
            parts[0].metalness != nullptr ||
            parts[0].roughness != nullptr);

        if (hasEmbedded)
        {
            UINT stride = sizeof(AssimpModelClass::VertexType);
            UINT offset = 0;

            for (const auto& part : parts)
            {
                if (!part.vertexBuffer || !part.indexBuffer) continue;

                context->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
                context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                const bool needsBlend = matIsGlass || part.isGlass;
                if (needsBlend) enableGlassBlend();

                const float partOpacity = matIsGlass ? matGlassAlpha
                    : (part.isTransparent ? part.opacity : 1.0f);

                // Если материал помечен как стекло (вручную или из GLB) —
                // игнорируем вшитые metalness/roughness (Metallic=1 из Blender glass trick)
                const bool isGlassPart = matIsGlass || part.isGlass;

                ID3D11ShaderResourceView* albedoSRV = getTexSRV(0, part.texture);
                ID3D11ShaderResourceView* normalSRV = isGlassPart ? nullptr : getTexSRV(1, part.normalMap);
                ID3D11ShaderResourceView* metalSRV = isGlassPart ? nullptr : getTexSRV(2, part.metalness);
                ID3D11ShaderResourceView* roughSRV = isGlassPart ? nullptr : getTexSRV(3, part.roughness);

                s->Render(context, part.indexCount,
                    world, view, projection,
                    albedoSRV, normalSRV, metalSRV, roughSRV,
                    lightColors, lightPositions,
                    cameraPos,
                    selectedFactor,
                    isGlassPart ? partOpacity : 1.0f);

                if (needsBlend)
                    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

                ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
                context->PSSetShaderResources(0, 4, nullSRVs);
            }
        }
        else
        {
            if (matIsGlass) enableGlassBlend();

            ID3D11ShaderResourceView* albedoSRV = getTexSRV(0, nullptr);
            ID3D11ShaderResourceView* normalSRV = getTexSRV(1, nullptr);
            // Если стекло — не передаём metalness/roughness (шейдер использует fallback = 0/1)
            ID3D11ShaderResourceView* metalSRV = matIsGlass ? nullptr : getTexSRV(2, nullptr);
            ID3D11ShaderResourceView* roughSRV = matIsGlass ? nullptr : getTexSRV(3, nullptr);

            s->Render(context, model->GetIndexCount(),
                world, view, projection,
                albedoSRV, normalSRV, metalSRV, roughSRV,
                lightColors, lightPositions,
                cameraPos,
                selectedFactor,
                matIsGlass ? matGlassAlpha : 1.0f);

            if (matIsGlass)
                context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

            ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
            context->PSSetShaderResources(0, 4, nullSRVs);
        }

        break;
    }
    case ShaderType::Projection:
    {
        auto* s = static_cast<projectionShaderClass*>(shader);
        ID3D11ShaderResourceView* projectionTex = nullptr;
        if (auto* tex = material->GetTexture(2))
            projectionTex = tex->GetTexture();

        ID3D11ShaderResourceView* colorTex = nullptr;
        if (auto* tex = material->GetTexture(0))
            colorTex = tex->GetTexture();

        XMFLOAT4 ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
        XMFLOAT4 diffuse = lightColors[0];
        XMFLOAT3 lightDir = { 0.0f, -1.0f, 0.0f };

        XMMATRIX view2 = material->GetProjectionView2();
        XMMATRIX proj2 = material->GetProjectionProj2();

        s->Render(context, model->GetIndexCount(),
            world, view, projection,
            view2, proj2,
            colorTex, projectionTex,
            ambient, diffuse, lightDir);

        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, nullSRVs);
        break;
    }
    case ShaderType::Texture:
    {
        auto* s = static_cast<TextureShaderClass*>(shader);
        ID3D11ShaderResourceView* fallbackSRV = nullptr;

        // Приоритет: материал → модель
        if (auto* t = material->GetTexture(0))
            fallbackSRV = t->GetTexture();
        if (!fallbackSRV)
            if (auto* t = material->GetMainTexture())
                fallbackSRV = t->GetTexture();
        if (!fallbackSRV)
            fallbackSRV = model->GetTexture();

        if (!fallbackSRV)
        {
            OutputDebugStringA("TextureShader: No valid SRV found! Skipping render.\n");
            break;
        }

        const auto& parts = model->GetMeshParts();
        bool anyEmbedded = false;
        for (const auto& part : parts)
        {
            if (part.texture) { anyEmbedded = true; break; }
        }

        if (anyEmbedded)
        {
            UINT stride = sizeof(AssimpModelClass::VertexType);
            UINT offset = 0;
            for (const auto& part : parts)
            {
                if (!part.vertexBuffer || !part.indexBuffer) continue;

                context->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
                context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                s->Render(context, part.indexCount, world, view, projection, fallbackSRV);

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
            }
        }
        else
        {
            s->Render(context, model->GetIndexCount(),
                world, view, projection, fallbackSRV);

            ID3D11ShaderResourceView* nullSRV = nullptr;
            context->PSSetShaderResources(0, 1, &nullSRV);
        }
        break;
    }
    case ShaderType::LightMap:
    {
        auto* s = static_cast<LightMapShaderClass*>(shader);
        ID3D11ShaderResourceView* srv0 = material->GetTexture(0) ? material->GetTexture(0)->GetTexture() : nullptr;
        ID3D11ShaderResourceView* srv1 = material->GetTexture(1) ? material->GetTexture(1)->GetTexture() : nullptr;
        s->Render(context, model->GetIndexCount(),
            world, view, projection, srv0, srv1);
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, nullSRVs);
        break;
    }
    case ShaderType::MultiTexture:
    {
        auto* s = static_cast<MultiTextureShaderClass*>(shader);
        ID3D11ShaderResourceView* srv0 = material->GetTexture(0) ? material->GetTexture(0)->GetTexture() : nullptr;
        ID3D11ShaderResourceView* srv1 = material->GetTexture(1) ? material->GetTexture(1)->GetTexture() : nullptr;
        s->Render(context, model->GetIndexCount(),
            world, view, projection, srv0, srv1);
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        context->PSSetShaderResources(0, 2, nullSRVs);
        break;
    }
    case ShaderType::NormalMap:
    {
        auto* s = static_cast<NormalMapShaderClass*>(shader);
        ID3D11ShaderResourceView* fallbackBaseSRV =
            material->GetTexture(0) ? material->GetTexture(0)->GetTexture() : nullptr;
        ID3D11ShaderResourceView* fallbackNormalSRV =
            material->GetTexture(1) ? material->GetTexture(1)->GetTexture() : nullptr;

        const auto& parts = model->GetMeshParts();
        bool anyEmbedded = false;
        for (const auto& part : parts)
        {
            if (part.texture || part.normalMap) { anyEmbedded = true; break; }
        }

        if (anyEmbedded)
        {
            UINT stride = sizeof(AssimpModelClass::VertexType);
            UINT offset = 0;
            for (const auto& part : parts)
            {
                if (!part.vertexBuffer || !part.indexBuffer) continue;

                context->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
                context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                // Приоритет материала над вшитыми текстурами
                ID3D11ShaderResourceView* srv0 = fallbackBaseSRV ? fallbackBaseSRV : part.texture;
                ID3D11ShaderResourceView* srv1 = fallbackNormalSRV ? fallbackNormalSRV : part.normalMap;

                s->Render(context, part.indexCount,
                    world, view, projection, srv0, srv1,
                    XMFLOAT3(0.707f, 0.707f, 0.0f),
                    XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

                ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
                context->PSSetShaderResources(0, 2, nullSRVs);
            }
        }
        else
        {
            s->Render(context, model->GetIndexCount(),
                world, view, projection, fallbackBaseSRV, fallbackNormalSRV,
                XMFLOAT3(0.707f, 0.707f, 0.0f),
                XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

            ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
            context->PSSetShaderResources(0, 2, nullSRVs);
        }
        break;
    }
    case ShaderType::Fog:
    {
        auto* s = static_cast<FogShaderClass*>(shader);
        ID3D11ShaderResourceView* fallbackSRV =
            material->GetMainTexture() ? material->GetMainTexture()->GetTexture() : nullptr;
        const float fogStart = fog.enabled ? fog.FOG_START : 10000.0f;
        const float fogEnd = fog.enabled ? fog.FOG_END : 10001.0f;

        const auto& parts = model->GetMeshParts();
        bool anyEmbedded = false;
        for (const auto& part : parts)
        {
            if (part.texture) { anyEmbedded = true; break; }
        }

        if (anyEmbedded)
        {
            UINT stride = sizeof(AssimpModelClass::VertexType);
            UINT offset = 0;
            for (const auto& part : parts)
            {
                if (!part.vertexBuffer || !part.indexBuffer) continue;

                context->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
                context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                ID3D11ShaderResourceView* srv = fallbackSRV ? fallbackSRV : part.texture;
                s->Render(context, part.indexCount,
                    world, view, projection, srv, fogStart, fogEnd);

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
            }
        }
        else
        {
            s->Render(context, model->GetIndexCount(),
                world, view, projection, fallbackSRV, fogStart, fogEnd);
            ID3D11ShaderResourceView* nullSRV = nullptr;
            context->PSSetShaderResources(0, 1, &nullSRV);
        }
        break;
    }
    case ShaderType::ClipPlane:
    {
        auto* s = static_cast<ClipPlaneShaderClass*>(shader);
        ID3D11ShaderResourceView* fallbackSRV =
            material->GetMainTexture() ? material->GetMainTexture()->GetTexture() : nullptr;

        const auto& parts = model->GetMeshParts();
        bool anyEmbedded = false;
        for (const auto& part : parts)
        {
            if (part.texture) { anyEmbedded = true; break; }
        }

        if (anyEmbedded)
        {
            UINT stride = sizeof(AssimpModelClass::VertexType);
            UINT offset = 0;
            for (const auto& part : parts)
            {
                if (!part.vertexBuffer || !part.indexBuffer) continue;

                context->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
                context->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                ID3D11ShaderResourceView* srv = fallbackSRV ? fallbackSRV : part.texture;
                s->Render(context, part.indexCount,
                    world, view, projection, srv, material->GetClipPlane());

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
            }
        }
        else
        {
            s->Render(context, model->GetIndexCount(),
                world, view, projection, fallbackSRV, material->GetClipPlane());
            ID3D11ShaderResourceView* nullSRV = nullptr;
            context->PSSetShaderResources(0, 1, &nullSRV);
        }
        break;
    }
    case ShaderType::Color:
    {
        auto* s = static_cast<ColorShaderClass*>(shader);
        s->Render(context, model->GetIndexCount(),
            world, view, projection);
        break;
    }
    case ShaderType::Water:
    {
        auto* s = static_cast<WaterShaderClass*>(shader);
        ID3D11ShaderResourceView* reflectionSRV = material->GetReflectionSRV();
        ID3D11ShaderResourceView* refractionSRV = material->GetRefractionSRV();
        ID3D11ShaderResourceView* normalMapSRV = material->GetTexture(0)
            ? material->GetTexture(0)->GetTexture() : nullptr;
        s->Render(context, model->GetIndexCount(),
            world, view, projection,
            material->GetReflectionMatrix(),
            reflectionSRV, refractionSRV, normalMapSRV,
            material->GetWaterTranslation(),
            material->GetReflectRefractScale());
        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(0, 3, nullSRVs);
        break;
    }
    case ShaderType::Glass:
    {
        auto* s = static_cast<GlassShaderClass*>(shader);
        ID3D11ShaderResourceView* colorSRV = material->GetTexture(0) ? material->GetTexture(0)->GetTexture() : nullptr;
        ID3D11ShaderResourceView* normalSRV = material->GetTexture(1) ? material->GetTexture(1)->GetTexture() : nullptr;
        ID3D11ShaderResourceView* refractionSRV = material->GetRefractionSRV();

        if (!colorSRV)      OutputDebugStringA("Glass: colorSRV is NULL\n");
        if (!normalSRV)     OutputDebugStringA("Glass: normalSRV is NULL\n");
        if (!refractionSRV) OutputDebugStringA("Glass: refractionSRV is NULL\n");

        bool ok = s->Render(context, model->GetIndexCount(),
            world, view, projection,
            colorSRV, normalSRV, refractionSRV,
            material->GetReflectRefractScale());
        if (!ok) OutputDebugStringA("Glass: Render() returned false!\n");

        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(0, 3, nullSRVs);
        break;
    }
    default:
        break;
    }
}
