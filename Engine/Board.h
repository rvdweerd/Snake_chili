#pragma once
#include "Graphics.h"
#include "Location.h"
#include "Colors.h"
#include <random>

class Board
{
public:
	enum class contentType {
		empty,
		food,
		poison,
		barrier
	};
	static constexpr Color foodColor = Colors::Blue;
	static constexpr Color barrierColor = Colors::White;
	static constexpr Color poisonColor = Colors::Magenta;

public:
	Board(Graphics& gfx_in);
	void DrawCell(Location& loc, Color c);
	void DrawBorders();
	void DrawCellContents();
	static int GetWidth();
	static int GetHeight();
	bool IsInsideBoard( const Location& loc) const;
	void DrawBarriers();
	void Spawn(contentType cellType, std::mt19937& rng, class Snake& snk, int n);
	contentType GetCellContent(Location loc);
	void SetCellContent(Location loc, contentType cellContent);

private:
	static constexpr int dimension=50;//20;
	const Location startPos = { 50,50 };
	static constexpr int cellPadding = 1;
	Graphics& gfx;
	static constexpr int width = 5;// 35;
	static constexpr int height = 5;// 25;
	bool BarrierArray[width * height] = { false };
	contentType masterArray[width * height] = { contentType::empty };
};