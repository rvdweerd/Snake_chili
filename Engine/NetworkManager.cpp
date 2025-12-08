#include "NetworkManager.h"
#include <chrono>
#include <iostream>

struct NetworkManager::Impl
{
	SOCKET discoverySock = INVALID_SOCKET;
	SOCKET gameSock = INVALID_SOCKET;
	
	sockaddr_in peerAddr{};
	sockaddr_in pendingPeerAddr{};
	bool hasPeer = false;
	bool hasPendingPeer = false;
	
	std::thread discoveryThread;
	std::thread networkThread;
	std::atomic<bool> running{false};
	std::atomic<ConnectionState> connectionState{ConnectionState::Idle};
	std::atomic<NetworkRole> role{NetworkRole::None};
	
	std::mutex callbackMutex;
	std::function<void(const InputMessage&)> onInputReceived;
	std::function<void(const GameStateSnapshot&)> onGameStateReceived;
	std::function<void()> onConnected;
	std::function<void()> onPeerDetected;
	std::function<void()> onDisconnected;
	
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

	sendto(pImpl->gameSock, (char*)&netState, sizeof(netState), 0,
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
		
		// Send optional rejection message to peer (future enhancement)
		// This allows the peer to know the connection was declined
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
	char localIp[INET_ADDRSTRLEN] = {};
	char hostname[256];
	
	if (gethostname(hostname, sizeof(hostname)) == 0)
	{
		struct hostent* host = gethostbyname(hostname);
		if (host != nullptr && host->h_addr_list[0] != nullptr)
		{
			struct in_addr addr;
			memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
			inet_ntop(AF_INET, &addr, localIp, sizeof(localIp));
			return std::string(localIp);
		}
	}
	
	return "Unknown";
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
	const int connectionTimeoutSeconds = 5;
	
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
			else if (received == sizeof(GameStateSnapshot))
			{
				GameStateSnapshot state = *(GameStateSnapshot*)buffer;
				state.sequence = ntohl(state.sequence);
				
				std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
				if (pImpl->onGameStateReceived)
				{
					pImpl->onGameStateReceived(state);
				}
			}
		}

		// Check for timeout with proper interval
		auto now = std::chrono::steady_clock::now();
		auto timeSinceLastReceive = std::chrono::duration_cast<std::chrono::seconds>(now - lastAliveCheck).count();
		
		if (timeSinceLastReceive > connectionTimeoutSeconds)
		{
			// Connection timeout detected
			if (pImpl->hasPeer)
			{
				pImpl->hasPeer = false;
				pImpl->connectionState = ConnectionState::Disconnected;
				
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
}
