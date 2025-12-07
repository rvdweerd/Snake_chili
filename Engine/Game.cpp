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
#include "NetworkManager.h"

Game::Game(MainWindow& wnd)
	:
	wnd(wnd),
	gfx(wnd),
	//gVar(std::string("data.txt")),
	rng(std::random_device()()),
	snk1({ 0,0 }, gVar.initialSnakelength, rng, Colors::Magenta),
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

	// Store original numPlayers setting from data.txt
	originalNumPlayers = gVar.numPlayers;

	// Setup networking callbacks
	networkMgr.SetOnPeerDetected([this]() {
		// Peer detected - if user wanted single player, prompt them
		if (originalNumPlayers == 1)
		{
			networkPeerDetected = true;
			networkPromptShown = false; // Will show prompt in UpdateModel
		}
		else
		{
			// User wanted multiplayer, auto-accept
			networkMgr.AcceptConnection();
		}
	});

	networkMgr.SetOnConnected([this]() {
		// Connection established
		isNetworkHost = (networkMgr.GetRole() == NetworkRole::Host);
		networkingEnabled = true;
		userWantsNetworking = true;
		gVar.numPlayers = 2; // Switch to 2 player mode
	});

	networkMgr.SetOnInputReceived([this](const InputMessage& msg) {
		// Received remote player input
		ApplyRemoteInput(msg);
	});

	networkMgr.SetOnGameStateReceived([this](const GameStateSnapshot& state) {
		// Received game state update (client only)
		if (!isNetworkHost)
		{
			ApplyGameStateSnapshot(state);
		}
	});

	networkMgr.SetOnDisconnected([this]() {
		// Connection lost
		networkingEnabled = false;
		// Restore original setting
		gVar.numPlayers = originalNumPlayers;
	});

	// Start network discovery (always start to detect peers)
	networkMgr.StartDiscovery();
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
	
	// Handle network peer detection prompt (before game starts)
	if (networkPeerDetected && !networkPromptShown && !isStarted)
	{
		networkPromptShown = true;
		// Prompt is displayed in ComposeFrame
		// User presses Y to accept, N to decline
	}

	if (networkPeerDetected && networkPromptShown && !isStarted)
	{
		// Check for user input
		if (wnd.kbd.KeyIsPressed('Y'))
		{
			// User accepts multiplayer
			networkMgr.AcceptConnection();
			networkPeerDetected = false;
		}
		else if (wnd.kbd.KeyIsPressed('N'))
		{
			// User declines multiplayer
			networkPeerDetected = false;
			networkPromptShown = false;
			// Continue in single player mode
		}
	}
	
	// Update networking
	UpdateNetworking();

	if (isStarted)
	{
		// In network mode, only host processes game logic
		// Client only sends input and receives state updates
		bool shouldProcessGameLogic = !networkingEnabled || isNetworkHost;

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
		
		// Control second snake
		if (gVar.numPlayers == 2)
		{
			// If networked, client controls snake 2 locally, host receives input
			bool isLocalSnake2 = !networkingEnabled || !isNetworkHost;
			
			if (isLocalSnake2)
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
		}

		// Game simulation (only run on host in network mode)
		if (shouldProcessGameLogic && !gameOver)
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
		
		// Display network peer detection prompt
		if (networkPeerDetected && networkPromptShown)
		{
			// Draw semi-transparent overlay
			gfx.DrawRect(150, 200, 650, 400, Color(50, 50, 50));
			
			// Draw border
			gfx.DrawRect(150, 200, 650, 205, Colors::Yellow);  // Top
			gfx.DrawRect(150, 395, 650, 400, Colors::Yellow);  // Bottom
			gfx.DrawRect(150, 200, 155, 400, Colors::Yellow);  // Left
			gfx.DrawRect(645, 200, 650, 400, Colors::Yellow);  // Right
			
			// Draw simple text indicators using colored blocks
			// "NETWORK PLAYER FOUND" indicator (yellow bars)
			for (int i = 0; i < 15; i++)
			{
				gfx.DrawRect(180 + i*30, 220, 180 + i*30 + 25, 240, Colors::Yellow);
			}
			
			// "Y = ACCEPT" indicator (green blocks)
			gfx.DrawRect(200, 280, 230, 310, Colors::Green);    // Y shape
			gfx.DrawRect(215, 295, 230, 310, Colors::Green);
			gfx.DrawRect(200, 295, 215, 310, Colors::Green);
			gfx.DrawRect(350, 280, 550, 310, Colors::Green);    // ACCEPT bar
			
			// "N = DECLINE" indicator (red blocks)
			gfx.DrawRect(200, 330, 230, 360, Colors::Red);       // N shape
			gfx.DrawRect(350, 330, 550, 360, Colors::Red);       // DECLINE bar
		}
	}
	if (gameOver)
	{
		SpriteCodex::DrawGameOver(200, 200, gfx);
	}
}

void Game::UpdateNetworking()
{
	if (!networkingEnabled)
	{
		return;
	}

	if (isNetworkHost)
	{
		// Host sends full game state periodically
		networkSyncCounter += frmTime.Mark();
		if (networkSyncCounter >= networkSyncPeriod)
		{
			GameStateSnapshot state = CreateGameStateSnapshot();
			networkMgr.SendGameState(state);
			networkSyncCounter = 0.0f;
		}
	}
	else
	{
		// Client sends input when it changes
		Location currentVel = snk2.GetSnakeVelocity();
		if (currentVel.x != lastSentVelocity2.x || currentVel.y != lastSentVelocity2.y)
		{
			networkMgr.SendInput(currentVel.x, currentVel.y, false, snk2MovePeriod);
			lastSentVelocity2 = currentVel;
		}
	}
}

void Game::ApplyRemoteInput(const InputMessage& msg)
{
	// Host receives client input and applies it to snake 2
	if (isNetworkHost)
	{
		snk2.SetSnakeVelocity({ msg.vx, msg.vy });
		snk2MovePeriod = msg.movePeriod;
	}
}

void Game::ApplyGameStateSnapshot(const GameStateSnapshot& state)
{
	// Client applies received game state
	player1Score = state.player1Score;
	player2Score = state.player2Score;
	gameOver = state.gameOver != 0;
	crashedPlayer = state.crashedPlayer;

	// Update snake 1
	DeserializeSnake(snk1, state.snake1Segments, state.snake1SegmentCount, 
					 state.snake1VelocityX, state.snake1VelocityY);
	snk1MovePeriod = state.snake1MovePeriod;

	// Update snake 2
	DeserializeSnake(snk2, state.snake2Segments, state.snake2SegmentCount,
					 state.snake2VelocityX, state.snake2VelocityY);
	snk2MovePeriod = state.snake2MovePeriod;

	// Update board contents (food, poison, barriers)
	// Clear existing items first would require Board API changes
	// For now, the host controls spawning and we rely on state sync
}

GameStateSnapshot Game::CreateGameStateSnapshot()
{
	GameStateSnapshot state{};
	state.magic = 0; // Set by NetworkManager
	state.sequence = 0; // Set by NetworkManager

	// Serialize snakes
	SerializeSnake(snk1, state.snake1Segments, state.snake1SegmentCount, 
				   state.snake1VelocityX, state.snake1VelocityY);
	state.snake1MovePeriod = snk1MovePeriod;

	SerializeSnake(snk2, state.snake2Segments, state.snake2SegmentCount,
				   state.snake2VelocityX, state.snake2VelocityY);
	state.snake2MovePeriod = snk2MovePeriod;

	// Game state
	state.player1Score = player1Score;
	state.player2Score = player2Score;
	state.gameOver = gameOver ? 1 : 0;
	state.crashedPlayer = crashedPlayer;

	// Board state - for simplicity, we'll skip serializing all board contents
	// The host controls spawning, so clients will see updates through snake interactions
	state.foodCount = 0;
	state.poisonCount = 0;
	state.barrierCount = 0;

	return state;
}

void Game::SerializeSnake(const Snake& snake, SnakeSegment* segments, uint16_t& count, int8_t& vx, int8_t& vy)
{
	Location vel = snake.GetSnakeVelocity();
	vx = vel.x;
	vy = vel.y;

	// We need access to snake segments - this requires Snake API changes
	// For now, we'll serialize just the head position
	Location head = snake.GetCurrentHeadLocation();
	count = 1;
	segments[0].x = head.x;
	segments[0].y = head.y;
}

void Game::DeserializeSnake(Snake& snake, const SnakeSegment* segments, uint16_t count, int8_t vx, int8_t vy)
{
	if (count > 0)
	{
		Location newHead = { segments[0].x, segments[0].y };
		snake.MoveTo(newHead);
	}
	snake.SetSnakeVelocity({ vx, vy });
}