#pragma once

#include <fstream>
#include <string>
#include <iostream>

class GameVariables
{
public:
	GameVariables()
		:
		filename("data.txt"),
		tileSize(	  LoadIntValue("[Tile Size]", 1) ),
		boardSizeX(	  LoadIntValue("[Board Size]", 1) ),
		boardSizeY(	  LoadIntValue("[Board Size]", 2) ),
		speedupRate(  LoadFloatValue("[Speedup Rate]", 1) ),
		poisonAmount( LoadIntValue("[Poison Amount]", 1) ),
		foodAmount(	  LoadIntValue("[Food Amount]", 1) )
	{
	}
	bool Lineup(std::ifstream& infile, std::string target)
	{
		std::string  string;
		for (std::getline(infile, string); !infile.eof(); std::getline(infile, string))
		{
			if (string == target) return true;
		}
		std::cout << "\n Variables not found in data file\n";
		return false;
	}
	int LoadIntValue(std::string target, int element)
	{
		std::ifstream infile(filename);
		if (!infile.good()) return 0;

		if (Lineup(infile, target))
		{
			int value=0;
			for (int i = 0; i < element; i++)
			{
				infile >> value;
			}
			infile.close();
			return value;
		}
		infile.close(); 
		return 0;
	}
	float LoadFloatValue(std::string target, float element)
	{
		std::ifstream infile(filename);
		if (!infile.good()) return 0;

		if (Lineup(infile, target))
		{
			float value = 0;
			for (int i = 0; i < element; i++)
			{
				infile >> value;
			}
			infile.close();
			return value;
		}
		infile.close();
		return 0;
	}
	

public:
	const std::string filename;
	const int tileSize;
	int boardSizeX;
	int boardSizeY;
	const float speedupRate;
	const int poisonAmount;
	const int foodAmount;
};
