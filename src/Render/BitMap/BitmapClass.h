#pragma once
#include <directxmath.h>
using namespace DirectX;

#include "Texture/Textureclass.h"

class BitmapClass {

private:
	struct VertexType {
		XMFLOAT3 position;
		XMFLOAT2 texture;

	};

public:
	BitmapClass();
	BitmapClass(const BitmapClass&);
	~BitmapClass();


	bool Initialize(ID3D11Device*, ID3D11DeviceContext*, int, int, const  char*, int, int);
	bool Initialize(ID3D11Device*, ID3D11DeviceContext*, int screenW, int screenH, int bitmapW, int bitmapH);
	void Shutdown();
	bool Render(ID3D11DeviceContext* deviceContext);

	int GetIndexCount();
	ID3D11ShaderResourceView* GetTexture();
	void SetRenderLocation(int, int);
	int GetBitmapWidth() const { return m_bitmapWidth; }
	int GetBitmapHeight() const { return m_bitmapHeight; }


private:
	bool InitializeBuffers(ID3D11Device*);
	void ShutdownBuffers();
	bool UpdateBuffers(ID3D11DeviceContext* deviceContext);
	void RenderBuffers(ID3D11DeviceContext*);
	bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename);
	void ReleaseTexture();

private:
	ID3D11Buffer* m_vertexBuffer, * m_indexBuffer;
	TextureClass* m_Texture;

	// Дополнительная информация для 2D
	int m_vertexCount, m_indexCount;
	int m_screenWidth, m_screenHeight;
	int m_bitmapWidth, m_bitmapHeight;
	int m_renderX, m_renderY;
	int m_prevPosX, m_prevPosY; // Для оптимизации обновлений




};