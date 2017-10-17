#include "pch.h"
#include "RenderTriangleScene.h"

#include "Utils.h"

using namespace DirectX;

class VertexHasNormal
{
public:
	XMFLOAT3 pos;
};

RenderTriangleScene::RenderTriangleScene(GPUDevice* gpuDevice)
	: ISceneNode(gpuDevice)
{
}

HRESULT RenderTriangleScene::Initialize()
{
	HRESULT hr = S_OK;

	// Vertex Shader Configuration
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"VertexShader.hlsl", "main", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL, L"The VertexShader.hlsl file cannot be compiled", L"Error", MB_OK);
		return hr;
	}

	auto pd3dDevice = gpuDevice->D3DDevice();
	auto pImmediateContext = gpuDevice->ImmediateContext();

	hr = pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), nullptr, &pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	hr = pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Pixel Shader Configuration
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"PixelShader.hlsl", "main", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL, L"The PixelShader.hlsl file cannot be compiled", L"Error", MB_OK);
		return hr;
	}

	hr = pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
		nullptr, &pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Vertex Specification
	VertexHasNormal vertices[] =
	{
		XMFLOAT3(0.0f,  0.5f, 0.5f),
		XMFLOAT3(0.5f, -0.5f, 0.5f),
		XMFLOAT3(-0.5f, -0.5f, 0.5f)
	};
	D3D11_BUFFER_DESC bd = { 0 };
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VertexHasNormal) * 3;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData = { 0 };
	InitData.pSysMem = vertices;
	hr = pd3dDevice->CreateBuffer(&bd, &InitData, &pVertexBuffer);
	if (FAILED(hr))
		return hr;

	return S_OK;
}

void RenderTriangleScene::Destroy()
{
	if (pVertexBuffer) pVertexBuffer->Release();
	if (pVertexLayout) pVertexLayout->Release();
	if (pVertexShader) pVertexShader->Release();
	if (pPixelShader) pPixelShader->Release();
}

void RenderTriangleScene::Update(float dt)
{

}

void RenderTriangleScene::Render()
{
	auto pSwapChain = gpuDevice->SwapChain();
	auto pRenderTargetView = gpuDevice->RenderTargetView();
	auto pImmediateContext = gpuDevice->ImmediateContext();

	float clearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	pImmediateContext->ClearRenderTargetView(pRenderTargetView, clearColor);

	pImmediateContext->IASetInputLayout(pVertexLayout);

	UINT stride = sizeof(VertexHasNormal);
	UINT offset = 0;
	pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
	pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pImmediateContext->VSSetShader(pVertexShader, nullptr, 0);
	pImmediateContext->PSSetShader(pPixelShader, nullptr, 0);
	pImmediateContext->Draw(3, 0);

	pSwapChain->Present(0, 0);
}