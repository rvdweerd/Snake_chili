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

private:
	static constexpr int dimension=20;
	static constexpr Location startPos = { 50,50 };
	static constexpr int cellPadding = 1;
	Graphics& gfx;
};