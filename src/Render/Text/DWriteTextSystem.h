#pragma once
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")
#include <dwrite.h>
#include <wincodec.h>
#include <stdio.h>
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
class DWriteTextSystem
{
public:

    ~DWriteTextSystem() {
        this->Shutdown();
    }

    bool Initialize()
    {
        HRESULT hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&m_factory)
        );

        if (FAILED(hr))
        {
            OutputDebugStringA("DWriteCreateFactory failed\n");
            return false;
        }

        return CreateDefaultFormat();
    }
    bool CreateDefaultFormat()
    {
        if (!m_factory)
            return false;

        HRESULT hr = m_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            24.0f,
            L"ru-ru",
            &m_defaultFormat
        );

        if (FAILED(hr))
        {
            OutputDebugStringA("CreateTextFormat failed\n");
            return false;
        }

        m_defaultFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_defaultFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        hr = m_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            17.0f,
            L"ru-ru",
            &m_smallFormat
        );

        if (FAILED(hr))
            return false;

        m_smallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_smallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_smallFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        return true;
    }
    bool MeasureText(
        const wchar_t* text,
        float maxWidth,
        float maxHeight,
        DWRITE_TEXT_METRICS& outMetrics)
    {
        if (!m_factory || !m_defaultFormat || !text)
            return false;

        IDWriteTextLayout* layout = nullptr;

        HRESULT hr = m_factory->CreateTextLayout(
            text,
            static_cast<UINT32>(wcslen(text)),
            m_defaultFormat,
            maxWidth,
            maxHeight,
            &layout
        );

        if (FAILED(hr))
            return false;

        hr = layout->GetMetrics(&outMetrics);

        if (FAILED(hr))
        {
            layout->Release();
            return false;
        }

        layout->Release();

        return SUCCEEDED(hr);
    }

    void Shutdown()
    {
        if (m_whiteBrush)
        {
            m_whiteBrush->Release();
            m_whiteBrush = nullptr;
        }

        if (m_renderTarget)
        {
            m_renderTarget->Release();
            m_renderTarget = nullptr;
        }

        if (m_d2dFactory)
        {
            m_d2dFactory->Release();
            m_d2dFactory = nullptr;
        }

        if (m_defaultFormat)
        {
            m_defaultFormat->Release();
            m_defaultFormat = nullptr;
        }

        if (m_smallFormat)
        {
            m_smallFormat->Release();
            m_smallFormat = nullptr;
        }

        if (m_factory)
        {
            m_factory->Release();
            m_factory = nullptr;
        }
    }

    bool InitializeRenderTarget(IDXGISwapChain* swapChain)
    {
        HRESULT hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            &m_d2dFactory
        );

        if (FAILED(hr))
            return false;

        IDXGISurface* dxgiBackBuffer = nullptr;

        hr = swapChain->GetBuffer(
            0,
            __uuidof(IDXGISurface),
            reinterpret_cast<void**>(&dxgiBackBuffer)
        );

        if (FAILED(hr))
            return false;

        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(
                DXGI_FORMAT_UNKNOWN,
                D2D1_ALPHA_MODE_IGNORE
            )
        );

        hr = m_d2dFactory->CreateDxgiSurfaceRenderTarget(
            dxgiBackBuffer,
            &props,
            &m_renderTarget
        );

        dxgiBackBuffer->Release();

        if (FAILED(hr))
            return false;

        hr = m_renderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::White),
            &m_whiteBrush
        );

        return SUCCEEDED(hr);
    }

    void DrawTextLine(const wchar_t* text, float x, float y)
    {
        DrawTextLineColor(text, x, y, D2D1::ColorF(D2D1::ColorF::White));
    }

    void DrawTextLineColor(
        const wchar_t* text,
        float x,
        float y,
        const D2D1_COLOR_F& color)
    {
        if (!m_renderTarget || !m_defaultFormat || !m_whiteBrush || !text)
            return;

        const bool ownsDrawSession = !m_isDrawing;
        if (ownsDrawSession)
            BeginBatch();

        D2D1_RECT_F rect = D2D1::RectF(
            x,
            y,
            x + 800.0f,
            y + 100.0f
        );

        m_whiteBrush->SetColor(color);
        m_renderTarget->DrawTextW(
            text,
            static_cast<UINT32>(wcslen(text)),
            m_defaultFormat,
            rect,
            m_whiteBrush
        );

        if (ownsDrawSession)
            EndBatch();
    }

    // Компактный текст для подписей внутри игровых панелей.
    void DrawSmallTextLineColor(
        const wchar_t* text,
        float x,
        float y,
        float maxWidth,
        const D2D1_COLOR_F& color)
    {
        if (!m_renderTarget || !m_smallFormat || !m_whiteBrush || !text || maxWidth <= 0.0f)
            return;

        const bool ownsDrawSession = !m_isDrawing;
        if (ownsDrawSession)
            BeginBatch();

        m_whiteBrush->SetColor(color);
        m_renderTarget->DrawTextW(
            text,
            static_cast<UINT32>(wcslen(text)),
            m_smallFormat,
            D2D1::RectF(x, y, x + maxWidth, y + 24.0f),
            m_whiteBrush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);

        if (ownsDrawSession)
            EndBatch();
    }

    void DrawFilledRect(
        float x,
        float y,
        float width,
        float height,
        const D2D1_COLOR_F& color)
    {
        if (!m_renderTarget || !m_whiteBrush || width <= 0.0f || height <= 0.0f)
            return;

        const bool ownsDrawSession = !m_isDrawing;
        if (ownsDrawSession)
            BeginBatch();

        m_whiteBrush->SetColor(color);
        m_renderTarget->FillRectangle(
            D2D1::RectF(x, y, x + width, y + height),
            m_whiteBrush);

        if (ownsDrawSession)
            EndBatch();
    }

    void DrawRectOutline(
        float x,
        float y,
        float width,
        float height,
        float thickness,
        const D2D1_COLOR_F& color)
    {
        if (!m_renderTarget || !m_whiteBrush || width <= 0.0f || height <= 0.0f)
            return;

        const bool ownsDrawSession = !m_isDrawing;
        if (ownsDrawSession)
            BeginBatch();

        m_whiteBrush->SetColor(color);
        m_renderTarget->DrawRectangle(
            D2D1::RectF(x, y, x + width, y + height),
            m_whiteBrush,
            thickness);

        if (ownsDrawSession)
            EndBatch();
    }

    bool LoadBitmapFromFile(const wchar_t* path, ID2D1Bitmap** outBitmap)
    {
        if (!m_renderTarget || !path || !outBitmap)
            return false;

        if (*outBitmap)
            return true;

        IWICImagingFactory* wicFactory = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;

        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wicFactory));
        if (SUCCEEDED(hr))
        {
            hr = wicFactory->CreateDecoderFromFilename(
                path,
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &decoder);
        }
        if (SUCCEEDED(hr))
            hr = decoder->GetFrame(0, &frame);
        if (SUCCEEDED(hr))
            hr = wicFactory->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr))
        {
            hr = converter->Initialize(
                frame,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0f,
                WICBitmapPaletteTypeCustom);
        }
        if (SUCCEEDED(hr))
            hr = m_renderTarget->CreateBitmapFromWicBitmap(converter, nullptr, outBitmap);

        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (wicFactory) wicFactory->Release();

        return SUCCEEDED(hr);
    }

    void DrawBitmap(
        ID2D1Bitmap* bitmap,
        float x,
        float y,
        float width,
        float height,
        float opacity = 1.0f)
    {
        if (!m_renderTarget || !bitmap || width <= 0.0f || height <= 0.0f)
            return;

        const bool ownsDrawSession = !m_isDrawing;
        if (ownsDrawSession)
            BeginBatch();

        m_renderTarget->DrawBitmap(
            bitmap,
            D2D1::RectF(x, y, x + width, y + height),
            opacity,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        if (ownsDrawSession)
            EndBatch();
    }

    void BeginBatch()
    {
        if (!m_renderTarget || m_isDrawing)
            return;

        m_renderTarget->BeginDraw();
        m_isDrawing = true;
    }

    void EndBatch()
    {
        if (!m_renderTarget || !m_isDrawing)
            return;

        const HRESULT hr = m_renderTarget->EndDraw();
        m_isDrawing = false;

        if (FAILED(hr))
            OutputDebugStringA("D2D DrawText failed\n");
    }


private:
    IDWriteFactory* m_factory = nullptr;
    IDWriteTextFormat* m_defaultFormat = nullptr;
    IDWriteTextFormat* m_smallFormat = nullptr;

    ID2D1Factory* m_d2dFactory = nullptr;
    ID2D1RenderTarget* m_renderTarget = nullptr;
    ID2D1SolidColorBrush* m_whiteBrush = nullptr;
    bool m_isDrawing = false;
};
