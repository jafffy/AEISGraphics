#ifndef RENDERTRIANGLESCENE_H_
#define RENDERTRIANGLESCENE_H_

#include "GPUDevice.h"

class RenderTriangleScene
{
public:
	explicit RenderTriangleScene(GPUDevice* gpuDevice);

	HRESULT Initialize();
	void Destroy();

	void Update(float dt);
	void Render();

private:
	GPUDevice* gpuDevice = nullptr;
	
	ID3D11VertexShader* pVertexShader = nullptr;
	ID3D11PixelShader* pPixelShader = nullptr;
	ID3D11InputLayout* pVertexLayout = nullptr;
	ID3D11Buffer* pVertexBuffer = nullptr;
};

#endif // RENDERTRIANGLESCENE_H_