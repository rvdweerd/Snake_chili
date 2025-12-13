#include "NetworkManager.h"
#include <chrono>
#include <iostream>

struct NetworkManager::Impl
{
	SOCKET discoverySock = INVALID_SOCKET;
	SOCKET gameSock = INVALID_SOCKET;
	
	sockaddr_in peerAddr{};
	sockaddr_in pendingPeerAddr{};
	std::atomic<bool> hasPeer{false};
	std::atomic<bool> hasPendingPeer{false};
	
	std::thread discoveryThread;
	std::thread networkThread;
	std::atomic<bool> running{false};
	std::atomic<ConnectionState> connectionState{ConnectionState::Idle};
	std::atomic<NetworkRole> role{NetworkRole::None};
	
	std::mutex callbackMutex;
	std::function<void(const InputMessage&)> onInputReceived;
	std::function<void(const GameStateSnapshot&)> onGameStateReceived;
	std::function<void(const BoardDelta&)> onBoardDeltaReceived;
	std::function<void()> onConnected;
	std::function<void()> onPeerDetected;
	std::function<void()> onDisconnected;
	std::function<void()> onGameStartReceived;
	
	uint32_t sendSequence = 0;
	uint32_t lastReceivedSequence = 0;
	
	std::chrono::steady_clock::time_point lastBroadcastTime;
	std::chrono::steady_clock::time_point lastReceiveTime;
};

NetworkManager::NetworkManager()
	: pImpl(new Impl)
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		// Error handling - in production you'd throw or return error
	}
}

NetworkManager::~NetworkManager()
{
	Stop();
	WSACleanup();
	delete pImpl;
}

bool NetworkManager::StartDiscovery()
{
	if (pImpl->running)
	{
		return false;
	}

	// Create discovery socket
	pImpl->discoverySock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (pImpl->discoverySock == INVALID_SOCKET)
	{
		return false;
	}

	// Enable broadcast
	BOOL broadcast = TRUE;
	setsockopt(pImpl->discoverySock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
	
	// Set non-blocking
	u_long mode = 1;
	ioctlsocket(pImpl->discoverySock, FIONBIO, &mode);

	// Bind to discovery port
	sockaddr_in localAddr{};
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	localAddr.sin_port = htons(DISCOVERY_PORT);
	
	if (bind(pImpl->discoverySock, (sockaddr*)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
	{
		closesocket(pImpl->discoverySock);
		pImpl->discoverySock = INVALID_SOCKET;
		return false;
	}

	// Create game socket
	pImpl->gameSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (pImpl->gameSock == INVALID_SOCKET)
	{
		closesocket(pImpl->discoverySock);
		pImpl->discoverySock = INVALID_SOCKET;
		return false;
	}

	mode = 1;
	ioctlsocket(pImpl->gameSock, FIONBIO, &mode);

	sockaddr_in gameAddr{};
	gameAddr.sin_family = AF_INET;
	gameAddr.sin_addr.s_addr = INADDR_ANY;
	gameAddr.sin_port = htons(GAME_PORT);
	
	if (bind(pImpl->gameSock, (sockaddr*)&gameAddr, sizeof(gameAddr)) == SOCKET_ERROR)
	{
		closesocket(pImpl->discoverySock);
		closesocket(pImpl->gameSock);
		pImpl->discoverySock = INVALID_SOCKET;
		pImpl->gameSock = INVALID_SOCKET;
		return false;
	}

	pImpl->running = true;
	pImpl->connectionState = ConnectionState::Discovering;
	pImpl->lastBroadcastTime = std::chrono::steady_clock::now();
	pImpl->lastReceiveTime = std::chrono::steady_clock::now();

	// Start threads with exception handling
	try {
		pImpl->discoveryThread = std::thread(&NetworkManager::DiscoveryThreadFunc, this);
		pImpl->networkThread = std::thread(&NetworkManager::NetworkThreadFunc, this);
	}
	catch (const std::system_error& e) {
		// Thread creation failed - clean up
		pImpl->running = false;
		closesocket(pImpl->discoverySock);
		closesocket(pImpl->gameSock);
		pImpl->discoverySock = INVALID_SOCKET;
		pImpl->gameSock = INVALID_SOCKET;
		return false;
	}

	return true;
}

void NetworkManager::Stop()
{
	pImpl->running = false;
	
	if (pImpl->discoveryThread.joinable())
	{
		pImpl->discoveryThread.join();
	}
	
	if (pImpl->networkThread.joinable())
	{
		pImpl->networkThread.join();
	}

	if (pImpl->discoverySock != INVALID_SOCKET)
	{
		closesocket(pImpl->discoverySock);
		pImpl->discoverySock = INVALID_SOCKET;
	}
	
	if (pImpl->gameSock != INVALID_SOCKET)
	{
		closesocket(pImpl->gameSock);
		pImpl->gameSock = INVALID_SOCKET;
	}

	pImpl->hasPeer = false;
	pImpl->connectionState = ConnectionState::Idle;
	pImpl->role = NetworkRole::None;
}

ConnectionState NetworkManager::GetConnectionState() const
{
	return pImpl->connectionState;
}

NetworkRole NetworkManager::GetRole() const
{
	return pImpl->role;
}

void NetworkManager::SendInput(int8_t vx, int8_t vy, bool jump, float movePeriod)
{
	if (!pImpl->hasPeer || pImpl->gameSock == INVALID_SOCKET)
	{
		return;
	}

	InputMessage msg{};
	msg.magic = GAME_MAGIC;
	msg.sequence = htonl(pImpl->sendSequence++);
	msg.vx = vx;
	msg.vy = vy;
	msg.jump = jump ? 1 : 0;
	msg.movePeriod = movePeriod;

	sendto(pImpl->gameSock, (char*)&msg, sizeof(msg), 0, 
		   (sockaddr*)&pImpl->peerAddr, sizeof(pImpl->peerAddr));
}

void NetworkManager::SendGameState(const GameStateSnapshot& state)
{
	if (!pImpl->hasPeer || pImpl->gameSock == INVALID_SOCKET)
	{
		return;
	}

	GameStateSnapshot netState = state;
	netState.magic = GAME_MAGIC;
	netState.sequence = htonl(pImpl->sendSequence++);
	
	// Store original counts before converting (needed for loop limits)
	uint16_t origSnake1Count = state.snake1SegmentCount;
	uint16_t origSnake2Count = state.snake2SegmentCount;
	uint16_t origFoodCount = state.foodCount;
	uint16_t origPoisonCount = state.poisonCount;
	uint16_t origBarrierCount = state.barrierCount;
	
	// Clamp counts to array bounds
	if (origSnake1Count > 500) origSnake1Count = 500;
	if (origSnake2Count > 500) origSnake2Count = 500;
	if (origFoodCount > 100) origFoodCount = 100;
	if (origPoisonCount > 100) origPoisonCount = 100;
	if (origBarrierCount > 200) origBarrierCount = 200;
	
	// Convert all multi-byte fields to network byte order
	netState.snake1SegmentCount = htons(origSnake1Count);
	netState.snake2SegmentCount = htons(origSnake2Count);
	netState.player1Score = htons(netState.player1Score);
	netState.player2Score = htons(netState.player2Score);
	netState.foodCount = htons(origFoodCount);
	netState.poisonCount = htons(origPoisonCount);
	netState.barrierCount = htons(origBarrierCount);
	
	// Convert all segment positions to network byte order
	for (int i = 0; i < origSnake1Count; i++)
	{
		netState.snake1Segments[i].x = htons(netState.snake1Segments[i].x);
		netState.snake1Segments[i].y = htons(netState.snake1Segments[i].y);
	}
	
	for (int i = 0; i < origSnake2Count; i++)
	{
		netState.snake2Segments[i].x = htons(netState.snake2Segments[i].x);
		netState.snake2Segments[i].y = htons(netState.snake2Segments[i].y);
	}
	
	// Convert food/poison/barrier locations to network byte order
	for (int i = 0; i < origFoodCount; i++)
	{
		netState.foodLocations[i].x = htons(netState.foodLocations[i].x);
		netState.foodLocations[i].y = htons(netState.foodLocations[i].y);
	}
	
	for (int i = 0; i < origPoisonCount; i++)
	{
		netState.poisonLocations[i].x = htons(netState.poisonLocations[i].x);
		netState.poisonLocations[i].y = htons(netState.poisonLocations[i].y);
	}
	
	for (int i = 0; i < origBarrierCount; i++)
	{
		netState.barrierLocations[i].x = htons(netState.barrierLocations[i].x);
		netState.barrierLocations[i].y = htons(netState.barrierLocations[i].y);
	}

	sendto(pImpl->gameSock, (char*)&netState, sizeof(netState), 0,
		   (sockaddr*)&pImpl->peerAddr, sizeof(pImpl->peerAddr));
}

void NetworkManager::SendStartCommand()
{
	if (!pImpl->hasPeer || pImpl->gameSock == INVALID_SOCKET)
	{
		return;
	}

	StartCommand cmd{};
	cmd.magic = GAME_MAGIC;
	cmd.commandType = 0; // start game

	// Send multiple times for reliability (UDP can drop packets)
	for (int i = 0; i < 3; i++)
	{
		sendto(pImpl->gameSock, (char*)&cmd, sizeof(cmd), 0,
			   (sockaddr*)&pImpl->peerAddr, sizeof(pImpl->peerAddr));
	}
	
	OutputDebugStringA("SendStartCommand: Sent START command (3x for reliability)\n");
}

void NetworkManager::SendHeartbeat()
{
	if (!pImpl->hasPeer || pImpl->gameSock == INVALID_SOCKET)
	{
		return;
	}

	Heartbeat hb{};
	hb.magic = GAME_MAGIC;
	hb.messageType = 0xFF;  // Distinguish from StartCommand
	hb.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());

	sendto(pImpl->gameSock, (char*)&hb, sizeof(hb), 0,
		   (sockaddr*)&pImpl->peerAddr, sizeof(pImpl->peerAddr));
}

void NetworkManager::SendBoardDelta(const BoardDelta& delta)
{
	if (!pImpl->hasPeer || pImpl->gameSock == INVALID_SOCKET)
	{
		return;
	}

	BoardDelta netDelta = delta;
	netDelta.magic = GAME_MAGIC;
	netDelta.sequence = htonl(pImpl->sendSequence++);
	
	// Convert change locations to network byte order
	uint8_t count = delta.changeCount;
	if (count > 20) count = 20;
	
	for (int i = 0; i < count; i++)
	{
		netDelta.changes[i].x = htons(netDelta.changes[i].x);
		netDelta.changes[i].y = htons(netDelta.changes[i].y);
		// changeType is uint8_t, no conversion needed
	}

	sendto(pImpl->gameSock, (char*)&netDelta, sizeof(netDelta), 0,
		   (sockaddr*)&pImpl->peerAddr, sizeof(pImpl->peerAddr));
}

void NetworkManager::SetOnInputReceived(std::function<void(const InputMessage&)> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onInputReceived = callback;
}

void NetworkManager::SetOnGameStateReceived(std::function<void(const GameStateSnapshot&)> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onGameStateReceived = callback;
}

void NetworkManager::SetOnBoardDeltaReceived(std::function<void(const BoardDelta&)> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onBoardDeltaReceived = callback;
}

void NetworkManager::SetOnGameStartReceived(std::function<void()> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onGameStartReceived = callback;
}

void NetworkManager::SetOnConnected(std::function<void()> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onConnected = callback;
}

void NetworkManager::SetOnPeerDetected(std::function<void()> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onPeerDetected = callback;
}

void NetworkManager::AcceptConnection()
{
	if (!pImpl->hasPendingPeer)
	{
		return;
	}

	// Move pending peer to active peer
	pImpl->peerAddr = pImpl->pendingPeerAddr;
	pImpl->hasPeer = true;
	pImpl->hasPendingPeer = false;
	pImpl->connectionState = ConnectionState::Connected;

	// Notify connection established
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	if (pImpl->onConnected)
	{
		pImpl->onConnected();
	}
}

void NetworkManager::DeclineConnection()
{
	if (pImpl->hasPendingPeer)
	{
		// Clear pending peer and return to discovering state
		pImpl->hasPendingPeer = false;
		pImpl->connectionState = ConnectionState::Discovering;
	}
}

void NetworkManager::SetOnDisconnected(std::function<void()> callback)
{
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	pImpl->onDisconnected = callback;
}

std::string NetworkManager::GetPeerAddress() const
{
	// Check both active and pending peer
	if (pImpl->hasPeer)
	{
		char buffer[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &pImpl->peerAddr.sin_addr, buffer, sizeof(buffer));
		return std::string(buffer);
	}
	else if (pImpl->hasPendingPeer)
	{
		char buffer[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &pImpl->pendingPeerAddr.sin_addr, buffer, sizeof(buffer));
		return std::string(buffer);
	}
	
	return "Unknown";
}

std::string NetworkManager::GetLocalAddress() const
{
	char hostname[256];
	
	if (gethostname(hostname, sizeof(hostname)) == 0)
	{
		struct hostent* host = gethostbyname(hostname);
		if (host != nullptr && host->h_addr_list[0] != nullptr)
		{
			struct in_addr addr;
			memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
			char localIp[INET_ADDRSTRLEN];
			if (inet_ntop(AF_INET, &addr, localIp, sizeof(localIp)) != nullptr)
			{
				return std::string(localIp);
			}
		}
	}
	
	// Fallback: return localhost IP
	return "127.0.0.1";
}

void NetworkManager::SendDiscoveryBroadcast()
{
	DiscoveryMessage msg{};
	msg.magic = DISCOVERY_MAGIC;
	msg.messageType = 0; // announce

	sockaddr_in broadcastAddr{};
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
	broadcastAddr.sin_port = htons(DISCOVERY_PORT);

	sendto(pImpl->discoverySock, (char*)&msg, sizeof(msg), 0,
		   (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
}

void NetworkManager::HandleDiscoveryResponse(sockaddr_in& from)
{
	// Don't auto-connect if we already have a peer or pending peer
	if (pImpl->hasPeer || pImpl->hasPendingPeer)
	{
		return;
	}

	// Get local and remote IPs for comparison
	char localIp[INET_ADDRSTRLEN] = {};
	char remoteIp[INET_ADDRSTRLEN] = {};
	
	// Get local IP
	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == 0)
	{
		struct hostent* host = gethostbyname(hostname);
		if (host != nullptr && host->h_addr_list[0] != nullptr)
		{
			struct in_addr addr;
			memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
			inet_ntop(AF_INET, &addr, localIp, sizeof(localIp));
		}
	}
	
	inet_ntop(AF_INET, &from.sin_addr, remoteIp, sizeof(remoteIp));
	
	// CRITICAL FIX: Ignore discovery messages from ourselves!
	if (strcmp(localIp, remoteIp) == 0)
	{
		// This is our own broadcast, ignore it
		return;
	}
	
	// Determine role based on IP comparison
	// Lower IP address becomes host
	if (strcmp(localIp, remoteIp) < 0)
	{
		pImpl->role = NetworkRole::Host;
	}
	else
	{
		pImpl->role = NetworkRole::Client;
	}

	// Store as pending peer (MUST be done before callback!)
	pImpl->pendingPeerAddr = from;
	pImpl->pendingPeerAddr.sin_port = htons(GAME_PORT);
	pImpl->hasPendingPeer = true;
	pImpl->connectionState = ConnectionState::WaitingForPeer;

	// Send response
	DiscoveryMessage response{};
	response.magic = DISCOVERY_MAGIC;
	response.messageType = 1; // response

	sendto(pImpl->discoverySock, (char*)&response, sizeof(response), 0,
		   (sockaddr*)&from, sizeof(from));

	// Notify that a peer was detected (peer is now fully set up)
	std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
	if (pImpl->onPeerDetected)
	{
		pImpl->onPeerDetected();
	}
}

void NetworkManager::DiscoveryThreadFunc()
{
	char buffer[256];
	
	while (pImpl->running && pImpl->connectionState != ConnectionState::Connected)
	{
		// Send broadcast every 500ms
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - pImpl->lastBroadcastTime).count() > 500)
		{
			SendDiscoveryBroadcast();
			pImpl->lastBroadcastTime = now;
		}

		// Check for incoming discovery messages
		sockaddr_in from{};
		int fromLen = sizeof(from);
		int received = recvfrom(pImpl->discoverySock, buffer, sizeof(buffer), 0,
								(sockaddr*)&from, &fromLen);

		if (received == sizeof(DiscoveryMessage))
		{
			DiscoveryMessage* msg = (DiscoveryMessage*)buffer;
			if (msg->magic == DISCOVERY_MAGIC && !pImpl->hasPeer)
			{
				HandleDiscoveryResponse(from);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

void NetworkManager::NetworkThreadFunc()
{
	char buffer[65536];
	std::chrono::steady_clock::time_point lastAliveCheck = std::chrono::steady_clock::now();
	const int connectionTimeoutSeconds = 60;
	
	OutputDebugStringA("NetworkThreadFunc: Started\n");
	
	while (pImpl->running)
	{
		if (pImpl->connectionState != ConnectionState::Connected)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		sockaddr_in from{};
		int fromLen = sizeof(from);
		int received = recvfrom(pImpl->gameSock, buffer, sizeof(buffer), 0,
								(sockaddr*)&from, &fromLen);

		if (received > 0)
		{
			pImpl->lastReceiveTime = std::chrono::steady_clock::now();
			lastAliveCheck = pImpl->lastReceiveTime;

			// Check magic number
			uint32_t* magic = (uint32_t*)buffer;
			if (*magic != GAME_MAGIC)
			{
				OutputDebugStringA("NetworkThreadFunc: Invalid magic number\n");
				continue;
			}

			// Process based on message size
			if (received == sizeof(InputMessage))
			{
				InputMessage msg = *(InputMessage*)buffer;
				msg.sequence = ntohl(msg.sequence);
				
				std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
				if (pImpl->onInputReceived)
				{
					pImpl->onInputReceived(msg);
				}
			}
			else if (received == sizeof(StartCommand))
			{
				OutputDebugStringA("NetworkThreadFunc: Received StartCommand\n");
				StartCommand cmd = *(StartCommand*)buffer;
				
				std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
				if (pImpl->onGameStartReceived)
				{
					pImpl->onGameStartReceived();
				}
			}
			else if (received == sizeof(Heartbeat))
			{
				// Heartbeat received - connection is alive
				static int heartbeatCount = 0;
				if (++heartbeatCount % 10 == 0)
				{
					OutputDebugStringA("NetworkThreadFunc: Heartbeat received\n");
				}
			}
			else if (received == sizeof(BoardDelta))
			{
				// Board delta received - incremental board update
				static int deltaCount = 0;
				deltaCount++;
				
				BoardDelta delta = *(BoardDelta*)buffer;
				delta.sequence = ntohl(delta.sequence);
				
				// Validate change count
				if (delta.changeCount > 20) delta.changeCount = 20;
				
				// Convert change locations FROM network byte order
				for (int i = 0; i < delta.changeCount; i++)
				{
					delta.changes[i].x = ntohs(delta.changes[i].x);
					delta.changes[i].y = ntohs(delta.changes[i].y);
				}
				
				if (deltaCount % 20 == 0)
				{
					std::string debugMsg = "NetworkThreadFunc: Received BoardDelta #" + std::to_string(deltaCount) + 
					                       " with " + std::to_string(delta.changeCount) + " changes\n";
					OutputDebugStringA(debugMsg.c_str());
				}
				
				std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
				if (pImpl->onBoardDeltaReceived)
				{
					pImpl->onBoardDeltaReceived(delta);
				}
			}
			else if (received == sizeof(SnakeStateUpdate))
			{
				// Lightweight snake-only update (no board data)
				static int snakeUpdateCount = 0;
				snakeUpdateCount++;
				
				SnakeStateUpdate update = *(SnakeStateUpdate*)buffer;
				update.sequence = ntohl(update.sequence);
				
				// Convert counts
				update.snake1SegmentCount = ntohs(update.snake1SegmentCount);
				update.snake2SegmentCount = ntohs(update.snake2SegmentCount);
				update.player1Score = ntohs(update.player1Score);
				update.player2Score = ntohs(update.player2Score);
				
				// Bounds validation
				if (update.snake1SegmentCount > 500) update.snake1SegmentCount = 500;
				if (update.snake2SegmentCount > 500) update.snake2SegmentCount = 500;
				
				// Convert segment positions
				for (int i = 0; i < update.snake1SegmentCount; i++)
				{
					update.snake1Segments[i].x = ntohs(update.snake1Segments[i].x);
					update.snake1Segments[i].y = ntohs(update.snake1Segments[i].y);
				}
				
				for (int i = 0; i < update.snake2SegmentCount; i++)
				{
					update.snake2Segments[i].x = ntohs(update.snake2Segments[i].x);
					update.snake2Segments[i].y = ntohs(update.snake2Segments[i].y);
				}
				
				// Create a GameStateSnapshot from the update (without board data)
				// The callback will handle applying just the snake state
				GameStateSnapshot state{};
				state.sequence = update.sequence;
				state.snake1SegmentCount = update.snake1SegmentCount;
				memcpy(state.snake1Segments, update.snake1Segments, sizeof(SnakeSegment) * update.snake1SegmentCount);
				state.snake1VelocityX = update.snake1VelocityX;
				state.snake1VelocityY = update.snake1VelocityY;
				state.snake1MovePeriod = update.snake1MovePeriod;
				state.snake2SegmentCount = update.snake2SegmentCount;
				memcpy(state.snake2Segments, update.snake2Segments, sizeof(SnakeSegment) * update.snake2SegmentCount);
				state.snake2VelocityX = update.snake2VelocityX;
				state.snake2VelocityY = update.snake2VelocityY;
				state.snake2MovePeriod = update.snake2MovePeriod;
				state.player1Score = update.player1Score;
				state.player2Score = update.player2Score;
				state.gameOver = update.gameOver;
				state.crashedPlayer = update.crashedPlayer;
				// Board counts are 0 - this signals to not update board
				state.foodCount = 0;
				state.poisonCount = 0;
				state.barrierCount = 0;
				
				if (snakeUpdateCount % 60 == 0)
				{
					std::string debugMsg = "NetworkThreadFunc: Received SnakeStateUpdate #" + std::to_string(snakeUpdateCount) + "\n";
					OutputDebugStringA(debugMsg.c_str());
				}
				
				std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
				if (pImpl->onGameStateReceived)
				{
					pImpl->onGameStateReceived(state);
				}
			}
			else if (received == sizeof(GameStateSnapshot))
			{
				static int stateCount = 0;
				stateCount++;
				
				GameStateSnapshot state = *(GameStateSnapshot*)buffer;
				state.sequence = ntohl(state.sequence);
				
				// CRITICAL FIX: Convert all multi-byte fields FROM network byte order
				state.snake1SegmentCount = ntohs(state.snake1SegmentCount);
				state.snake2SegmentCount = ntohs(state.snake2SegmentCount);
				state.player1Score = ntohs(state.player1Score);
				state.player2Score = ntohs(state.player2Score);
				state.foodCount = ntohs(state.foodCount);
				state.poisonCount = ntohs(state.poisonCount);
				state.barrierCount = ntohs(state.barrierCount);
				
				// BOUNDS VALIDATION: Clamp counts to prevent buffer overruns from malformed packets
				if (state.snake1SegmentCount > 500) state.snake1SegmentCount = 500;
				if (state.snake2SegmentCount > 500) state.snake2SegmentCount = 500;
				if (state.foodCount > 100) state.foodCount = 100;
				if (state.poisonCount > 100) state.poisonCount = 100;
				if (state.barrierCount > 200) state.barrierCount = 200;
				
				// Convert all segment positions FROM network byte order
				for (int i = 0; i < state.snake1SegmentCount && i < 500; i++)
				{
					state.snake1Segments[i].x = ntohs(state.snake1Segments[i].x);
					state.snake1Segments[i].y = ntohs(state.snake1Segments[i].y);
				}
				
				for (int i = 0; i < state.snake2SegmentCount && i < 500; i++)
				{
					state.snake2Segments[i].x = ntohs(state.snake2Segments[i].x);
					state.snake2Segments[i].y = ntohs(state.snake2Segments[i].y);
				}
				
				// Convert food/poison/barrier locations FROM network byte order
				for (int i = 0; i < state.foodCount && i < 100; i++)
				{
					state.foodLocations[i].x = ntohs(state.foodLocations[i].x);
					state.foodLocations[i].y = ntohs(state.foodLocations[i].y);
				}
				
				for (int i = 0; i < state.poisonCount && i < 100; i++)
				{
					state.poisonLocations[i].x = ntohs(state.poisonLocations[i].x);
					state.poisonLocations[i].y = ntohs(state.poisonLocations[i].y);
				}
				
				for (int i = 0; i < state.barrierCount && i < 200; i++)
				{
					state.barrierLocations[i].x = ntohs(state.barrierLocations[i].x);
					state.barrierLocations[i].y = ntohs(state.barrierLocations[i].y);
				}
				
				// DEBUG: Log raw packet data AFTER conversion
				if (stateCount % 60 == 0)
				{
					std::string debugMsg = "NetworkThreadFunc: Received GameStateSnapshot #" + std::to_string(stateCount) + 
					                       ", size=" + std::to_string(received) + " bytes\n";
					OutputDebugStringA(debugMsg.c_str());
					
					std::string dataMsg = "  CONVERTED data - Snake1 segments: " + std::to_string(state.snake1SegmentCount) + 
					                      ", Snake2 segments: " + std::to_string(state.snake2SegmentCount) + 
					                      ", gameOver: " + std::to_string(state.gameOver) + "\n";
					OutputDebugStringA(dataMsg.c_str());
				}
				
				// Invoke callback
				{
					std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
					if (!pImpl->onGameStateReceived)
					{
						OutputDebugStringA("NetworkThreadFunc: ERROR - onGameStateReceived callback is NULL!\n");
					}
					else
					{
						pImpl->onGameStateReceived(state);
						
						if (stateCount % 60 == 0)
						{
							OutputDebugStringA("NetworkThreadFunc: Callback invoked successfully\n");
						}
					}
				}
			}
			else
			{
				std::string debugMsg = "NetworkThreadFunc: Unknown message size: " + std::to_string(received) + " bytes\n";
				OutputDebugStringA(debugMsg.c_str());
			}
		}

		// Check for timeout
		auto now = std::chrono::steady_clock::now();
		auto timeSinceLastReceive = std::chrono::duration_cast<std::chrono::seconds>(now - lastAliveCheck).count();
		
		if (timeSinceLastReceive > connectionTimeoutSeconds)
		{
			// Connection timeout detected
			if (pImpl->hasPeer)
			{
				pImpl->hasPeer = false;
				pImpl->connectionState = ConnectionState::Disconnected;
				
				OutputDebugStringA("NetworkThreadFunc: Connection timeout!\n");
				
				std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
				if (pImpl->onDisconnected)
				{
					pImpl->onDisconnected();
				}
			}
			
			// Reset timer to avoid repeated callbacks
			lastAliveCheck = now;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60Hz
	}
	
	OutputDebugStringA("NetworkThreadFunc: Exiting\n");
}
