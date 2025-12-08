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

	networkMgr.SetOnGameStartReceived([this]() {
		// Received game start command from host (client only)
		if (!isNetworkHost)
		{
			isStarted = true;
			gameOver = false;
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
				networkMgr.AcceptConnection();
				// AcceptConnection calls onConnected callback synchronously,
				// which sets networkingEnabled = true
				networkPeerDetected = false;
				
				// Transition directly to Connected since callback is synchronous
				if (networkingEnabled)
				{
					networkState = NetworkState::Connected;
					lastPingTime = 0.0f;
					missedPings = 0;
				}
				else
				{
					networkState = NetworkState::Connecting;
				}
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
			// The AcceptConnection call will trigger onConnected callback immediately
			// which sets networkingEnabled = true. Check that here.
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

		// ===== PLAYER 1 CONTROLS (HOST in network mode) =====
		// In network mode: Only HOST controls snake 1
		// In local mode: Local player controls snake 1
		bool shouldControlSnake1 = !networkingEnabled || isNetworkHost;
		
		if (shouldControlSnake1)
		{
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
		}
		
		// ===== PLAYER 2 CONTROLS (CLIENT in network mode) =====
		// Control second snake
		if (gVar.numPlayers == 2)
		{
			// In network mode: CLIENT controls snake 2, HOST receives input from network
			// In local mode: Local player 2 controls snake 2
			bool shouldControlSnake2 = !networkingEnabled || !isNetworkHost;
			
			if (shouldControlSnake2)
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
		// In network mode, only HOST can start the game
		if (!networkingEnabled || isNetworkHost)
		{
			isStarted = true;
			gameOver = false;
			
			// If host in network mode, notify client to start too
			if (networkingEnabled && isNetworkHost)
			{
				networkMgr.SendStartCommand();
			}
			
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
}

void Game::ComposeFrame()
{
	if (isStarted)
	{
		// DEBUG: Log occasionally to verify drawing
		static int frameCount = 0;
		if (++frameCount % 300 == 0) // Every ~5 seconds at 60 FPS
		{
			std::string debugMsg = "ComposeFrame: Drawing - Snake1 has " + std::to_string(snk1.GetSegmentCount()) + 
			                       " segments, Snake2 has " + std::to_string(snk2.GetSegmentCount()) + " segments\n";
			OutputDebugStringA(debugMsg.c_str());
			
			if (networkingEnabled)
			{
				std::string netMsg = "  Network mode: " + std::string(isNetworkHost ? "HOST" : "CLIENT") + "\n";
				OutputDebugStringA(netMsg.c_str());
			}
		}
		
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
			
			// DEBUG: Show which controls are active
			bool shouldControlSnake1 = !networkingEnabled || isNetworkHost;
			bool shouldControlSnake2 = !networkingEnabled || !isNetworkHost;
			
			std::string debugInfo1 = shouldControlSnake1 ? "P1:LOCAL" : "P1:NETWORK";
			std::string debugInfo2 = shouldControlSnake2 ? "P2:LOCAL" : "P2:NETWORK";
			
			SpriteCodex::DrawString(debugInfo1, 10, gfx.ScreenHeight - 40, Colors::Yellow, gfx);
			SpriteCodex::DrawString(debugInfo2, 10, gfx.ScreenHeight - 20, Colors::Yellow, gfx);
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
			{
				// Draw modal dialog
				gfx.DrawRect(150, 180, 650, 420, Color(50, 50, 50));
				gfx.DrawRect(150, 180, 650, 185, Colors::Yellow);   // Top border
				gfx.DrawRect(150, 415, 650, 420, Colors::Yellow);   // Bottom border
				gfx.DrawRect(150, 180, 155, 420, Colors::Yellow);   // Left border
				gfx.DrawRect(645, 180, 650, 420, Colors::Yellow);   // Right border
				
				// Title
				SpriteCodex::DrawString("NETWORK PLAYER FOUND!", 220, 200, Colors::Yellow, gfx);
				
				// IP addresses with better formatting
				std::string localIP = "YOUR IP:  " + networkMgr.GetLocalAddress();
				std::string peerIP = "PEER IP:  " + networkMgr.GetPeerAddress();
				SpriteCodex::DrawString(localIP, 200, 240, Colors::Cyan, gfx);
				SpriteCodex::DrawString(peerIP, 200, 260, Colors::Green, gfx);
				
				// Role information
				std::string role = (networkMgr.GetRole() == NetworkRole::Host) 
				    ? "YOU WILL BE: HOST (PLAYER 1)" 
				    : "YOU WILL BE: CLIENT (PLAYER 2)";
				SpriteCodex::DrawString(role, 200, 300, Colors::White, gfx);
				
				// Action buttons
				SpriteCodex::DrawString("PRESS Y TO ACCEPT", 250, 350, Colors::Green, gfx);
				SpriteCodex::DrawString("PRESS O TO DECLINE", 245, 380, Colors::Red, gfx);
			}
			break;
			
		case NetworkState::Connecting:
			// Intentional blank - UI handled in Connected state
			break;
			
		case NetworkState::Connected:
			SpriteCodex::DrawString("CONNECTED!", 310, 230, Colors::Green, gfx);
			
			// Show different instructions for host vs client
			if (isNetworkHost)
			{
				SpriteCodex::DrawString("PRESS ENTER TO START", 260, 270, Colors::White, gfx);
			}
			else
			{
				SpriteCodex::DrawString("WAITING FOR HOST TO START...", 220, 270, Colors::Yellow, gfx);
			}
			
			// Show who you're playing with
			{
				std::string opponent = "OPPONENT: " + networkMgr.GetPeerAddress();
				SpriteCodex::DrawString(opponent, 250, 310, Colors::Cyan, gfx);
				
				std::string yourRole = (isNetworkHost) ? "YOU ARE: PLAYER 1" : "YOU ARE: PLAYER 2";
				SpriteCodex::DrawString(yourRole, 270, 340, Colors::Magenta, gfx);
			}
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

	// Both host and client send heartbeats periodically to keep connection alive
	static float heartbeatTimer = 0.0f;
	heartbeatTimer += frmTime.Mark();
	constexpr float heartbeatInterval = 2.0f; // Send heartbeat every 2 seconds
	
	if (heartbeatTimer >= heartbeatInterval)
	{
		networkMgr.SendHeartbeat();
		heartbeatTimer = 0.0f;
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
			
			// DEBUG: Output every second
			static int sendCount = 0;
			if (++sendCount % 20 == 0) // Every 20 sends (~1 second at 20Hz)
			{
				OutputDebugStringA("HOST: Sending game state update\n");
			}
		}
	}
	else
	{
		// CLIENT: Send snake 2 input to host whenever it changes
		Location currentVel = snk2.GetSnakeVelocity();
		if (currentVel.x != lastSentVelocity2.x || currentVel.y != lastSentVelocity2.y)
		{
			networkMgr.SendInput(currentVel.x, currentVel.y, false, snk2MovePeriod);
			lastSentVelocity2 = currentVel;
		}
		
		// Also send periodically to keep connection alive (input also serves as heartbeat)
		static float inputHeartbeatTimer = 0.0f;
		inputHeartbeatTimer += frmTime.Mark();
		if (inputHeartbeatTimer > 0.5f) // Send input heartbeat every 500ms
		{
			networkMgr.SendInput(currentVel.x, currentVel.y, false, snk2MovePeriod);
			inputHeartbeatTimer = 0.0f;
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
	// DEBUG: Log EVERY update to diagnose the issue
	static int updateCount = 0;
	updateCount++;
	
	if (updateCount == 1)
	{
		OutputDebugStringA("ApplyGameStateSnapshot: FIRST CALL - callback is working!\n");
	}
	
	if (updateCount % 20 == 0) // Every 20 updates (~1 second at 20Hz)
	{
		std::string debugMsg = "ApplyGameStateSnapshot: Update #" + std::to_string(updateCount) + 
		                       " - Snake1: " + std::to_string(state.snake1SegmentCount) + " segments, " +
		                       "Snake2: " + std::to_string(state.snake2SegmentCount) + " segments\n";
		OutputDebugStringA(debugMsg.c_str());
		
		// Log first segment position for snake 1
		if (state.snake1SegmentCount > 0)
		{
			std::string posMsg = "  Snake1 head at: (" + std::to_string(state.snake1Segments[0].x) + ", " + 
			                     std::to_string(state.snake1Segments[0].y) + ")\n";
			OutputDebugStringA(posMsg.c_str());
		}
	}

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
	
	// Verify snakes were updated
	if (updateCount % 20 == 0)
	{
		std::string verifyMsg = "  After deserialize: Snake1 has " + std::to_string(snk1.GetSegmentCount()) + 
		                        " segments, Snake2 has " + std::to_string(snk2.GetSegmentCount()) + " segments\n";
		OutputDebugStringA(verifyMsg.c_str());
	}
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

	// Serialize ALL segments (up to max allowed)
	int segmentCount = snake.GetSegmentCount();
	count = static_cast<uint16_t>(std::min(segmentCount, 500)); // Cap at max array size
	
	for (int i = 0; i < count; i++)
	{
		Location loc = snake.GetSegmentLocation(i);
		segments[i].x = static_cast<int16_t>(loc.x);
		segments[i].y = static_cast<int16_t>(loc.y);
	}
}

void Game::DeserializeSnake(Snake& snake, const SnakeSegment* segments, uint16_t count, int8_t vx, int8_t vy)
{
	static int deserializeCount = 0;
	if (++deserializeCount % 40 == 0) // Log occasionally
	{
		std::string debugMsg = "DeserializeSnake: count=" + std::to_string(count) + 
		                       ", velocity=(" + std::to_string(vx) + "," + std::to_string(vy) + ")\n";
		OutputDebugStringA(debugMsg.c_str());
	}
	
	if (count > 0)
	{
		// Convert SnakeSegments to Locations
		std::vector<Location> locations(count);
		for (uint16_t i = 0; i < count; i++)
		{
			locations[i] = { static_cast<int>(segments[i].x), static_cast<int>(segments[i].y) };
		}
		
		// Apply all segments to the snake
		snake.SetSegments(locations.data(), count);
		
		if (deserializeCount % 40 == 0)
		{
			std::string resultMsg = "  SetSegments called with " + std::to_string(count) + " locations\n";
			OutputDebugStringA(resultMsg.c_str());
		}
	}
	else
	{
		OutputDebugStringA("DeserializeSnake: WARNING - count is 0!\n");
	}
	
	// Update velocity
	snake.SetSnakeVelocity({ vx, vy });
}