#include "Board.h"
#include <assert.h>
#include "Location.h"
#include "Snake.h"

// Board change type values (must match BoardChangeType enum in NetworkManager.h)
namespace BoardChangeTypes {
	constexpr uint8_t FoodAdded = 0;
	constexpr uint8_t FoodRemoved = 1;
	constexpr uint8_t PoisonAdded = 2;
	constexpr uint8_t PoisonRemoved = 3;
	constexpr uint8_t BarrierAdded = 4;
	constexpr uint8_t BarrierRemoved = 5;
}


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

int Board::GetAllContentLocations(contentType type, Location* outLocations, int maxCount) const
{
	int count = 0;
	for (int i = 0; i < width * height && count < maxCount; i++)
	{
		if (masterArray[i] == type)
		{
			int x = i % width;
			int y = i / width;
			outLocations[count] = Location(x, y);
			count++;
		}
	}
	return count;
}

void Board::ClearAllContent()
{
	for (int i = 0; i < width * height; i++)
	{
		masterArray[i] = contentType::empty;
	}
}

void Board::SetContentFromLocations(contentType type, const Location* locations, int count)
{
	for (int i = 0; i < count; i++)
	{
		int index = locations[i].y * width + locations[i].x;
		if (index >= 0 && index < width * height)
		{
			masterArray[index] = type;
		}
	}
}

void Board::EnableChangeTracking(bool enable)
{
	trackChanges = enable;
	if (!enable)
	{
		pendingChanges.clear();
	}
}

bool Board::HasPendingChanges() const
{
	return !pendingChanges.empty();
}

int Board::GetPendingChanges(BoardChangeRecord* outChanges, int maxCount)
{
	int count = 0;
	for (size_t i = 0; i < pendingChanges.size() && count < maxCount; i++)
	{
		outChanges[count] = pendingChanges[i];
		count++;
	}
	return count;
}

void Board::ClearPendingChanges()
{
	pendingChanges.clear();
}

void Board::RecordChange(uint8_t changeType, const Location& loc)
{
	if (trackChanges && pendingChanges.size() < maxPendingChanges)
	{
		BoardChangeRecord record;
		record.changeType = changeType;
		record.loc = loc;
		pendingChanges.push_back(record);
	}
}

void Board::AddContent(contentType type, const Location& loc)
{
	int index = loc.y * width + loc.x;
	if (index >= 0 && index < width * height)
	{
		masterArray[index] = type;
		
		// Record the change for delta sync
		if (trackChanges)
		{
			uint8_t changeType = 0;
			switch (type)
			{
			case contentType::food:
				changeType = BoardChangeTypes::FoodAdded;
				break;
			case contentType::poison:
				changeType = BoardChangeTypes::PoisonAdded;
				break;
			case contentType::barrier:
				changeType = BoardChangeTypes::BarrierAdded;
				break;
			default:
				return; // Don't record empty
			}
			RecordChange(changeType, loc);
		}
	}
}

void Board::RemoveContent(const Location& loc)
{
	int index = loc.y * width + loc.x;
	if (index >= 0 && index < width * height)
	{
		contentType oldType = masterArray[index];
		masterArray[index] = contentType::empty;
		
		// Record the change for delta sync
		if (trackChanges && oldType != contentType::empty)
		{
			uint8_t changeType = 0;
			switch (oldType)
			{
			case contentType::food:
				changeType = BoardChangeTypes::FoodRemoved;
				break;
			case contentType::poison:
				changeType = BoardChangeTypes::PoisonRemoved;
				break;
			case contentType::barrier:
				changeType = BoardChangeTypes::BarrierRemoved;
				break;
			default:
				return;
			}
			RecordChange(changeType, loc);
		}
	}
}

void Board::ApplyChange(uint8_t changeType, const Location& loc)
{
	int index = loc.y * width + loc.x;
	if (index < 0 || index >= width * height)
	{
		return;
	}
	
	switch (changeType)
	{
	case BoardChangeTypes::FoodAdded:
		masterArray[index] = contentType::food;
		break;
	case BoardChangeTypes::FoodRemoved:
		if (masterArray[index] == contentType::food)
			masterArray[index] = contentType::empty;
		break;
	case BoardChangeTypes::PoisonAdded:
		masterArray[index] = contentType::poison;
		break;
	case BoardChangeTypes::PoisonRemoved:
		if (masterArray[index] == contentType::poison)
			masterArray[index] = contentType::empty;
		break;
	case BoardChangeTypes::BarrierAdded:
		masterArray[index] = contentType::barrier;
		break;
	case BoardChangeTypes::BarrierRemoved:
		if (masterArray[index] == contentType::barrier)
			masterArray[index] = contentType::empty;
		break;
	}
}
