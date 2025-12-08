#pragma once
#include "Graphics.h"
#include "Location.h"
#include "Colors.h"
#include <random>
#include "GameVariables.h"
#include <vector>

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
	Board(Graphics& gfx_in, class Snake& snk1_in, class Snake& snk2_in, GameVariables& gVar);
	void DrawCell(const Location& loc, Color c) const;
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
	Snake& snk1;
	Snake& snk2;
	int width;
	int height;
	
	//contentType masterArray[width * height] = { contentType::empty };
	//contentType* masterArray = nullptr;
	std::vector<contentType> masterArray;
	
};