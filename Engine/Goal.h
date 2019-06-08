#pragma once

#include "Location.h"
#include "Board.h"
#include "Snake.h"
#include "Barriers.h"
#include <random>

class Goal 
{
public:
	Goal(Location goal_startloc);
	void Draw(Board& brd);
	void ReSpawn(Location new_loc);
	const Location GetLocation() const;
private:
	Location loc;
	Color col;

};

