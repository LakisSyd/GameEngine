////////////////////////////////////////////////////////////////////////////////
// Filename: textclass.h
////////////////////////////////////////////////////////////////////////////////
#ifndef _TEXTCLASS_H_
#define _TEXTCLASS_H_


///////////////////////
// MY CLASS INCLUDES //
///////////////////////
#include "fontclass.h"


////////////////////////////////////////////////////////////////////////////////
// Class name: TextClass
////////////////////////////////////////////////////////////////////////////////
class TextClass
{
private:
	struct VertexType
	{
		XMFLOAT3 position;
	    XMFLOAT2 texture;
	};

public:
	TextClass();
	TextClass(const TextClass&);
	~TextClass();

	bool Initialize(ID3D11Device*, ID3D11DeviceContext*, int, int, int maxLength, FontClass* Font);
	void Shutdown();
	void Render(ID3D11DeviceContext*);

	int GetIndexCount();

	bool UpdateText(char*, int, int, float, float, float);
    XMFLOAT4 GetPixelColor();

private:
	bool InitializeBuffers(ID3D11Device*, ID3D11DeviceContext*);
	void ShutdownBuffers();
	void RenderBuffers(ID3D11DeviceContext*);

private:


	ID3D11Device* m_device = nullptr;  // ← НОВОЕ
	ID3D11DeviceContext* m_deviceContext = nullptr;  // ← НОВОЕ


	FontClass* m_font;
	int m_maxLength;

	ID3D11Buffer *m_vertexBuffer, *m_indexBuffer;
	int m_screenWidth, m_screenHeight, m_vertexCount, m_indexCount;
	XMFLOAT4 m_pixelColor;
};

#endif