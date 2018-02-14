#ifndef FRAMERATECONTROLLER_H_
#define FRAMERATECONTROLLER_H_

#include "Common\Singleton.h"

class FramerateController
	: public Singleton<FramerateController>
{
public:
	FramerateController();
	~FramerateController();

	void Start();

	void Tick();
	void SetFramerate(double frameratePerSecond);

private:
	LARGE_INTEGER lastTime;
	LARGE_INTEGER frequency;
	LARGE_INTEGER qpcMaxDelta;

	double wantedFramePerSecond;
};

#endif // FRAMERATECONTROLLER_H_
