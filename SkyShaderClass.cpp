
// (вставлен полностью модифицированный файл с поправками в InitializeShader и Render)
#include "SkyShaderClass.h"
#include <fstream>
using namespace std;

// В InitializeShader: загрузи и создай шейдеры из "SkyShader.vs/cso" и "SkyShader.ps/cso"
// InputLayout: D3D11_INPUT_DESC с одним элементом { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }


bool SkyShaderClass::InitializeGeometry(ID3D11Device* device)
{
    // 🔥 Вершины куба (размер 1.0, центр в (0,0,0))
    struct Vertex { XMFLOAT3 pos; };

    Vertex vertices[] = {
        // Front (+Z)
        { XMFLOAT3(-1, -1,  1) }, { XMFLOAT3(1, -1,  1) },
        { XMFLOAT3(1,  1,  1) }, { XMFLOAT3(-1,  1,  1) },
        // Back (-Z)
        { XMFLOAT3(1, -1, -1) }, { XMFLOAT3(-1, -1, -1) },
        { XMFLOAT3(-1,  1, -1) }, { XMFLOAT3(1,  1, -1) },
        // Top (+Y)
        { XMFLOAT3(-1,  1, -1) }, { XMFLOAT3(-1,  1,  1) },
        { XMFLOAT3(1,  1,  1) }, { XMFLOAT3(1,  1, -1) },
        // Bottom (-Y)
        { XMFLOAT3(-1, -1,  1) }, { XMFLOAT3(-1, -1, -1) },
        { XMFLOAT3(1, -1, -1) }, { XMFLOAT3(1, -1,  1) },
        // Right (+X)
        { XMFLOAT3(1, -1, -1) }, { XMFLOAT3(1, -1,  1) },
        { XMFLOAT3(1,  1,  1) }, { XMFLOAT3(1,  1, -1) },
        // Left (-X)
        { XMFLOAT3(-1, -1,  1) }, { XMFLOAT3(-1, -1, -1) },
        { XMFLOAT3(-1,  1, -1) }, { XMFLOAT3(-1,  1,  1) },
    };

    // 🔥 Индексы (36 для 6 граней × 2 треугольника)
    UINT indices[] = {
        // Front
        0, 1, 2,  0, 2, 3,
        // Back
        4, 5, 6,  4, 6, 7,
        // Top
        8, 9, 10,  8, 10, 11,
        // Bottom
        12, 13, 14,  12, 14, 15,
        // Right
        16, 17, 18,  16, 18, 19,
        // Left
        20, 21, 22,  20, 22, 23,
    };

    // Vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = sizeof(Vertex) * 24;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = { vertices, 0, 0 };
    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer)))
        return false;

    // Index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.ByteWidth = sizeof(UINT) * 36;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = { indices, 0, 0 };
    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer)))
    {
        m_vertexBuffer->Release();
        return false;
    }

    m_indexCount = 36;
    return true;
}

void SkyShaderClass::RenderGeometry(ID3D11DeviceContext* context)
{
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->DrawIndexed(m_indexCount, 0, 0);
}


bool SkyShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd, const WCHAR* vsFile, const WCHAR* psFile)
{
    ID3D10Blob* vsBlob = nullptr;
    ID3D10Blob* psBlob = nullptr;

    // ✅ Правильные entry points
    if (!CompileShader(vsFile, "SkyVS", "vs_5_0", &vsBlob)) return false;
    if (!CompileShader(psFile, "SkyPS", "ps_5_0", &psBlob)) {
        if (vsBlob) vsBlob->Release();
        return false;
    }



    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) { if (vsBlob) vsBlob->Release(); if (psBlob) psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) { if (vsBlob) vsBlob->Release(); if (psBlob) psBlob->Release(); return false; }

    // ✅ Попытка создать InputLayout для {POSITION}
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layoutDesc, 1,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &m_layout);
    if (FAILED(hr)) {
        // На всякий случай — не фатал, но сохраняем nullptr
        m_layout = nullptr;
        OutputDebugStringA("[SkyShader] Warning: failed to create input layout for sky\n");
    }

    // ✅ Правильный размер буфера
    if (!CreateConstantBuffer(device, sizeof(SkyConstantBuffer), &m_matrixBuffer)) {
        if (vsBlob) vsBlob->Release(); if (psBlob) psBlob->Release();
        return false;
    }

    if (vsBlob) vsBlob->Release();
    if (psBlob) psBlob->Release();
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_ANISOTROPIC; // 🔥 Анизотропная (макс. качество)
    sampDesc.MaxAnisotropy = 16;                // 🔥 Степень фильтрации
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    sampDesc.MipLODBias = 0.0f;                 // 🔥 Не смещаем LOD

    device->CreateSamplerState(&sampDesc, &m_skySampler);

    return true;
}
bool SkyShaderClass::Initialize(ID3D11Device* device, HWND hwnd)
{
    // Сначала создаём шейдеры
    if (!InitializeShader(device, hwnd,
        L"src/Render/Shader/Vs/SkyShader.vs",
        L"src/Render/Shader/Ps/SkyShader.ps"))
        return false;

    // 🔥 Затем создаём геометрию куба
    if (!InitializeGeometry(device))
        return false;

    return true;
}
bool SkyShaderClass::SetShaderParameters(ID3D11DeviceContext* ctx,
    const XMMATRIX& camRot, const XMMATRIX& invProj,
    const XMFLOAT3& sunDir, float sunIntensity)
{
    XMMATRIX camRotT = XMMatrixTranspose(camRot);
    XMMATRIX invProjT = XMMatrixTranspose(invProj);

    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    // 🔥 Упрощённая структура (только нужное)
    struct SkyCB {
        XMMATRIX camRot;
        XMMATRIX invProjection;
        XMFLOAT3 sunDirection;   // Опционально
        float sunIntensity;      // Опционально
        // Итого: 64+64+12+4 = 144 байта → выравнивание до 160
    };

    SkyCB* data = reinterpret_cast<SkyCB*>(mapped.pData);
    data->camRot = camRotT;
    data->invProjection = invProjT;
    data->sunDirection = sunDir;
    data->sunIntensity = sunIntensity;

    ctx->Unmap(m_matrixBuffer, 0);
    ctx->VSSetConstantBuffers(0, 1, &m_matrixBuffer);
    ctx->PSSetConstantBuffers(0, 1, &m_matrixBuffer);
    return true;
}
bool SkyShaderClass::Render(ID3D11DeviceContext* ctx, const XMMATRIX& mvpMatrix)
{
    // Привязываем input layout (если создан)
    ctx->IASetInputLayout(m_layout);
    ctx->VSSetShader(m_vertexShader, nullptr, 0);
    ctx->PSSetShader(m_pixelShader, nullptr, 0);

    // 🔥 1. Создаём массивы для очистки (размер = количеству слотов)
    ID3D11Buffer* nullBuffers[5] = {};           // Инициализация нулями
    ID3D11ShaderResourceView* nullSRVs[2] = {};  // Для слотов 0 и 7
    ID3D11SamplerState* nullSamplers[2] = {};    // Для слотов 0 и 7

    // 🔥 2. Очищаем "грязные" слоты от предыдущего шейдера
    ctx->VSSetConstantBuffers(1, 5, nullBuffers);   // Слоты 1-5 для VS
    ctx->PSSetConstantBuffers(1, 5, nullBuffers);   // Слоты 1-5 для PS

    // Очищаем слот тени (t7/s7)
    ctx->PSSetShaderResources(7, 1, &nullSRVs[1]);  // Или &nullSRVs[0] — неважно, т.к. размер 1
    ctx->PSSetSamplers(7, 1, &nullSamplers[1]);

    // 🔥 3. Обновляем и привязываем МАТРИЦУ (Слот 0)
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        *(XMMATRIX*)mapped.pData = XMMatrixTranspose(mvpMatrix);
        ctx->Unmap(m_matrixBuffer, 0);

        ctx->VSSetConstantBuffers(0, 1, &m_matrixBuffer);
        ctx->PSSetConstantBuffers(0, 1, &m_matrixBuffer);
    }

    // 🔥 4. Привязываем ресурсы НЕБА (Слот 0)
    if (m_skySRV)
    {
        ctx->PSSetShaderResources(0, 1, &m_skySRV);
        ctx->PSSetSamplers(0, 1, &m_skySampler);
    }

    // 🔥 5. Рендер геометрии
    RenderGeometry(ctx);

    // 🔥 6. Очистка слота 0 после отрисовки
    ctx->PSSetShaderResources(0, 1, &nullSRVs[0]);

    return true;
}

void SkyShaderClass::RenderShader(ID3D11DeviceContext* ctx, int indexCount)
{
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_vertexShader, nullptr, 0);
    ctx->PSSetShader(m_pixelShader, nullptr, 0);

    // 🔥 Привязка кубмапа в слот 0
    if (m_skySRV)
    {
        ctx->PSSetShaderResources(0, 1, &m_skySRV);
        ctx->PSSetSamplers(0, 1, &m_skySampler);
    }

    ctx->Draw(indexCount, 0);

    // Очистка слотов (хорошая практика)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
}
void SkyShaderClass::Shutdown() { ShutdownShader(); }

void SkyShaderClass::ShutdownShader()
{
    // 🔥 Освобождаем ВСЁ, что создали:
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    if (m_indexBuffer) { m_indexBuffer->Release(); m_indexBuffer = nullptr; }

    if (m_skySampler) { m_skySampler->Release(); m_skySampler = nullptr; }
    if (m_skySRV) { m_skySRV = nullptr; }  // Владеет ApplicationClass
    if (m_matrixBuffer) { m_matrixBuffer->Release(); m_matrixBuffer = nullptr; }
    if (m_layout) { m_layout->Release(); m_layout = nullptr; }
    if (m_pixelShader) { m_pixelShader->Release(); m_pixelShader = nullptr; }
    if (m_vertexShader) { m_vertexShader->Release(); m_vertexShader = nullptr; }
}