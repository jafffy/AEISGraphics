#ifndef UTILS_H_
#define UTILS_H_

HRESULT CompileShaderFromFile(const WCHAR* szFileName,
	LPCSTR szEntryPoint, LPCSTR szShaderModel,
	ID3DBlob** ppBlobOut);

#endif // UTILS_H_