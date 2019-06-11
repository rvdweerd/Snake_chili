#include "FrameTimer.h"


using namespace std::chrono;

FrameTimer::FrameTimer()
	:
	last(/*std::chrono::*/steady_clock::now())
{
	//last = std::chrono::steady_clock::now();
}

float FrameTimer::Mark()
{
	const steady_clock::time_point old = last;
	last = steady_clock::now();
	const /*std::chrono::*/duration<float> dur = last - old;
	return dur.count();
}