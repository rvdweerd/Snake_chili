#pragma once
#include "Location.h"
#include "Board.h"

class Snake
{
private:
	class Segment
	{
	public:
		
		Segment();
		void InitHead(const Location& in_loc);
		void Follow(const Segment& segmentNext);
		void Draw(Board &brd);
		Location loc;
		Color color;
	};

public:
	Snake(const Location& startloc, const int size0, std::mt19937& rng);
	void Draw(Board& brd) ;
	void MoveTo(const Location& new_loc);
	void Grow(std::mt19937& rng);
	bool IsInTileExceptEnd(const Location& target) const;
	void Reset();
	void JumpOn();
	void JumpOff();
	bool IsMoving() const;
	Location GetCurrentHeadLocation() const;
	Location GetNextHeadLocation(const Location& delta, Board& brd) const;
	Location GetSnakeVelocity() const;
	void SetSnakeVelocity(const Location new_velocity);

private:
	static constexpr int jumpSize = 3;
	static constexpr Color headColor = Colors::Red;
	static constexpr Color bodyColor = Colors::White;
	static constexpr int nSegmentsMax = 2000;
	static constexpr int growth = 1;
	int jumpMultiplier = 1;
	Segment segments[nSegmentsMax];
	int nSegments;
	Location snakeVelocity;
};