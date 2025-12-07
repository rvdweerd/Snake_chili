#pragma once

#include <fstream>
#include <string>
#include <iostream>

class GameVariables
{
public:
	GameVariables(std::string& filename)
	{
		std::ifstream in(filename);
		for (std::string line; std::getline(in, line); )
		{
			if (line == "[Tile Size]")
			{
				in >> tileSize;
			}
			if (line == "[Board Size]")
			{
				in >> boardSizeX >> boardSizeY;
			}
			if (line == "[Speedup Rate]")
			{
				in >> speedupRate;
			}
			if (line == "[Poison Amount]")
			{
				in >> poisonAmount;
			}
			if (line == "[Food Amount]")
			{
				in >> foodAmount;
			}
			if (line == "[Initial Speed]")
			{
				in >> initialSpeed;
			}
			if (line == "[Initial Snakelength]")
			{
				in >> initialSnakelength;
				}
			if (line == "[Num Players]")
			{
				in >> numPlayers;
			}
		}
	}

public:
	int tileSize;
	int boardSizeX;
	int boardSizeY;
	float speedupRate;
	int poisonAmount;
	int foodAmount;
	float initialSpeed;
	int initialSnakelength;
	int numPlayers = 1; // Default to single-player mode
};
