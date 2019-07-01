#pragma once
#include "Colors.h"

class Location
{
public:
	Location() = default;
	Location(int x_in, int y_in)
	{
		x = x_in;
		y = y_in;
	}
	bool operator==(const Location& rhs) const
	{
		return (x == rhs.x && y == rhs.y);
	}
	Location operator*(int k) const
	{
		return { x*k,y*k };
	}
	Location operator+(const Location& rhs) const
	{
		return { x + rhs.x, y + rhs.y };
	}

	bool operator!=(const Location& rhs) const
	{
		return !( x == rhs.x && y == rhs.y );
	}

	/*void Add(const Location &delta)
	{
		x += delta.x;
		y += delta.y;
	}*/
	int x;
	int y;
};