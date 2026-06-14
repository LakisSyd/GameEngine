#include "D3DClass.h"
#include <string>
#include "helper.h"
//==============CHECK 


D3DClass::D3DClass()
{
	m_swapChain = nullptr; 
	m_device = nullptr;
	m_deviceContext = nullptr;
	m_renderTargetView = nullptr;
	m_depthStencilBuffer = nullptr;
	m_depthStencilState = nullptr;
	m_depthStencilView = nullptr;
	m_rasterState = nullptr;
    m_depthDisabledStencilState = nullptr;  // ❗ Важно!

    
}

D3DClass::D3DClass(const D3DClass&)
{
}

D3DClass::~D3DClass()
{
}

// 📌 Инициализация графической подсистемы DirectX 11
// Параметры:
//   screenWidth, screenHeight — размеры окна (или экрана в полноэкранном режиме)
//   vsync — включена ли вертикальная синхронизация
//   hwnd — дескриптор окна Windows, в которое будет рендериться изображение
//   fullscreen — полноэкранный режим или оконный
//   screenDepth — дальняя плоскость отсечения (для камеры)
//   screenNear — ближняя плоскость отсечения
bool D3DClass::Initialize(
    int screenWidth,
    int screenHeight,
    bool vsync,
    HWND hwnd,
    bool fullscreen,
    float screenDepth,
    float screenNear)
{
    // ========================================================================
    // 📦 ПЕРЕМЕННЫЕ И ПОДГОТОВКА
    // ========================================================================
    HRESULT result;
    IDXGIFactory* factory = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIOutput* adapterOutput = nullptr;
    unsigned int numModes = 0, i = 0, numerator = 60, denominator = 1;
    unsigned long long stringLength;
    DXGI_MODE_DESC* displayModeList = nullptr;
    DXGI_ADAPTER_DESC adapterDesc;
    int error;
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    D3D_FEATURE_LEVEL featureLevel;
    ID3D11Texture2D* backBufferPtr = nullptr;
    D3D11_TEXTURE2D_DESC depthBufferDesc;
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    D3D11_RASTERIZER_DESC rasterDesc;
    float fieldOfView, screenAspect;

    m_vsync_enabled = vsync;

    // ========================================================================
    // 🔍 ПОЛУЧЕНИЕ ИНФОРМАЦИИ О ВИДЕОКАРТЕ И МОНИТОРЕ
    // ========================================================================

    // Создаём фабрику DXGI
    DX_CHECK(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory));

    // Получаем основной адаптер (видеокарту)
    DX_CHECK(factory->EnumAdapters(0, &adapter));

    // Получаем выход адаптера (монитор)
    DX_CHECK(adapter->EnumOutputs(0, &adapterOutput));

    // Получаем количество поддерживаемых режимов отображения
    DX_CHECK(adapterOutput->GetDisplayModeList(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_ENUM_MODES_INTERLACED,
        &numModes,
        NULL));

    // Создаём массив для хранения режимов
    displayModeList = new DXGI_MODE_DESC[numModes];
    if (!displayModeList) return false;

    // Заполняем массив режимов
    DX_CHECK(adapterOutput->GetDisplayModeList(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_ENUM_MODES_INTERLACED,
        &numModes,
        displayModeList));

    // Ищем режим, совпадающий с разрешением экрана
    for (i = 0; i < numModes; i++)
    {
        if (displayModeList[i].Width == (unsigned int)screenWidth &&
            displayModeList[i].Height == (unsigned int)screenHeight)
        {
            numerator = displayModeList[i].RefreshRate.Numerator;
            denominator = displayModeList[i].RefreshRate.Denominator;
        }
    }

    // Получаем описание видеокарты
    DX_CHECK(adapter->GetDesc(&adapterDesc));
    m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

    // Конвертируем имя видеокарты
    error = wcstombs_s(&stringLength, m_videoCardDescription, 128,
        adapterDesc.Description, 128);
    if (error != 0) return false;

    // Освобождаем временные ресурсы
    delete[] displayModeList;
    displayModeList = nullptr;

    adapterOutput->Release();
    adapterOutput = nullptr;

    adapter->Release();
    adapter = nullptr;

    factory->Release();
    factory = nullptr;

    // ========================================================================
    // 🔗 НАСТРОЙКА SWAP CHAIN
    // ========================================================================
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = screenWidth;
    swapChainDesc.BufferDesc.Height = screenHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Настройка частоты обновления
    if (m_vsync_enabled)
    {
        swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
    }
    else
    {
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    }

    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = !fullscreen;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = 0;

    featureLevel = D3D_FEATURE_LEVEL_11_0;

    // ========================================================================
    // ⭐ СОЗДАНИЕ УСТРОЙСТВА И SWAP CHAIN (ТОЛЬКО ПОСЛЕ НАСТРОЙКИ!)
    // ========================================================================

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DX_CHECK(D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        createDeviceFlags,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,        // ⭐ Теперь структура заполнена!
        &m_swapChain,
        &m_device,
        NULL,
        &m_deviceContext));

    // ========================================================================
    // 🎨 СОЗДАНИЕ BACK BUFFER
    // ========================================================================

    DX_CHECK(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
        (LPVOID*)&backBufferPtr));
    DX_CHECK(m_device->CreateRenderTargetView(backBufferPtr, NULL,
        &m_renderTargetView));
    backBufferPtr->Release();
    backBufferPtr = nullptr;

    // ========================================================================
    // 🧱 СОЗДАНИЕ DEPTH BUFFER
    // ========================================================================

    ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));
    depthBufferDesc.Width = screenWidth;
    depthBufferDesc.Height = screenHeight;
    depthBufferDesc.MipLevels = 1;
    depthBufferDesc.ArraySize = 1;
    depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthBufferDesc.SampleDesc.Count = 1;
    depthBufferDesc.SampleDesc.Quality = 0;
    depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthBufferDesc.CPUAccessFlags = 0;
    depthBufferDesc.MiscFlags = 0;

    DX_CHECK(m_device->CreateTexture2D(&depthBufferDesc, NULL,
        &m_depthStencilBuffer));

    // ========================================================================
    // 🎭 DEPTH STENCIL STATE (ДЛЯ 3D)
    // ========================================================================

    ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = true;
    depthStencilDesc.StencilReadMask = 0xFF;
    depthStencilDesc.StencilWriteMask = 0xFF;
    depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    DX_CHECK(m_device->CreateDepthStencilState(&depthStencilDesc,
        &m_depthStencilState));

    // ========================================================================
    // 🖼️ DEPTH STENCIL STATE (ДЛЯ 2D)
    // ========================================================================

    D3D11_DEPTH_STENCIL_DESC depthDisabledStencilDesc;
    ZeroMemory(&depthDisabledStencilDesc, sizeof(depthDisabledStencilDesc));
    depthDisabledStencilDesc.DepthEnable = false;
    depthDisabledStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDisabledStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthDisabledStencilDesc.StencilEnable = true;
    depthDisabledStencilDesc.StencilReadMask = 0xFF;
    depthDisabledStencilDesc.StencilWriteMask = 0xFF;
    depthDisabledStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthDisabledStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    depthDisabledStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthDisabledStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthDisabledStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthDisabledStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthDisabledStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthDisabledStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    result = m_device->CreateDepthStencilState(&depthDisabledStencilDesc,
        &m_depthDisabledStencilState);
    if (FAILED(result)) return false;

    // Активируем 3D-состояние по умолчанию
    m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 1);

    // ========================================================================
    // 👁️ DEPTH STENCIL VIEW
    // ========================================================================

    ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
    depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Texture2D.MipSlice = 0;

    DX_CHECK(m_device->CreateDepthStencilView(m_depthStencilBuffer,
        &depthStencilViewDesc,
        &m_depthStencilView));

    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

    // ========================================================================
    // 🎨 RASTERIZER STATE
    // ========================================================================

    rasterDesc.AntialiasedLineEnable = false;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.DepthClipEnable = true;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.FrontCounterClockwise = false;
    rasterDesc.MultisampleEnable = false;
    rasterDesc.ScissorEnable = false;
    rasterDesc.SlopeScaledDepthBias = 0.0f;

    DX_CHECK(m_device->CreateRasterizerState(&rasterDesc, &m_rasterState));
    m_deviceContext->RSSetState(m_rasterState);

    // ========================================================================
    // 🌈 BLEND STATE (для альфа-смешивания)
    // ========================================================================

    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    DX_CHECK(m_device->CreateBlendState(&blendDesc, &m_alphaBlendState));

    // ========================================================================
    // 🖥️ VIEWPORT
    // ========================================================================

    m_viewport.Width = (float)screenWidth;
    m_viewport.Height = (float)screenHeight;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;

    m_deviceContext->RSSetViewports(1, &m_viewport);

    // ========================================================================
    // 📐 МАТРИЦЫ
    // ========================================================================

    fieldOfView = 3.141592654f / 4.0f;
    screenAspect = (float)screenWidth / (float)screenHeight;
    fieldOfView = XMConvertToRadians(75.0f);


    m_projectionMatrix = XMMatrixPerspectiveFovLH(fieldOfView, screenAspect,
        screenNear, screenDepth);
    m_worldMatrix = XMMatrixIdentity();
    m_orthoMatrix = XMMatrixOrthographicOffCenterLH(
        0.0f, (float)screenWidth,
        (float)screenHeight, 0.0f,
        screenNear, screenDepth);

    return true;
}



void D3DClass::Shutdown()
{
    // Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.

    if (m_deviceContext)
    {
        m_deviceContext->ClearState();
        m_deviceContext->Flush();
    }

    if (m_renderTargetView) { m_renderTargetView->Release(); m_renderTargetView = nullptr; }
    if (m_depthStencilView) { m_depthStencilView->Release(); m_depthStencilView = nullptr; }

    if (m_rasterState)
    {
        m_rasterState->Release();
        m_rasterState = 0;
    }


    if (m_depthStencilState)
    {
        m_depthStencilState->Release();
        m_depthStencilState = 0;
        
    }

    if (m_depthDisabledStencilState) {
        m_depthDisabledStencilState->Release();
        m_depthDisabledStencilState = nullptr;
    }

    if (m_depthStencilBuffer)
    {
        m_depthStencilBuffer->Release();
        m_depthStencilBuffer = 0;
    }

   

    if (m_alphaBlendState)
    {
        m_alphaBlendState->Release();
        m_alphaBlendState = nullptr;
    }

    if (m_swapChain)
    {
        m_swapChain->SetFullscreenState(FALSE, nullptr);
        m_swapChain->Release();
        m_swapChain = nullptr;
    }

    if (m_deviceContext)
    {
        m_deviceContext->Release();
        m_deviceContext = nullptr;
    }

    if (m_device)
    {
#ifdef _DEBUG
        ID3D11Debug* debug = nullptr;
        if (SUCCEEDED(m_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&debug)))
        {
            debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
            debug->Release();
        }
#endif
        m_device->Release();
        m_device = nullptr;
    }

    return;
}



// В классе D3DClass есть несколько вспомогательных функций. 
// Первые две — BeginScene и EndScene. 
// BeginScene вызывается каждый раз, когда мы собираемся отрисовать новую 3D-сцену 
// в начале каждого кадра. Всё, что она делает — очищает буферы, 
// чтобы они были пустыми и готовыми к отрисовке. 
// Другая функция — EndScene — сообщает цепочке подкачки, 
// что нужно отобразить нашу 3D-сцену после завершения всей отрисовки в конце кадра.

void D3DClass::BeginScene(float red, float green, float blue, float alpha)
{
    float color[4];

    // Устанавливаем цвет, которым будем очищать буфер.
    color[0] = red;
    color[1] = green;
    color[2] = blue;
    color[3] = alpha;

    // Очищаем задний буфер.
    m_deviceContext->ClearRenderTargetView(m_renderTargetView, color);

    // Очищаем буфер глубины.
    m_deviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    return;
}

void D3DClass::EndScene()
{
    // Отображаем задний буфер на экране, так как рендеринг завершён.
    if (m_vsync_enabled)
    {
        // Синхронизация с частотой обновления экрана.
        m_swapChain->Present(1, 0);
    }
    else
    {
        // Отображать как можно быстрее.
        m_swapChain->Present(0, 0);
    }

    return;
}

// Следующие функции просто возвращают указатели на устройство Direct3D 
// и контекст устройства Direct3D. Эти вспомогательные функции часто будут 
// вызываться фреймворком.

ID3D11Device* D3DClass::GetDevice()
{
    return m_device;
}

ID3D11DeviceContext* D3DClass::GetDeviceContext()
{
    return m_deviceContext;
}

// Следующие три вспомогательные функции возвращают копии проекционной, 
// мировой и ортогональной матриц вызывающим функциям. 
// Большинству шейдеров нужны эти матрицы для рендеринга, 
// поэтому должен быть простой способ для внешних объектов получить их копию. 
// Мы не будем вызывать эти функции в этом уроке, 
// но я просто объясняю, почему они есть в коде.

void D3DClass::GetProjectionMatrix(XMMATRIX& projectionMatrix)
{
    projectionMatrix = m_projectionMatrix;
    return;
}

void D3DClass::GetWorldMatrix(XMMATRIX& worldMatrix)
{
    worldMatrix = m_worldMatrix;
    return;
}

void D3DClass::GetOrthoMatrix(XMMATRIX& orthoMatrix)
{
    orthoMatrix = m_orthoMatrix;
    return;
}

// Эта вспомогательная функция возвращает по ссылке название видеокарты 
// и объём видеопамяти. Знание названия видеокарты может помочь 
// при отладке на разных конфигурациях.

void D3DClass::GetVideoCardInfo(char* cardName, int& memory)
{
    strcpy_s(cardName, 128, m_videoCardDescription);
    memory = m_videoCardMemory;
    return;
}

// Последние две вспомогательные функции будут использоваться 
// в уроке по рендерингу в текстуру (render to texture).

void D3DClass::SetBackBufferRenderTarget()
{
    // Привязываем представление цели рендеринга и буфер глубины 
    // к выходному конвейеру рендеринга.
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

    return;
}

void D3DClass::ResetViewport()
{
    // Устанавливаем viewport.
    m_deviceContext->RSSetViewports(1, &m_viewport);

    return;
}


void D3DClass::TurnZBufferOn() {
    m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 1);
}

void D3DClass::TurnZBufferOff() {
    m_deviceContext->OMSetDepthStencilState(m_depthDisabledStencilState, 1);
}

void D3DClass::TurnAlphaBlendingOn()
{
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_deviceContext->OMSetBlendState(m_alphaBlendState, blendFactor, 0xffffffff);
}

void D3DClass::TurnAlphaBlendingOff()
{
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_deviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}
