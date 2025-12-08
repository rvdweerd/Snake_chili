#pragma once
#include "Location.h"
#include "Board.h"
#include <vector>

class Snake
{
private:
	class Segment
	{
	public:
		Segment();
		Segment(const Location& in_loc, Color col);
		void Follow(const Segment& segmentNext);
		void Draw(Board &brd) const;
		Location loc;
		Color color;
	};

public:
	Snake(const Location& startloc, const int size0, std::mt19937& rng, Color headCol = Colors::Red);
	void Draw(Board& brd) ;
	void MoveTo(const Location& new_loc);
	void Grow(std::mt19937& rng);
	bool IsInTileExceptEnd(const Location& target) const;
	bool Snake::IsInTile(const Location& target) const;
	void Reset();
	void JumpOn();
	void JumpOff();
	bool IsMoving() const;
	Location GetCurrentHeadLocation() const;
	Location GetNextHeadLocation(const Location& delta, Board& brd) const;
	Location GetSnakeVelocity() const;
	Location GetBaseVelocity() const;  // Get velocity without jump multiplier
	void SetSnakeVelocity(const Location new_velocity);
	
	// Network serialization methods
	int GetSegmentCount() const;
	Location GetSegmentLocation(int index) const;
	void SetSegments(const Location* locations, int segmentCount);

private:
	static constexpr int jumpSize = 3;
	Color headColor;
	static constexpr Color bodyColor = Colors::White;
	static constexpr int nSegmentsMax = 2000;
	static constexpr int growth = 1;
	int jumpMultiplier = 1;
	//Segment segments[nSegmentsMax];
	std::vector<Segment> segments;
	Location snakeVelocity;
};