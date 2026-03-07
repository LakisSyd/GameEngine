#include "BaseShader.h"

ShaderBaseClass::ShaderBaseClass()
    : m_vertexShader(nullptr), m_pixelShader(nullptr)
    , m_layout(nullptr), m_sampleState(nullptr)
    , m_fogBuffer(nullptr) {
}

ShaderBaseClass::~ShaderBaseClass() { Shutdown(); }

void ShaderBaseClass::Shutdown()
{
    if (m_fogBuffer)    { m_fogBuffer->Release();    m_fogBuffer    = nullptr; }
    if (m_sampleState)  { m_sampleState->Release();  m_sampleState  = nullptr; }
    if (m_layout)       { m_layout->Release();       m_layout       = nullptr; }
    if (m_pixelShader)  { m_pixelShader->Release();  m_pixelShader  = nullptr; }
    if (m_vertexShader) { m_vertexShader->Release(); m_vertexShader = nullptr; }
}

bool ShaderBaseClass::CompileShader(
    const std::wstring& filename,
    const char* entryPoint,
    const char* profile,
    ID3D10Blob** shaderBlob)
{
    DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3D10Blob* errorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile(
        filename.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,  // ✅ Включаем поддержку #include
        entryPoint, profile,
        flags, 0, shaderBlob, &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            // Выводим текст ошибки в Output окно VS
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            OutputShaderErrorMessage(errorBlob, nullptr, filename.c_str());
        }
        else
        {
            // Файл не найден
            OutputDebugStringW((L"Shader file not found: " + filename + L"\n").c_str());
        }
        return false;
    }
    if (errorBlob) errorBlob->Release();
    return true;
}

bool ShaderBaseClass::CreateInputLayout(
    ID3D11Device* device,
    const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
    int numElements,
    ID3D10Blob* vertexShaderBlob)
{
    HRESULT hr = device->CreateInputLayout(
        layoutDesc, numElements,
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        &m_layout);
    return SUCCEEDED(hr);
}



void ShaderBaseClass::OutputShaderErrorMessage(
    ID3D10Blob* errorMessage,
    HWND hwnd,
    const WCHAR* filename)
{
    // Твоя текущая реализация: запись в файл + MessageBox
    char* errors = static_cast<char*>(errorMessage->GetBufferPointer());
    FILE* f = nullptr;
    _wfopen_s(&f, L"shader-error.txt", L"w");
    if (f) {
        fprintf(f, "%s", errors);
        fclose(f);
    }
    if (hwnd)
        MessageBoxW(hwnd, L"Ошибка шейдера! См. shader-error.txt", filename, MB_OK);
}

void ShaderBaseClass::BindShader(ID3D11DeviceContext* context)
{
    context->IASetInputLayout(m_layout);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    if (m_sampleState)
        context->PSSetSamplers(0, 1, &m_sampleState);
}


bool ShaderBaseClass::CreateDefaultSampler(ID3D11Device* device)
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = device->CreateSamplerState(&desc, &m_sampleState);
    return SUCCEEDED(hr);
}

bool ShaderBaseClass::CreateConstantBuffer(ID3D11Device* device, UINT byteWidth, ID3D11Buffer** outBuffer)
{
    // ⚠️ ByteWidth должен быть кратен 16 для constant buffers!
    UINT alignedSize = (byteWidth + 15) & ~15;

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = alignedSize;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&desc, nullptr, outBuffer);
    return SUCCEEDED(hr);
}

template<typename T>
bool ShaderBaseClass::UpdateConstantBuffer(
    ID3D11DeviceContext* context,
    ID3D11Buffer* buffer,
    const T& data,
    UINT slot,
    bool vertexShader)
{
    if (!buffer) return false;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return false;

    // Копируем данные (с учётом выравнивания)
    memcpy(mapped.pData, &data, sizeof(T));
    context->Unmap(buffer, 0);

    // Привязываем к нужной стадии шейдера
    if (vertexShader)
        context->VSSetConstantBuffers(slot, 1, &buffer);
    else
        context->PSSetConstantBuffers(slot, 1, &buffer);

    return true;
}

// ✅ Создаёт FogBuffer (слот b2)
bool ShaderBaseClass::CreateFogBuffer(ID3D11Device* device)
{
    return CreateConstantBuffer(device, sizeof(FogBufferType), &m_fogBuffer);
}

// ✅ Обновляет FogBuffer и привязывает к VS+PS слот 2
void ShaderBaseClass::UpdateFog(ID3D11DeviceContext* context, const Settings::FogSettings& fog)
{
    if (!m_fogBuffer) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(m_fogBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    FogBufferType* ptr = (FogBufferType*)mapped.pData;
    ptr->fogColor   = XMFLOAT4(fog.color.x, fog.color.y, fog.color.z, 1.0f);
    ptr->fogStart   = fog.enabled ? fog.FOG_START : 100000.0f;
    ptr->fogEnd     = fog.enabled ? fog.FOG_END   : 100001.0f;
    ptr->fogEnabled = fog.enabled ? 1.0f : 0.0f;
    ptr->padding    = 0.0f;

    context->Unmap(m_fogBuffer, 0);

    // Слот b2 — резервируем для тумана во ВСЕХ шейдерах
    context->VSSetConstantBuffers(2, 1, &m_fogBuffer);
    context->PSSetConstantBuffers(2, 1, &m_fogBuffer);
}