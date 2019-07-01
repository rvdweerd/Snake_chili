#include "Board.h"
#include <assert.h>
#include "Location.h"
#include "Snake.h"
#include "Goal.h"

Board::Board(Graphics & gfx_in)
	 :
	 gfx(gfx_in)
{
}

void Board::DrawCell(Location& loc, Color c)
{
	assert(loc.x < width);
	assert(loc.x >= 0);
	assert(loc.y >= 0);
	assert(loc.y < height);
	gfx.DrawRectDim(startPos.x+cellPadding+loc.x*dimension, startPos.y+cellPadding+loc.y*dimension, dimension-1*cellPadding, dimension-1*cellPadding, c);
}

void Board::DrawBorders()
{
	Color c = Colors::Red;
	for (int x = startPos.x; x <= startPos.x +1+ width*dimension; x++)
	{
		gfx.PutPixel(x, startPos.y, c);
		gfx.PutPixel(x, startPos.y+1+height*dimension, c);
	}

	for (int y = startPos.y; y <= startPos.y +1+ height*dimension; y++)
	{
		gfx.PutPixel(startPos.x, y, c);
		gfx.PutPixel(startPos.x+1+width*dimension, y, c);
	}
	
}

int Board::GetWidth()
{
	return width;
}

int Board::GetHeight()
{
	return height;
}

bool Board::IsInsideBoard(const Location & loc) const
{
	return (loc.x >=0 && loc.x <width) && (loc.y>=0 && loc.y < height);
}

void Board::SpawnNewBarrier(std::mt19937 rng, class Snake& snk, class Goal& goal)
{
	std::uniform_int_distribution<int> xDistr(0, Board::width - 1);
	std::uniform_int_distribution<int> yDistr(0, Board::height - 1);
	Location new_loc;
	do
	{
		new_loc = Location(xDistr(rng), yDistr(rng));
	} while (snk.IsInTileExceptEnd(new_loc) || goal.GetLocation()==new_loc || BarrierArray[ new_loc.y * width + new_loc.x ] == true);
	BarrierArray[new_loc.y * width + new_loc.x] = true;
}

bool Board::CellContainsBarrier(Location loc)
{
	return BarrierArray[loc.y * width + loc.x ];
}

bool Board::CellContainsPoison(Location loc)
{
	return PoisonArray[loc.y * width + loc.x];
}

void Board::DrawBarriers()
{
	for (int i = 0; i < width * height; i++)
	{
		if (BarrierArray[i])
		{
			DrawCell(Location( i%(width) , (i-i%(width))/(width) ), BarrierColor);
		}
	}
}

void Board::DrawPoison()
{
	for (int i = 0; i < width * height; i++)
	{
		if (PoisonArray[i])
		{
			DrawCell(Location(i % (width), (i - i % (width)) / (width)), PoisonColor);
		}
	}
}
