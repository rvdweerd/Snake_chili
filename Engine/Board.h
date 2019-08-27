#pragma once
#include "Graphics.h"
#include "Location.h"
#include "Colors.h"
#include <random>
#include "GameVariables.h"

class Board
{
public:
	enum  contentType {
		empty,
		food,
		poison,
		barrier
	};
	static constexpr Color foodColor = Colors::Blue;
	static constexpr Color barrierColor = Colors::White;
	static constexpr Color poisonColor = Colors::Magenta;

public:
	Board(Graphics& gfx_in, class Snake& snk_in);
	void DrawCell(Location& loc, Color c);
	void DrawBorders();
	void DrawCellContents();
	const int GetWidth();
	const int GetHeight();
	bool IsInsideBoard( const Location& loc) const;
	//void Spawn(contentType cellType, std::mt19937& rng, class Snake& snk, int n);
	void Spawn(contentType cellType, std::mt19937& rng, int n);
	contentType GetCellContent(Location loc);
	void SetCellContent(Location loc, contentType cellContent);

private:
	GameVariables gVar;
	const int dimension= gVar.tileSize;
	const Location startPos = { 10,10 };
	static constexpr int cellPadding = 1;
	Graphics& gfx;
	Snake& snk;
	//static constexpr int width =  35;
	int width = gVar.boardSizeX;
	int height = gVar.boardSizeY;
	
	//contentType masterArray[width * height] = { contentType::empty };
	contentType* masterArray = new contentType[width * height] { contentType::empty };
	
};