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
#include "../../ProjectionShaderClass.h"
#include "../../PBRShaderClass.h"
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
    XMFLOAT3             cameraPos = { 0.0f, 0.0f, 0.0f })  // ← новый параметр
{
    if (!model || !material) return;

    auto* shader = material->GetShader();
    if (!shader) return;

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

        // --- НАДЕЖНЫЙ GET TEXTURE SRV ---
        auto getTexSRV = [&](ID3D11ShaderResourceView* embedded) -> ID3D11ShaderResourceView*
            {
             //   if (embedded) return embedded; //Вот если это срабатывало мой текстура каменя не ставилась цвет сферы был просто белый

                // 1. Слот 0 из материала (если назначено в редакторе)
                if (auto* t0 = material->GetTexture(0))
                {
                    auto* srv = t0->GetTexture();
                    if (srv) return srv;
                }

                // 2. MainTexture (например, m_defaultTexture)
                if (auto* main = material->GetMainTexture())
                {
                    auto* srv = main->GetTexture();
                    if (srv) return srv;
                }

                // 3. Не проверяем модель здесь, проверим ниже после вызова getTexSRV

                return nullptr;
            };

        const auto& parts = model->GetMeshParts();

        // Проверяем, есть ли ВСЁГО ОДНА встроенная текстура (не только part.texture, но и другие)
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
                if (!srv) // <<<--- ДОБАВЛЕНО: проверка fallback из модели
                {
                    srv = model->GetTexture();
                    if (srv)
                    {
                        OutputDebugStringA("LightShader (part): Using fallback texture from model.\n");
                    }
                }

                if (!srv)
                {
                    OutputDebugStringA("LightShader (part): SRV is NULL! Skipping render for this part.\n");
                    continue; // пропускаем, чтобы не вызвать падение
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
            if (!textureSRV) // <<<--- ДОБАВЛЕНО: проверка fallback из модели
            {
                textureSRV = model->GetTexture();
                if (textureSRV)
                {
                    OutputDebugStringA("LightShader: Using fallback texture from model.\n");
                }
            }

            if (!textureSRV)
            {
                OutputDebugStringA("LightShader: Final fallback SRV is NULL! Object will be invisible/white.\n");
                break; // не рендерим
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

        // Слоты: 0=albedo, 1=normal, 2=metalness, 3=roughness
        auto getTexSRV = [&](int slot, ID3D11ShaderResourceView* embedded) -> ID3D11ShaderResourceView*
            {
                if (embedded) return embedded;
                if (auto* t = material->GetTexture((size_t)slot)) return t->GetTexture();
                if (slot == 0)
                {
                    if (auto* t = material->GetMainTexture()) return t->GetTexture();
                }
                return nullptr;
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

                ID3D11ShaderResourceView* albedoSRV = getTexSRV(0, part.texture);
                ID3D11ShaderResourceView* normalSRV = getTexSRV(1, part.normalMap);
                ID3D11ShaderResourceView* metalSRV = getTexSRV(2, part.metalness);
                ID3D11ShaderResourceView* roughSRV = getTexSRV(3, part.roughness);

                s->Render(context, part.indexCount,
                    world, view, projection,
                    albedoSRV, normalSRV, metalSRV, roughSRV,
                    lightColors, lightPositions,
                    cameraPos,
                    selectedFactor);

                ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
                context->PSSetShaderResources(0, 4, nullSRVs);
            }
        }
        else
        {
            ID3D11ShaderResourceView* albedoSRV = getTexSRV(0, nullptr);
            ID3D11ShaderResourceView* normalSRV = getTexSRV(1, nullptr);
            ID3D11ShaderResourceView* metalSRV = getTexSRV(2, nullptr);
            ID3D11ShaderResourceView* roughSRV = getTexSRV(3, nullptr);

            s->Render(context, model->GetIndexCount(),
                world, view, projection,
                albedoSRV, normalSRV, metalSRV, roughSRV,
                lightColors, lightPositions,
                cameraPos,
                selectedFactor);

            ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
            context->PSSetShaderResources(0, 4, nullSRVs);
        }

        break;
    }
    case ShaderType::Texture:
    {
        auto* s = static_cast<TextureShaderClass*>(shader);
        ID3D11ShaderResourceView* fallbackSRV = nullptr;

        // 1. Попробуем взять текстуру из Material (MainTexture или slot 0)
        if (auto* t = material->GetMainTexture())
            fallbackSRV = t->GetTexture();
        if (!fallbackSRV && material->GetTexture(0))
            fallbackSRV = material->GetTexture(0)->GetTexture();

        // 2. Если всё ещё null — используем fallback из модели (m_Texture)
        if (!fallbackSRV && model->GetTexture())
            fallbackSRV = model->GetTexture();

        // 3. КРИТИЧЕСКИ ВАЖНО: если fallbackSRV == nullptr, не рендерим (или рисуем цветом)
        if (!fallbackSRV)
        {
            OutputDebugStringA("TextureShader: No valid SRV found! Skipping render.\n");
            // Можно вместо этого вызвать Render с цветом (например, grey), но лучше логировать.
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

                // Для TextureShader — всегда используем fallbackSRV, НЕ part.texture!
                // Потому что TextureShader не знает про PBR-карты.
                s->Render(context, part.indexCount, world, view, projection, fallbackSRV);

                ID3D11ShaderResourceView* nullSRV = nullptr;
                context->PSSetShaderResources(0, 1, &nullSRV);
            }
        }
        else
        {
            // Одиночный рендер — используем fallbackSRV
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

                ID3D11ShaderResourceView* srv0 = part.texture ? part.texture : fallbackBaseSRV;
                ID3D11ShaderResourceView* srv1 = part.normalMap ? part.normalMap : fallbackNormalSRV;

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

                ID3D11ShaderResourceView* srv = part.texture ? part.texture : fallbackSRV;
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

                ID3D11ShaderResourceView* srv = part.texture ? part.texture : fallbackSRV;
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