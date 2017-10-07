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

private:
	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	ID3D11Device* pd3dDevice = nullptr;
	ID3D11DeviceContext* pImmediateContext = nullptr;
	IDXGISwapChain* pSwapChain = nullptr;
	ID3D11RenderTargetView* pRenderTargetView = nullptr;
};

#endif // GPUDEVICE_H_