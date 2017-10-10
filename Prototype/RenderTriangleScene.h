#ifndef RENDERTRIANGLESCENE_H_
#define RENDERTRIANGLESCENE_H_

#include "ISceneNode.h"

class RenderTriangleScene : public ISceneNode
{
public:
	explicit RenderTriangleScene(GPUDevice* device);

	virtual HRESULT Initialize();
	virtual void Destroy();

	virtual void Update(float dt);
	virtual void Render();

private:
	ID3D11VertexShader* pVertexShader = nullptr;
	ID3D11PixelShader* pPixelShader = nullptr;
	ID3D11InputLayout* pVertexLayout = nullptr;
	ID3D11Buffer* pVertexBuffer = nullptr;
};

#endif // RENDERTRIANGLESCENE_H_