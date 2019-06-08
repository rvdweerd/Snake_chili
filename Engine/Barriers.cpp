#pragma once

#include "Barriers.h"
//#include "Goal.h"
#include <assert.h>

Barriers::Barriers()
	:
	nBarriers(0)
{
}

void Barriers::Draw(Board & brd)
{
	for (int i = 0; i < nBarriers; i++)
	{
		brd.DrawCell(loc[i], colBarrier);
	}

}

void Barriers::Add(Location new_loc)
{
	if (nBarriers < maxBarriers)
	{
		loc[nBarriers] = new_loc;
		nBarriers++;
	}
	
}

bool Barriers::IsHit(const Location & snkHead) const
{
	for (int i = 0; i < nBarriers; i++)
	{
		if (loc[i] == snkHead)
		{
			return true;
		}
	}
	return false;
}

void Barriers::Reset()
{
	nBarriers = 0;
}

int Barriers::getSize()
{
	return nBarriers;
}

bool Barriers::IsInBarrier(Location target)
{
	for (int i = 0; i < nBarriers-1; i++)
	{
		if (loc[i] == target)
		{
			return true;
		}
	}
	return false;
}

Location Barriers::GetLocation(const int nBar)
{
	assert(nBar < nBarriers);
	return loc[nBar];
}