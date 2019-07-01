#pragma once
#include "Graphics.h"
#include "Location.h"
#include "Colors.h"
#include <random>

class Board
{
public:
	Board(Graphics& gfx_in);
	void DrawCell(Location& loc, Color c);
	void DrawBorders();
	int GetWidth();
	int GetHeight();
	static constexpr int width = 35;
	static constexpr int height = 25;
	bool IsInsideBoard( const Location& loc) const;
	void SpawnNewBarrier(std::mt19937 rng, class Snake& snk, class Goal& goal);
	bool CellContainsBarrier(Location loc);
	void DrawBarriers();

private:
	static constexpr int dimension=20;
	const Location startPos = { 50,50 };
	static constexpr int cellPadding = 1;
	Graphics& gfx;
	bool BarrierArray[width * height] = { false };
	static constexpr Color BarrierColor = Colors::White;
};