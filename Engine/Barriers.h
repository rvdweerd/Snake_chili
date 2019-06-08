#pragma once
#include "Location.h"
#include "Board.h"
#include "Snake.h"
#include <random>
#include "Goal.h"

class Barriers
{
public:
	Barriers();
	void Draw(Board& brd);
	void Add(Location new_loc);
	bool IsHit(const Location& snkHead) const;
	void Reset();
	int getSize();
	bool IsInBarrier(Location target);
	Location GetLocation(const int nBar);
private:
	static constexpr int maxBarriers = 100;
	Location loc[maxBarriers];
	int nBarriers;
private:
	const Color colBarrier = Colors::White;
	

};
