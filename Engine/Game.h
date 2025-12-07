/****************************************************************************************** 
 *	Chili DirectX Framework Version 16.07.20											  *	
 *	Game.h																				  *
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
#pragma once

#include "Keyboard.h"
#include "Mouse.h"
#include "Graphics.h"
#include "Board.h"
#include <random>
#include "Snake.h"
#include "FrameTimer.h"
#include "GameVariables.h"
#include "NetworkManager.h"

class Game
{
public:
	Game( class MainWindow& wnd );
	Game( const Game& ) = delete;
	Game& operator=( const Game& ) = delete;
	void Go();
private:
	void ComposeFrame();
	void UpdateModel();
	/********************************/
	/*  User Functions              */
	void UpdateNetworking();
	void ApplyRemoteInput(const InputMessage& msg);
	void ApplyGameStateSnapshot(const GameStateSnapshot& state);
	GameStateSnapshot CreateGameStateSnapshot();
	void SerializeSnake(const Snake& snake, SnakeSegment* segments, uint16_t& count, int8_t& vx, int8_t& vy);
	void DeserializeSnake(Snake& snake, const SnakeSegment* segments, uint16_t count, int8_t vx, int8_t vy);
	/********************************/
private:
	MainWindow& wnd;//init
	Graphics gfx;//init
	/********************************/
	/*  User Variables              */
	GameVariables gVar = std::string("data.txt");
	std::mt19937 rng;
	Snake snk1;
	Snake snk2;
	Board brd;

	FrameTimer frmTime;
	bool gameOver = false;
	bool isStarted = false;
	int snkAcceleration = 0;
	float snk1MovePeriod; // timestep in seconds for snake 1
	float snk2MovePeriod; // timestep in seconds for snake 2
	float snk1MoveCounter = 0.0f;
	float snk2MoveCounter = 0.0f;
	std::uniform_int_distribution<int> xDistr;
	std::uniform_int_distribution<int> yDistr;

	// Score variables
	int player1Score = 0;
	int player2Score = 0;
	int crashedPlayer = 0; // 0 = no crash, 1 = player1 crashed, 2 = player2 crashed, 3 = both crashed

	// Networking
	NetworkManager networkMgr;
	bool networkingEnabled = false;
	bool isNetworkHost = false;
	float networkSyncCounter = 0.0f;
	static constexpr float networkSyncPeriod = 0.05f; // 20Hz sync rate
	Location lastSentVelocity1 = {0, 0};
	Location lastSentVelocity2 = {0, 0};
	
	// Network prompt state
	bool networkPeerDetected = false;
	bool networkPromptShown = false;
	bool userWantsNetworking = false;
	int originalNumPlayers = 1; // Store original setting from data.txt
	/********************************/
	//std::random_device rd;
	//std::mt19937 rng;
	//std::uniform_int_distribution<int> cDistr;

};