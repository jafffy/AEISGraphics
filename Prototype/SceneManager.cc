#include "pch.h"

#include "SceneManager.h"

void SceneManager::Destroy()
{
	for (auto sceneNode : sceneNodes)
	{
		if (sceneNode != nullptr)
			delete sceneNode;
	}

	sceneNodes.clear();
}

HRESULT SceneManager::Add(ISceneNode* pNode)
{
	if (pNode == nullptr)
		return E_INVALIDARG;

	sceneNodes.push_back(pNode);

	return S_OK;
}

void SceneManager::Update(float dt)
{
	for (auto sceneNode : sceneNodes)
	{
		sceneNode->Update(dt);
	}
}

void SceneManager::Render()
{
	for (auto sceneNode : sceneNodes)
	{
		sceneNode->Render();
	}
}