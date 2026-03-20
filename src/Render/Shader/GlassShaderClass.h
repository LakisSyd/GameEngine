#pragma once
#include "BaseShader.h"

class GlassShaderClass : public ShaderBaseClass
{
	struct MatrixBufferType
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;
	};
	struct GlassBufferType
	{
		float refractionScale;
		XMFLOAT3 padding; // 16 байт для выравнивания
	};

public:
	GlassShaderClass() = default;
	~GlassShaderClass() = default;
	bool Initialize(ID3D11Device* device, HWND hwnd) override;
	bool Render(
		ID3D11DeviceContext* context,
		int                        indexCount,
		XMMATRIX                   worldMatrix,
		XMMATRIX                   viewMatrix,
		XMMATRIX                   projectionMatrix,
		ID3D11ShaderResourceView* colorTexture,
		ID3D11ShaderResourceView* normalTexture,
		ID3D11ShaderResourceView* refractionTexture,
		float                      refractionScale
	);

	void Shutdown() override;

	ShaderType GetType() const override { return ShaderType::Glass; }
private:
	bool SetShaderParameters(
		ID3D11DeviceContext* context,
		XMMATRIX                   world,
		XMMATRIX                   view,
		XMMATRIX                   projection,
		ID3D11ShaderResourceView* colorTexture,
		ID3D11ShaderResourceView* normalTexture,
		ID3D11ShaderResourceView* refractionTexture,
		float                      refractionScale
	);

	void RenderShader(ID3D11DeviceContext* context, int indexCount);

private:
	ID3D11Buffer* m_matrixBuffer = nullptr;
	ID3D11Buffer* m_glassBuffer = nullptr;
};
