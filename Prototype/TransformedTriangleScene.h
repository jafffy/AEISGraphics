#ifndef TRANSFORMEDTRIANGLESCENE_H_
#define TRANSFORMEDTRIANGLESCENE_H_

#include "ISceneNode.h"

class TransformedTriangleScene : public ISceneNode
{
public:
	explicit TransformedTriangleScene(GPUDevice* gpu);

	virtual HRESULT Initialize();
	virtual void Destroy();

	virtual void Update(float dt);
	virtual void Render();

private:
	ID3D11VertexShader* pVertexShader = nullptr;
	ID3D11PixelShader* pPixelShader = nullptr;
	ID3D11InputLayout* pVertexLayout = nullptr;
	ID3D11Buffer* pVertexBuffer = nullptr;
	ID3D11Buffer* pIndexBuffer = nullptr;
	ID3D11Buffer* pConstantBuffer = nullptr;

	DirectX::XMMATRIX World;
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Projection;

	int IndexCount = 0;
};

#endif // TRANSFORMEDTRIANGLESCENE_H_