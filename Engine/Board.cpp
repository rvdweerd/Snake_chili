#include "Board.h"
#include <assert.h>
#include "Location.h"
#include "Snake.h"


Board::Board(Graphics & gfx_in, Snake& snk1_in, Snake& snk2_in, GameVariables& gVar)
	:
	dimension(gVar.tileSize),
	gfx(gfx_in),
	snk1(snk1_in),
	snk2(snk2_in),
	width(gVar.boardSizeX),
	height(gVar.boardSizeY),
	masterArray(width * height, contentType::empty)// { contentType::empty } )
{
}

void Board::DrawCell(const Location& loc, Color c) const
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

void Board::DrawCellContents()
{
	for (int i = 0; i < width * height; i++)
	{
		switch (masterArray[i]) 
		{
		case contentType::food : DrawCell(Location(i % (width), (i - i % (width)) / (width)), foodColor);
			break;
		case contentType::poison : DrawCell(Location(i % (width), (i - i % (width)) / (width)), poisonColor);
			break;
		case contentType::barrier : DrawCell(Location(i % (width), (i - i % (width)) / (width)), barrierColor);
			break;
		}
	}
}

const int Board::GetWidth()
{
	return width;
}

const int Board::GetHeight()
{
	return height;
}

bool Board::IsInsideBoard(const Location & loc) const
{
	return (loc.x >=0 && loc.x <width) && (loc.y>=0 && loc.y < height);
}

//void Board::Spawn(contentType cellType, std::mt19937& rng, Snake& snk, int n)
void Board::Spawn(contentType cellType, std::mt19937& rng, int n)
{
	std::uniform_int_distribution<int> arrayDistr(0, width*height - 1);
	
	for (int nSpawns = 0; nSpawns < n; nSpawns++)
	{
		int i;
		Location testLoc;
		do
		{
			i = arrayDistr(rng);
			testLoc = { i % width , (i - i % width) / width };
		} while (masterArray[i] != contentType::empty || 
		         snk1.IsInTile(testLoc) || 
		         snk2.IsInTile(testLoc));
		masterArray[i] = cellType;
	}
}

Board::contentType Board::GetCellContent(Location loc)
{
	return masterArray[loc.y*width+loc.x];
}

void Board::SetCellContent(Location loc, contentType cellContent)
{
	masterArray[loc.y * width + loc.x] = cellContent;
}
