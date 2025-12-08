#include "Snake.h"
#include <assert.h>
#include "Colors.h"

Snake::Snake(const Location& startloc, const int size0, std::mt19937& rng, Color headCol)
	: 
	snakeVelocity({1,0}),
	headColor(headCol)
{
	segments.emplace_back(Segment{startloc, headColor});
	
	std::uniform_int_distribution<int> colDistr(100, 255);
	for (int i = 1; i < size0; i++)
	{
		segments.emplace_back(Segment{ startloc, Color(10, colDistr(rng), 10) });
	}
}

void Snake::Draw(Board& brd)
{
	for ( const Segment& s : segments)
	{
		s.Draw(brd);
	}
	/*for (int i=segments.size()-1;i>=0;i--)
	{
		segments[i].Draw(brd);		
	}*/
}

void Snake::MoveTo(const Location& new_loc)
{
	if (segments[0].loc != new_loc)
	{
		for (size_t i = segments.size() - 1; i > 0; i--)
		{
			segments[i].Follow(segments[i - 1]);
		}
		segments.front().loc = new_loc;
	}
}

void Snake::Grow(std::mt19937& rng)
{
	std::uniform_int_distribution<int> colDistr(100, 255);
	int currentSnakeLength = int( segments.size() );
	if (currentSnakeLength + growth < nSegmentsMax)
	{
		for (int i = currentSnakeLength; i < currentSnakeLength+growth; i++)
		{
			Color newColor = Color(10, colDistr(rng), 10);
			segments.emplace_back(Segment{});
			segments[i].color = newColor;
			segments[i].Follow(segments[currentSnakeLength - 1]);
		}
	}
}

bool Snake::IsInTileExceptEnd(const Location& target) const
{
	for (int i = 0; i < segments.size()-1; i++)
	{
		if (segments[i].loc == target)
		{
			return true;
		}
	}
	return false;
}

bool Snake::IsInTile(const Location& target) const
{
	for (const Segment& s : segments)
	{
		if (s.loc == target)
		{
			return true;
		}
	}
	return false;
}

void Snake::Reset()
{
	segments.resize(2);
	//segments.clear();
	//segments.emplace_back(Segment{ {0,0} });
	//segments[0].InitHead({ 10,10 });
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
		new_loc.x = new_loc.x + brd.GetWidth();
	}
	else if (new_loc.x >= brd.GetWidth())
	{
		new_loc.x = new_loc.x - brd.GetWidth();
	}
	else if (new_loc.y < 0)
	{
		new_loc.y = new_loc.y + brd.GetHeight();
	}
	else if (new_loc.y >= brd.GetHeight())
	{
		new_loc.y = new_loc.y - brd.GetHeight();
	}
	return new_loc;
}

Location Snake::GetSnakeVelocity() const 
{
	return snakeVelocity * jumpMultiplier;
}

void Snake::SetSnakeVelocity(const Location new_velocity)
{
	if ( segments.size() == 1 )
	{
		snakeVelocity = new_velocity;
	}
	else if (segments[1].loc != segments[0].loc + new_velocity)
	{
		snakeVelocity = new_velocity;
	}
}

Snake::Segment::Segment()
{
}

Snake::Segment::Segment(const Location& in_loc, Color col)
{
	loc = in_loc;
	color = col;
}

void Snake::Segment::Follow(const Segment& segmentNext)
{
	loc = segmentNext.loc;
}

void Snake::Segment::Draw(Board& brd) const
{
	brd.DrawCell(loc, color);
}

int Snake::GetSegmentCount() const
{
	return static_cast<int>(segments.size());
}

Location Snake::GetSegmentLocation(int index) const
{
	return segments[index].loc;
}

void Snake::SetSegments(const Location* locations, int segmentCount)
{
	// Resize to match received segment count
	if (segmentCount > 0 && segmentCount < nSegmentsMax)
	{
		segments.resize(segmentCount);
		
		// Update all segment locations
		for (int i = 0; i < segmentCount; i++)
		{
			segments[i].loc = locations[i];
		}
	}
}







