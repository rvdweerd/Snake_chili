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
	void DrawCellContents();
	int GetWidth();
	int GetHeight();
	static constexpr int width = 35;
	static constexpr int height = 25;
	bool IsInsideBoard( const Location& loc) const;
	void SpawnNewBarrier(std::mt19937 rng, class Snake& snk, class Goal& goal);
	bool CellContainsPoison(Location loc);
	bool CellContainsBarrier(Location loc);
	void DrawBarriers();
	void DrawPoison();
	void FillBoardWithPoison(std::mt19937& rng, int poisonDensityPercentage);
	void Spawn(int cellType, std::mt19937& rng, Snake& snk, int n);
	int GetCellContent(Location loc);
	void SetCellContent(Location loc, int cellContent);


private:
	static constexpr int dimension=20;
	const Location startPos = { 50,50 };
	static constexpr int cellPadding = 1;
	Graphics& gfx;
	bool BarrierArray[width * height] = { false };
public:
	bool PoisonArray[width * height] = { false };
	int masterArray[width * height] = { 0 };
	// 0 = empty, 1=food, 2=poison, 3=barrier
	static constexpr Color foodColor = Colors::Blue;
	static constexpr Color barrierColor = Colors::White;
	static constexpr Color poisonColor = Colors::Magenta;
};