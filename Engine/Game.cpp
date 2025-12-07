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



Game::Game(MainWindow& wnd)
	:
	wnd(wnd),
	gfx(wnd),
	//gVar(std::string("data.txt")),
	rng(std::random_device()()),
	snk1({ 0,0 }, gVar.initialSnakelength, rng),
	snk2({ gVar.boardSizeX - 1, gVar.boardSizeY - 1 }, gVar.initialSnakelength, rng), // Second snake starts at the opposite corner
	brd(gfx, snk1, gVar),
	xDistr(0, brd.GetWidth() - 1),
	yDistr(0, brd.GetHeight() - 1)
{
	//int coveragePercentage = 10;
	//int nPoison = Board::GetWidth() * Board::GetHeight() * coveragePercentage / 100;

	snk1MovePeriod = gVar.initialSpeed;
	snk2MovePeriod = gVar.initialSpeed;
	brd.Spawn(Board::contentType::food, rng, std::max(1, gVar.foodAmount));
	brd.Spawn(Board::contentType::poison, rng, gVar.poisonAmount);
	brd.Spawn(Board::contentType::barrier, rng, 0);
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
		if (wnd.kbd.KeyIsPressed(0x66))	{ snk1.SetSnakeVelocity( { 1, 0} );}	// move right
		if (wnd.kbd.KeyIsPressed(0x64))	{ snk1.SetSnakeVelocity( {-1, 0} );}	// move left
		if (wnd.kbd.KeyIsPressed(0x62))	{ snk1.SetSnakeVelocity( { 0, 1} );}	// move down
		if (wnd.kbd.KeyIsPressed(0x68))	{ snk1.SetSnakeVelocity( { 0,-1} );}	// move up
		//Adjust speed 
		if (wnd.kbd.KeyIsPressed(0x61)) { snk1MovePeriod *= 1.05f; }			// slower 7
		if (wnd.kbd.KeyIsPressed(0x67)) { snk1MovePeriod /= 1.05f; }			// faster 1
		if (wnd.kbd.KeyIsPressed(0x69)) { snk1.SetSnakeVelocity( { 0,0 } );}	// stall 9
		//Jump
		if (wnd.kbd.KeyIsPressed(0x65)) { snk1.JumpOn();}						//jump
		
		// Control second snake (if two players)
		if (gVar.numPlayers == 2)
		{
			if (wnd.kbd.KeyIsPressed('D')) { snk2.SetSnakeVelocity({ 1, 0 }); } // move right
			if (wnd.kbd.KeyIsPressed('A')) { snk2.SetSnakeVelocity({ -1, 0 }); } // move left
			if (wnd.kbd.KeyIsPressed('X')) { snk2.SetSnakeVelocity({ 0, 1 }); } // move down
			if (wnd.kbd.KeyIsPressed('W')) { snk2.SetSnakeVelocity({ 0, -1 }); } // move up
			if (wnd.kbd.KeyIsPressed('Z')) { snk2MovePeriod *= 1.05f; }			// slower
			if (wnd.kbd.KeyIsPressed('Q')) { snk2MovePeriod /= 1.05f; }			// faster
			if (wnd.kbd.KeyIsPressed('E')) { snk2.SetSnakeVelocity({0,0}); }	// stall
			if (wnd.kbd.KeyIsPressed('S')) { snk2.JumpOn(); }

		}


		if (!gameOver)
		{
			snk1MoveCounter += dt;
			if (gVar.numPlayers == 2)
			{
				snk2MoveCounter += dt;
			}

			// Move snake 1 if its timer has elapsed
			if (snk1MoveCounter >= snk1MovePeriod)
			{
				const Location new_loc1 = snk1.GetNextHeadLocation(snk1.GetSnakeVelocity(), brd);

				// Handle food and poison interactions for snake 1
				if (brd.GetCellContent(new_loc1) == Board::contentType::food)
				{
					snk1.Grow(rng);
					brd.Spawn(Board::contentType::food, rng, 1);
					brd.Spawn(Board::contentType::barrier, rng, 1);
					brd.SetCellContent(new_loc1, Board::contentType::empty);
					// Increase player 1's score
					player1Score++;
				}

				if (brd.GetCellContent(new_loc1) == Board::contentType::poison)
				{
					brd.SetCellContent(new_loc1, Board::contentType::empty);
					snk1MovePeriod /= gVar.speedupRate; // Only affects snake 1
				}

				// Collision detection for snake 1
				bool snake1Collision = false;
				if (snk1.IsInTileExceptEnd(new_loc1) && snk1.IsMoving() ||
					brd.GetCellContent(new_loc1) == Board::contentType::barrier)
				{
					snake1Collision = true;
				}

				// Check if snake 1 collides with snake 2 (if two players)
				if (gVar.numPlayers == 2 && snk2.IsInTile(new_loc1))
				{
					snake1Collision = true;
				}

				if (snake1Collision)
				{
					// Player 1 crashed, reset their score
					player1Score = 0;
					crashedPlayer = 1;
					gameOver = true;
				}

				// Move snake 1
				if (!gameOver)
				{
					snk1.MoveTo(new_loc1);
					snk1.JumpOff();
				}

				snk1MoveCounter = 0;
			}

			// Move snake 2 if its timer has elapsed (if two players)
			if (gVar.numPlayers == 2 && snk2MoveCounter >= snk2MovePeriod)
			{
				const Location new_loc2 = snk2.GetNextHeadLocation(snk2.GetSnakeVelocity(), brd);

				// Handle food and poison interactions for snake 2
				if (brd.GetCellContent(new_loc2) == Board::contentType::food)
				{
					snk2.Grow(rng);
					brd.Spawn(Board::contentType::food, rng, 1);
					brd.Spawn(Board::contentType::barrier, rng, 1);
					brd.SetCellContent(new_loc2, Board::contentType::empty);
					// Increase player 2's score
					player2Score++;
				}

				if (brd.GetCellContent(new_loc2) == Board::contentType::poison)
				{
					brd.SetCellContent(new_loc2, Board::contentType::empty);
					snk2MovePeriod /= gVar.speedupRate; // Only affects snake 2
				}

				// Collision detection for snake 2
				bool snake2Collision = false;
				if (snk2.IsInTileExceptEnd(new_loc2) && snk2.IsMoving() ||
					brd.GetCellContent(new_loc2) == Board::contentType::barrier)
				{
					snake2Collision = true;
				}

				// Check if snake 2 collides with snake 1
				if (snk1.IsInTile(new_loc2))
				{
					snake2Collision = true;
				}

				if (snake2Collision)
				{
					// Player 2 crashed, reset their score
					player2Score = 0;
					if (crashedPlayer == 1)
					{
						// Both players crashed
						crashedPlayer = 3;
					}
					else
					{
						crashedPlayer = 2;
					}
					gameOver = true;
				}

				// Move snake 2
				if (!gameOver)
				{
					snk2.MoveTo(new_loc2);
					snk2.JumpOff();
				}

				snk2MoveCounter = 0;
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
			// Reset speeds to initial values
			// Only reset speed for players that crashed
			if (crashedPlayer == 1 || crashedPlayer == 3) // Player 1 crashed or both crashed
			{
				snk1MovePeriod = gVar.initialSpeed;
				snk1.Reset();
				
			}
			if (crashedPlayer == 2 || crashedPlayer == 3) // Player 2 crashed or both crashed
			{
				snk2MovePeriod = gVar.initialSpeed;
				snk2.Reset();
			}
			crashedPlayer = 0;
		}
	}
}

void Game::ComposeFrame()
{
	if (isStarted)
	{
		brd.DrawBorders();
		brd.DrawCellContents();
		snk1.Draw(brd); // Draw first snake
		if (gVar.numPlayers == 2)
		{
			snk2.Draw(brd); // Draw second snake
		}
		// Draw score displays
		// Player 1 score (top-right)
		SpriteCodex::DrawNumber(player1Score, gfx.ScreenWidth - 50, 10, gfx);

		// Player 2 score (top-left) - only in two player mode
		if (gVar.numPlayers == 2)
		{
			SpriteCodex::DrawNumber(player2Score, 10, 10, gfx);
		}
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