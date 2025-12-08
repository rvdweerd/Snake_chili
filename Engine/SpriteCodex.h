#pragma once

#include "Graphics.h"
#include <string>

class SpriteCodex
{
public:
	static void DrawGameOver( int x,int y,Graphics& gfx );
	static void DrawTitle( int x,int y,Graphics& gfx );
	static void DrawNumber(int number, int x, int y, Graphics& gfx);
	static void DrawDigit(int digit, int x, int y, Graphics& gfx);
	static void DrawChar(char c, int x, int y, Color color, Graphics& gfx);
	static void DrawString(const std::string& text, int x, int y, Color color, Graphics& gfx);
};