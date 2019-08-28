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
	Board() = default;
	Board(Graphics& gfx_in, class Snake& snk_in, GameVariables& gVar);
	~Board();
	Board(const Board& ) = delete;
	const Board& operator=(const Board& ) = delete;
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
	int dimension;
	const Location startPos = { 10,10 };
	static constexpr int cellPadding = 1;
	Graphics& gfx;
	Snake& snk;
	//static constexpr int width =  35;
	int width;
	int height;
	
	//contentType masterArray[width * height] = { contentType::empty };
	contentType* masterArray = nullptr;
	
};