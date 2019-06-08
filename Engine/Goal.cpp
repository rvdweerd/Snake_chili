#include "Goal.h"
#include <assert.h>
#include "Barriers.h"

Goal::Goal(Location goal_startloc)
	:
	loc(goal_startloc),
	col(Colors::Blue)
{
}

void Goal::Draw(Board & brd)
{
	brd.DrawCell(loc, col);
}

void Goal::ReSpawn(Location new_loc)
{
	loc = new_loc;
}

const Location Goal::GetLocation() const
{
	return loc;
}

