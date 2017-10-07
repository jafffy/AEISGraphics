#include "pch.h"

#include "Utils.h"

HRESULT CompileShaderFromFile(WCHAR* szFileName,
	LPCSTR szEntryPoint, LPCSTR szShaderModel,
	ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif // defined(DEBUG) || defined(_DEBUG)

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr,
		szEntryPoint, szShaderModel, dwShaderFlags, 0,
		ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob != nullptr)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}

	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}