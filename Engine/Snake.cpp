#include "Snake.h"
#include <assert.h>
#include "Colors.h"
//#include "Board.h"

Snake::Snake(const Location& startloc, const int size0, std::mt19937& rng)
	: 
	nSegments(size0),
	snakeVelocity({1,0})
{
	segments[0].InitHead(startloc);
	std::uniform_int_distribution<int> colDistr(100, 255);
	for (int i = 1; i < nSegments; i++)
	{
		segments[i] = segments[0];
		segments[i].color = Color(10, colDistr(rng), 10);
	}
}

void Snake::Draw(Board& brd)
{
	for (int i=nSegments-1;i>=0;i--)
	{
		segments[i].Draw(brd);		
	}
}

void Snake::MoveTo(const Location& new_loc)
{
	if (segments[0].loc != new_loc)
	{
		for (int i = nSegments - 1; i > 0; i--)
		{
			segments[i].Follow(segments[i - 1]);
		}
		segments[0].loc = new_loc;
	}
}

void Snake::Grow(std::mt19937& rng)
{
	std::uniform_int_distribution<int> colDistr(100, 255);
	if (nSegments+growth < nSegmentsMax)
	{
		for (int i = nSegments; i <= nSegments+growth; i++)
		{
			segments[i].Follow(segments[nSegments - 1]);
			segments[i].color = Color(10, colDistr(rng), 10);
		}
		nSegments+=growth;
	}
}

bool Snake::IsInTileExceptEnd(const Location& target) const
{
	for (int i = 0; i < nSegments-1; i++)
	{
		if (segments[i].loc == target)
		{
			return true;
		}
	}
	return false;
}

void Snake::Reset()
{
	nSegments = 1;
	segments[0].InitHead({ 10,10 });
}

void Snake::JumpOn()
{
	jumpMultiplier = jumpSize+1;
}

void Snake::JumpOff()
{
	jumpMultiplier = 1;
}

bool Snake::IsMoving() const
{
	return (abs(snakeVelocity.x) + abs(snakeVelocity.y)) > 0;
}

Location Snake::GetCurrentHeadLocation() const
{
	return segments[0].loc;
}

Location Snake::GetNextHeadLocation(const Location& delta, Board& brd) const
{
	Location new_loc ( segments[0].loc + delta );

	if (new_loc.x < 0)
	{
		new_loc.x = new_loc.x + brd.width;
	}
	else if (new_loc.x >= brd.width)
	{
		new_loc.x = new_loc.x - brd.width;
	}
	else if (new_loc.y < 0)
	{
		new_loc.y = new_loc.y + brd.height;
	}
	else if (new_loc.y >= brd.height)
	{
		new_loc.y = new_loc.y - brd.height;
	}
	return new_loc;
}

Location Snake::GetSnakeVelocity() const 
{
	return snakeVelocity * jumpMultiplier;
}

void Snake::SetSnakeVelocity(const Location new_velocity)
{
	if ( (segments[1].loc != segments[0].loc + new_velocity) || nSegments == 1 )
	{
		snakeVelocity = new_velocity;
	}
}

Snake::Segment::Segment()
{
}

void Snake::Segment::InitHead(const Location& in_loc)
{
	loc = in_loc;
	color = Snake::headColor;
}

void Snake::Segment::Follow(const Segment& segmentNext)
{
	loc = segmentNext.loc;
}

void Snake::Segment::Draw(Board& brd)
{
	brd.DrawCell(loc, color);
}



