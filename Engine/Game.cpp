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
	rng(std::random_device()()),
	snk1({ 0,0 }, gVar.initialSnakelength, rng, Colors::Magenta),
	snk2({ gVar.boardSizeX - 1, gVar.boardSizeY - 1 }, gVar.initialSnakelength, rng),
	brd(gfx, snk1, snk2, gVar),
	xDistr(0, brd.GetWidth() - 1),
	yDistr(0, brd.GetHeight() - 1)
{
	snk1MovePeriod = gVar.initialSpeed;
	snk2MovePeriod = gVar.initialSpeed;
	brd.Spawn(Board::contentType::food, rng, std::max(1, gVar.foodAmount));
	brd.Spawn(Board::contentType::poison, rng, gVar.poisonAmount);
	brd.Spawn(Board::contentType::barrier, rng, 0);

	// Store original numPlayers setting from data.txt
	originalNumPlayers = gVar.numPlayers;

	// Setup networking callbacks (but don't start networking yet)
	// Networking will only start if user explicitly opts in by pressing 'N' key on title screen
	networkMgr.SetOnPeerDetected([this]() {
		networkPeerDetected = true;
		networkPromptShown = false; // Will show prompt in UpdateModel
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

	// NOTE: Networking is NOT started automatically
	// User must press 'N' key on title screen to start network discovery
	// This prevents the thread creation crash on startup
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
	
	// ===== NETWORKING STATE MACHINE =====
	if (!isStarted)  // Only handle networking on title screen
	{
		switch (networkState)
		{
		case NetworkState::Disabled:
			// Allow user to start networking
			if (wnd.kbd.KeyIsPressed('N'))
			{
				networkState = NetworkState::Starting;
				networkSearchTimer = 0.0f;
				networkError.clear();
				
				bool success = networkMgr.StartDiscovery();
				if (success)
				{
					networkState = NetworkState::Searching;
				}
				else
				{
					networkState = NetworkState::Failed;
					networkError = "Failed to start network";
				}
			}
			break;
			
		case NetworkState::Searching:
			// Increment search timer
			networkSearchTimer += dt;
			
			// Allow cancel with ESC key
			if (wnd.kbd.KeyIsPressed(VK_ESCAPE))
			{
				networkMgr.Stop();
				networkState = NetworkState::Disabled;
				networkPeerDetected = false;
				networkPromptShown = false;
			}
			
			// Check for timeout
			if (networkSearchTimer > networkSearchTimeout)
			{
				networkMgr.Stop();
				networkState = NetworkState::Failed;
				networkError = "Timeout: No players found";
			}
			
			// Check if peer detected
			if (networkPeerDetected && !networkPromptShown)
			{
				networkState = NetworkState::PeerFound;
				networkPromptShown = true;
			}
			break;
			
		case NetworkState::PeerFound:
			// User must accept or decline
			if (wnd.kbd.KeyIsPressed('Y'))
			{
				networkState = NetworkState::Connecting;
				networkMgr.AcceptConnection();
				networkPeerDetected = false;
			}
			else if (wnd.kbd.KeyIsPressed('O'))
			{
				// Decline and stop networking entirely
				networkMgr.DeclineConnection();
				networkMgr.Stop();
				networkState = NetworkState::Disabled;
				networkPeerDetected = false;
				networkPromptShown = false;
			}
			break;
			
		case NetworkState::Connecting:
			// Wait for onConnected callback to change networkingEnabled
			if (networkingEnabled)
			{
				networkState = NetworkState::Connected;
				lastPingTime = 0.0f;
				missedPings = 0;
			}
			break;
			
		case NetworkState::Connected:
			// Ready to play! Waiting for user to press ENTER
			break;
			
		case NetworkState::Failed:
			// Show error, allow return with ENTER
			if (wnd.kbd.KeyIsPressed(VK_RETURN))
			{
				networkState = NetworkState::Disabled;
				networkError.clear();
			}
			break;
		}
	}
	else  // Game is running
	{
		// Monitor connection health during gameplay
		if (networkState == NetworkState::Connected && networkingEnabled)
		{
			lastPingTime += dt;
			if (lastPingTime > pingInterval)
			{
				// Check if we've been receiving data
				// This is a simple check - could be enhanced with actual ping messages
				lastPingTime = 0.0f;
			}
		}
		
		// Handle disconnection during game
		if (networkState == NetworkState::Connected && !networkingEnabled)
		{
			// Connection was lost
			networkState = NetworkState::Failed;
			networkError = "Connection lost";
			gameOver = true;
		}
	}
	
	// Update networking (only if connected)
	if (networkState == NetworkState::Connected && networkingEnabled)
	{
		UpdateNetworking();
	}

	if (isStarted)
	{
		// In network mode, only host processes game logic
		// Client only sends input and receives state updates
		bool shouldProcessGameLogic = !networkingEnabled || isNetworkHost;

		//Control direction of snake 1
		if (wnd.kbd.KeyIsPressed(0x66))	{ snk1.SetSnakeVelocity( { 1, 0} );}	// move right (numpad 6)
		if (wnd.kbd.KeyIsPressed(0x64))	{ snk1.SetSnakeVelocity( {-1, 0} );}	// move left (numpad 4)
		if (wnd.kbd.KeyIsPressed(0x62))	{ snk1.SetSnakeVelocity( { 0, 1} );}	// move down (numpad 2)
		if (wnd.kbd.KeyIsPressed(0x68))	{ snk1.SetSnakeVelocity( { 0,-1} );}	// move up (numpad 8)
		//Adjust speed 
		if (wnd.kbd.KeyIsPressed(0x61)) { snk1MovePeriod *= 1.05f; }			// slower (numpad 7)
		if (wnd.kbd.KeyIsPressed(0x67)) { snk1MovePeriod /= 1.05f; }			// faster (numpad 1)
		if (wnd.kbd.KeyIsPressed(0x69)) { snk1.SetSnakeVelocity( { 0,0 } );}	// stall (numpad 9)
		//Jump
		if (wnd.kbd.KeyIsPressed(0x65)) { snk1.JumpOn();}						// jump (numpad 5)
		
		// Control second snake
		if (gVar.numPlayers == 2)
		{
			// If networked, client controls snake 2 locally, host receives input
			bool isLocalSnake2 = !networkingEnabled || !isNetworkHost;
			
			if (isLocalSnake2)
			{
				if (wnd.kbd.KeyIsPressed('D')) { snk2.SetSnakeVelocity({ 1, 0 }); }  // move right
				if (wnd.kbd.KeyIsPressed('A')) { snk2.SetSnakeVelocity({ -1, 0 }); } // move left
				if (wnd.kbd.KeyIsPressed('X')) { snk2.SetSnakeVelocity({ 0, 1 }); }  // move down
				if (wnd.kbd.KeyIsPressed('W')) { snk2.SetSnakeVelocity({ 0, -1 }); } // move up
				if (wnd.kbd.KeyIsPressed('Z')) { snk2MovePeriod *= 1.05f; }          // slower
				if (wnd.kbd.KeyIsPressed('Q')) { snk2MovePeriod /= 1.05f; }          // faster
				if (wnd.kbd.KeyIsPressed('E')) { snk2.SetSnakeVelocity({0,0}); }     // stall
				if (wnd.kbd.KeyIsPressed('S')) { snk2.JumpOn(); }                    // jump
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
					player1Score++;
				}

				if (brd.GetCellContent(new_loc1) == Board::contentType::poison)
				{
					brd.SetCellContent(new_loc1, Board::contentType::empty);
					snk1MovePeriod /= gVar.speedupRate;
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
					player2Score++;
				}

				if (brd.GetCellContent(new_loc2) == Board::contentType::poison)
				{
					brd.SetCellContent(new_loc2, Board::contentType::empty);
					snk2MovePeriod /= gVar.speedupRate;
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
					player2Score = 0;
					if (crashedPlayer == 1)
					{
						crashedPlayer = 3; // Both crashed
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
	
	// Handle game (re)start
	if (wnd.kbd.KeyIsPressed(VK_RETURN))
	{
		isStarted = true;
		gameOver = false;
		
		// Reset speeds to initial values
		// Only reset speed for players that crashed
		if (crashedPlayer == 1 || crashedPlayer == 3)
		{
			snk1MovePeriod = gVar.initialSpeed;
			snk1.Reset();
		}
		if (crashedPlayer == 2 || crashedPlayer == 3)
		{
			snk2MovePeriod = gVar.initialSpeed;
			snk2.Reset();
		}
		crashedPlayer = 0;
	}
}

void Game::ComposeFrame()
{
	if (isStarted)
	{
		brd.DrawBorders();
		brd.DrawCellContents();
		snk1.Draw(brd);
		if (gVar.numPlayers == 2)
		{
			snk2.Draw(brd);
		}
		
		// Draw score displays
		SpriteCodex::DrawNumber(player1Score, gfx.ScreenWidth - 50, 10, gfx);
		
		if (gVar.numPlayers == 2)
		{
			SpriteCodex::DrawNumber(player2Score, 10, 10, gfx);
		}
		
		// Show connection status if networked
		if (networkingEnabled && networkState == NetworkState::Connected)
		{
			std::string statusText = isNetworkHost ? "HOST" : "CLIENT";
			SpriteCodex::DrawString(statusText, gfx.ScreenWidth - 100, gfx.ScreenHeight - 20, Colors::Cyan, gfx);
		}
	}
	else  // Title screen
	{
		SpriteCodex::DrawTitle(100, 100, gfx);
		
		// ===== NETWORK STATE UI =====
		switch (networkState)
		{
		case NetworkState::Disabled:
			SpriteCodex::DrawString("PRESS ENTER TO START", 280, 250, Colors::White, gfx);
			
			// Show player controls based on data.txt setting
			if (gVar.numPlayers == 1)
			{
				SpriteCodex::DrawString("PLAYER 1 CONTROLS:", 50, 300, Colors::Magenta, gfx);
				SpriteCodex::DrawString("NUMPAD 8246 - MOVE", 50, 315, Colors::White, gfx);
				SpriteCodex::DrawString("NUMPAD 5 - JUMP", 50, 330, Colors::White, gfx);
			}
			else if (gVar.numPlayers == 2)
			{
				SpriteCodex::DrawString("PLAYER 1:", 50, 300, Colors::Magenta, gfx);
				SpriteCodex::DrawString("NUMPAD 8246 - MOVE", 50, 315, Colors::White, gfx);
				SpriteCodex::DrawString("NUMPAD 5 - JUMP", 50, 330, Colors::White, gfx);
				
				SpriteCodex::DrawString("PLAYER 2:", 450, 300, Colors::Green, gfx);
				SpriteCodex::DrawString("WASD - MOVE", 450, 315, Colors::White, gfx);
				SpriteCodex::DrawString("S - JUMP", 450, 330, Colors::White, gfx);
			}
			
			SpriteCodex::DrawString("PRESS N FOR NETWORK MODE", 240, 380, Colors::Cyan, gfx);
			break;
			
		case NetworkState::Searching:
			{
				// Show search progress with countdown
				int remainingTime = static_cast<int>(networkSearchTimeout - networkSearchTimer);
				std::string searchText = "SEARCHING FOR PLAYERS... " + 
				                        std::to_string(remainingTime) + "s";
				SpriteCodex::DrawString(searchText, 220, 250, Colors::Green, gfx);
				
				// Animated dots
				int dots = (static_cast<int>(networkSearchTimer * 2) % 4);
				std::string dotStr(dots, '.');
				SpriteCodex::DrawString(dotStr, 540, 250, Colors::Green, gfx);
				
				// Show local IP
				std::string localIP = "YOUR IP: " + networkMgr.GetLocalAddress();
				SpriteCodex::DrawString(localIP, 260, 290, Colors::Cyan, gfx);
				
				// Cancel instructions
				SpriteCodex::DrawString("PRESS ESC TO CANCEL", 260, 380, Colors::Gray, gfx);
			}
			break;
			
		case NetworkState::PeerFound:
			// User must accept or decline
			if (!networkPromptShown)
			{
				SpriteCodex::DrawString("PEER FOUND! PRESS Y TO CONNECT", 180, 250, Colors::Yellow, gfx);
				SpriteCodex::DrawString("PRESS O TO DECLINE", 240, 290, Colors::Yellow, gfx);
				
				networkPromptShown = true;
			}
			break;
			
		case NetworkState::Connecting:
			SpriteCodex::DrawString("CONNECTING...", 260, 250, Colors::Yellow, gfx);
			break;
			
		case NetworkState::Connected:
			// Nothing extra to show on UI for now
			break;
			
		case NetworkState::Failed:
			SpriteCodex::DrawString("NETWORK ERROR:", 240, 250, Colors::Red, gfx);
			SpriteCodex::DrawString(networkError, 240, 290, Colors::Red, gfx);
			break;
		}
	}

	if (gameOver)
	{
		SpriteCodex::DrawGameOver(200, 200, gfx);
		
		// Show disconnect message if connection was lost
		if (networkState == NetworkState::Failed && !networkError.empty())
		{
			SpriteCodex::DrawString(networkError, 240, 350, Colors::Red, gfx);
		}
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

	// Board state
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

	// Serialize head position
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