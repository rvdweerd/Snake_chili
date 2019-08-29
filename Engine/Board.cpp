#include "Board.h"
#include <assert.h>
#include "Location.h"
#include "Snake.h"


Board::Board(Graphics & gfx_in, Snake& snk_in, GameVariables& gVar)
	:
	dimension(gVar.tileSize),
	gfx(gfx_in),
	snk(snk_in),
	width(gVar.boardSizeX),
	height(gVar.boardSizeY)
	//masterArray(new contentType[width * height])// { contentType::empty } )
{
	/*for (contentType* pRunner = masterArray; pRunner < masterArray + width*height ; pRunner++)
	{
		*pRunner = contentType::empty;
	}*/
	for (int i = 0; i < width * height; i++)
	{
		masterArray.emplace_back(contentType::empty);
	}
}

Board::~Board()
{
	//delete[] masterArray;
	//masterArray = nullptr;
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
	std::uniform_int_distribution<int> arrayDistr(0, width*height);
	
	for (int nSpawns = 0; nSpawns < n; nSpawns++)
	{
		int i;
		do
		{
			i = arrayDistr(rng);
		} while (masterArray[i] != contentType::empty || snk.IsInTile({ i % (width) , (i - i % (width)) / width }));
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
