#include "pch.h"
#include "TransformedTriangleScene.h"

#include "Utils.h"

using namespace DirectX;

struct SimpleVertex
{
	XMFLOAT3 pos;
};

struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};

TransformedTriangleScene::TransformedTriangleScene(GPUDevice* gpuDevice)
	: ISceneNode(gpuDevice)
{
}

HRESULT TransformedTriangleScene::Initialize()
{
	HRESULT hr = S_OK;

	// Vertex Shader Configuration
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"TransformVertexShader.hlsl", "main", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL, L"The TransformVertexShader.hlsl file cannot be compiled", L"Error", MB_OK);
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
	{
		SimpleVertex vertices[] =
		{
			XMFLOAT3(0.0f,  0.5f, 0.5f),
			XMFLOAT3(0.5f, -0.5f, 0.5f),
			XMFLOAT3(-0.5f, -0.5f, 0.5f)
		};

		D3D11_BUFFER_DESC bd = { 0 };
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(SimpleVertex) * 3;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData = { 0 };
		InitData.pSysMem = vertices;
		hr = pd3dDevice->CreateBuffer(&bd, &InitData, &pVertexBuffer);
		if (FAILED(hr))
			return hr;
	}

	// Index Specification
	{
		WORD indices[] =
		{
			0, 1, 2
		};

		D3D11_BUFFER_DESC bd = { 0 };
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * 3;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData = { 0 };
		InitData.pSysMem = indices;
		hr = pd3dDevice->CreateBuffer(&bd, &InitData, &pIndexBuffer);
		if (FAILED(hr))
			return hr;
	}

	// Matrix Specification
	World = XMMatrixIdentity();

	XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	View = XMMatrixLookAtLH(Eye, At, Up);

	int width = gpuDevice->WindowWidth();
	int height = gpuDevice->WindowHeight();
	Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f);

	// Constant Buffer Construction
	{
		D3D11_BUFFER_DESC bd = { 0 };
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(ConstantBuffer);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = pd3dDevice->CreateBuffer(&bd, nullptr, &pConstantBuffer);
		if (FAILED(hr))
			return hr;
	}

	return S_OK;
}

void TransformedTriangleScene::Destroy()
{
	if (pConstantBuffer) pConstantBuffer->Release();
	if (pIndexBuffer) pIndexBuffer->Release();
	if (pVertexBuffer) pVertexBuffer->Release();
	if (pVertexLayout) pVertexLayout->Release();
	if (pVertexShader) pVertexShader->Release();
	if (pPixelShader) pPixelShader->Release();
}

void TransformedTriangleScene::Update(float dt)
{
	World = XMMatrixRotationY(XM_PI * 0.025f);
}

void TransformedTriangleScene::Render()
{
	auto pImmediateContext = gpuDevice->ImmediateContext();
	auto pRenderTargetView = gpuDevice->RenderTargetView();
	auto pDepthStencilView = gpuDevice->DepthStencilView();
	auto pSwapChain = gpuDevice->SwapChain();

	float clearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	pImmediateContext->ClearRenderTargetView(pRenderTargetView, clearColor);
	pImmediateContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose(World);
	cb.mView = XMMatrixTranspose(View);
	cb.mProjection = XMMatrixTranspose(Projection);
	pImmediateContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);

	pImmediateContext->IASetInputLayout(pVertexLayout);

	{
		UINT stride = sizeof(SimpleVertex);
		UINT offset = 0;
		pImmediateContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
	}

	{
		UINT offset = 0;
		pImmediateContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	}

	pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pImmediateContext->VSSetShader(pVertexShader, nullptr, 0);
	pImmediateContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
	pImmediateContext->PSSetShader(pPixelShader, nullptr, 0);
	pImmediateContext->DrawIndexed(3, 0, 0);

	pSwapChain->Present(0, 0);
}