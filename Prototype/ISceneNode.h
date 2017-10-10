#ifndef ISCENENODE_H_
#define ISCENENODE_H_

#include "GPUDevice.h"

class ISceneNode
{
public:
	virtual ~ISceneNode() {}

	virtual HRESULT Initialize() { return S_OK; }
	virtual void Destroy() {}

	virtual void Update(float dt) {}
	virtual void Render() {}

protected:
	ISceneNode(GPUDevice* gpuDevice)
		: gpuDevice(gpuDevice)
	{}

	GPUDevice* gpuDevice = nullptr;
};

#endif // ISCENENODE_H_