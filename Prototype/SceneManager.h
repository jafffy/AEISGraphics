#ifndef SCENEMANAGER_H_
#define SCENEMANAGER_H_

#include "Singleton.h"
#include "ISceneNode.h"

class SceneManager
	: public Singleton<SceneManager>
{
public:
	SceneManager() {}
	~SceneManager() {}

	void Destroy();

	HRESULT Add(ISceneNode* pNode);

	void Update(float dt);

	void Render();

private:
	std::vector<ISceneNode*> sceneNodes;
};

#endif // SCENEMANAGER_H_