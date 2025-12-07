#pragma once

#include "Graphics.h"

class SpriteCodex
{
public:
	static void DrawGameOver( int x,int y,Graphics& gfx );
	static void DrawTitle( int x,int y,Graphics& gfx );
	static void DrawNumber(int number, int x, int y, Graphics& gfx);
	static void DrawDigit(int digit, int x, int y, Graphics& gfx);
};