/****************************************************************************************** 
 *	Chili DirectX Framework Version 16.07.20											  *	
 *	Game.cpp																			  *
 *	Copyright 2016 PlanetChili.net <http://www.planetchili.net>							  *
 *																						  *
 *	This file is part of The Chili DirectX Framework.									  *
 *																						  *
 *	The Chili DirectX Framework is free software: you can redistribute it and/or modify	  *
 *	it under the terms of the GNU General Public License as published by				  *
 *	the Free Software Foundation, either version 3 of the License, or					  *
 *	(at your option) any later version.													  *
 *																						  *
 *	The Chili DirectX Framework is distributed in the hope that it will be useful,		  *
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of						  *
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the						  *
 *	GNU General Public License for more details.										  *
 *																						  *
 *	You should have received a copy of the GNU General Public License					  *
 *	along with The Chili DirectX Framework.  If not, see <http://www.gnu.org/licenses/>.  *
 ******************************************************************************************/
#include "MainWindow.h"
#include "Game.h"
#include "SpriteCodex.h"



Game::Game( MainWindow& wnd )
	:
	wnd( wnd ),
	gfx( wnd ),
	brd( gfx, snk ),
	rng(std::random_device()() ),
	snk({0,0},1,rng),
	xDistr(0, brd.GetWidth() - 1),
	yDistr(0, brd.GetHeight() - 1)
{
	//int coveragePercentage = 10;
	//int nPoison = Board::GetWidth() * Board::GetHeight() * coveragePercentage / 100;
	

	brd.Spawn(Board::contentType::food, rng,  std::max(1,gVar.foodAmount) );			
	brd.Spawn(Board::contentType::poison, rng, gVar.poisonAmount );
	brd.Spawn(Board::contentType::barrier, rng,  0);		
}

void Game::Go()
{
	gfx.BeginFrame();	
	UpdateModel();
	ComposeFrame();
	gfx.EndFrame();
}

void Game::UpdateModel()
{
	float dt = frmTime.Mark();
	if (isStarted)
	{
		//Control direction of snake
		if (wnd.kbd.KeyIsPressed(0x66))	{ snk.SetSnakeVelocity( { 1, 0} );}	// move right
		if (wnd.kbd.KeyIsPressed(0x64))	{ snk.SetSnakeVelocity( {-1, 0} );}	// move left
		if (wnd.kbd.KeyIsPressed(0x62))	{ snk.SetSnakeVelocity( { 0, 1} );}	// move down
		if (wnd.kbd.KeyIsPressed(0x68))	{ snk.SetSnakeVelocity( { 0,-1} );}	// move up
		//Adjust speed 
		if (wnd.kbd.KeyIsPressed(0x61)) { snkMovePeriod *= 1.05f; }			// slower
		if (wnd.kbd.KeyIsPressed(0x67)) { snkMovePeriod /= 1.05f; }			// faster
		if (wnd.kbd.KeyIsPressed(0x69)) { snk.SetSnakeVelocity( { 0,0 } );}	// stall
		//Jump
		if (wnd.kbd.KeyIsPressed(0x65)) { snk.JumpOn();}						//jump

		if (!gameOver)
		{	
			snkMoveCounter+=dt;
			if (snkMoveCounter >= snkMovePeriod)
			{
				const Location new_loc = snk.GetNextHeadLocation(snk.GetSnakeVelocity(), brd);
		
				//grow if eats food
				if (brd.GetCellContent(new_loc) == Board::contentType::food)
				{
					snk.Grow(rng);
					brd.Spawn(Board::contentType::food    , rng ,  1); 
					brd.Spawn(Board::contentType::barrier , rng ,  1); 
					brd.SetCellContent(new_loc, Board::contentType::empty);
				}

				//speed up if eats poison
				if (brd.GetCellContent(new_loc) == Board::contentType::poison) 
				{
					brd.SetCellContent(new_loc, Board::contentType::empty);
					snkMovePeriod /= gVar.speedupRate;//1.05f;
				}

				//game over conditions
				if	( snk.IsInTileExceptEnd(new_loc) &&  snk.IsMoving() ||			// snake eats itself
					  brd.GetCellContent(new_loc) == Board::contentType::barrier)	// snake hits barrier
				{
					gameOver = true;
				}
				
				//Optional game over condition if out of bounds (keep gameOver to false to allow re-entering the board)
				if (!brd.IsInsideBoard( snk.GetCurrentHeadLocation() + snk.GetSnakeVelocity() ))
				{
					//gameOver = true;
				}

				//move the snake 
				if (!gameOver) 
				{
					snk.MoveTo( new_loc ); 
					snk.JumpOff();
				}
				
				//reset framecounter
				snkMoveCounter = 0;
			}
		}
	}
	
	//handle game (re)start
	{
		if (wnd.kbd.KeyIsPressed(VK_RETURN))
		{
			isStarted = true;
			gameOver = false;
			//snk.Reset();
		}
	}
}

void Game::ComposeFrame()
{
	if (isStarted)
	{
		brd.DrawBorders();
		brd.DrawCellContents();
		snk.Draw(brd);
	} 
	else
	{
		SpriteCodex::DrawTitle(100, 100, gfx);
	}	
	if (gameOver)
	{
		SpriteCodex::DrawGameOver(200, 200, gfx);
	}
}