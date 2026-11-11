#pragma once
#include <d3d11.h>
#include <directxmath.h>
using namespace DirectX;

////////////////////////////////////////////////////////////////////////////////
// Имя класса: RenderTextureClass
////////////////////////////////////////////////////////////////////////////////
class RenderTextureClass
{
public:
    RenderTextureClass();
    RenderTextureClass(const RenderTextureClass&) = delete;
    RenderTextureClass& operator=(const RenderTextureClass&) = delete;
    ~RenderTextureClass();

    // Инициализация: создаёт текстуру, целевое представление и буфер глубины
    bool Initialize(ID3D11Device*, int textureWidth, int textureHeight,
        float screenDepth, float screenNear, int format);

    void Shutdown();  // Освобождение ресурсов

    // Переключает целевое представление рендеринга на эту  текстуру
    void SetRenderTarget(ID3D11DeviceContext*);

    // Очищает текстуру и буфер глубины перед рендерингом
    void ClearRenderTarget(ID3D11DeviceContext*, float red, float green,
        float blue, float alpha);

    // Возвращает представление ресурса шейдера для использования текстуры
    ID3D11ShaderResourceView* GetShaderResourceView();

    // Вспомогательные функции для получения матриц и размеров
    void GetProjectionMatrix(XMMATRIX&);
    void GetOrthoMatrix(XMMATRIX&);
    int GetTextureWidth();
    int GetTextureHeight();

private:
    int m_textureWidth, m_textureHeight;

    // 👇 Основные ресурсы рендер-текстуры
    ID3D11Texture2D* m_renderTargetTexture;        // Сама текстура
    ID3D11RenderTargetView* m_renderTargetView;    // Представление для рендеринга
    ID3D11ShaderResourceView* m_shaderResourceView; // Представление для шейдеров
    ID3D11Texture2D* m_depthStencilBuffer;         // Буфер глубины
    ID3D11DepthStencilView* m_depthStencilView;    // Представление буфера глубины

    D3D11_VIEWPORT m_viewport;  // Вьюпорт для этой текстуры
    XMMATRIX m_projectionMatrix; // Матрица проекции с учётом аспекта текстуры
    XMMATRIX m_orthoMatrix;      // Ортографическая матрица для 2D-рендеринга
};
