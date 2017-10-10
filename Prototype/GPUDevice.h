#ifndef GPUDEVICE_H_
#define GPUDEVICE_H_

class GPUDevice
{
public:
	HRESULT Initialize(HWND hWnd);
	void Destory();

	ID3D11Device* D3DDevice() {
		assert(pd3dDevice != nullptr);
		return pd3dDevice;
	}
	ID3D11DeviceContext* ImmediateContext() {
		assert(pImmediateContext != nullptr);
		return pImmediateContext;
	}
	IDXGISwapChain* SwapChain() {
		assert(pSwapChain != nullptr);
		return pSwapChain;
	}
	ID3D11RenderTargetView* RenderTargetView() {
		assert(pRenderTargetView != nullptr);
		return pRenderTargetView;
	}
	ID3D11DepthStencilView* DepthStencilView() {
		assert(pDepthStencilView != nullptr);
		return pDepthStencilView;
	}

	int WindowWidth() const { return windowWidth; }
	int WindowHeight() const { return windowHeight; }

private:
	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	ID3D11Device* pd3dDevice = nullptr;
	ID3D11DeviceContext* pImmediateContext = nullptr;
	IDXGISwapChain* pSwapChain = nullptr;
	ID3D11RenderTargetView* pRenderTargetView = nullptr;
	ID3D11Texture2D* pDepthStencil = nullptr;
	ID3D11DepthStencilView* pDepthStencilView = nullptr;

	int windowWidth;
	int windowHeight;
};

#endif // GPUDEVICE_H_