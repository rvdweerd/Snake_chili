#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include "Location.h"

#pragma comment(lib, "Ws2_32.lib")

// Forward declarations
struct GameStateSnapshot;
struct InputMessage;

enum class NetworkRole
{
	None,
	Discovering,
	Host,
	Client
};

enum class ConnectionState
{
	Idle,
	Discovering,
	WaitingForPeer,
	Connected,
	Disconnected
};

class NetworkManager
{
public:
	NetworkManager();
	~NetworkManager();

	// Start discovery process (broadcasts presence and listens for peers)
	bool StartDiscovery();
	
	// Stop networking
	void Stop();

	// Check current connection state
	ConnectionState GetConnectionState() const;
	NetworkRole GetRole() const;

	// Send local player input to peer
	void SendInput(int8_t vx, int8_t vy, bool jump, float movePeriod);

	// Send full game state (host only)
	void SendGameState(const GameStateSnapshot& state);

	// Set callback for received input (host uses this)
	void SetOnInputReceived(std::function<void(const InputMessage&)> callback);

	// Set callback for received game state (client uses this)
	void SetOnGameStateReceived(std::function<void(const GameStateSnapshot&)> callback);

	// Set callback for connection established
	void SetOnConnected(std::function<void()> callback);

	// Set callback for peer detected (before accepting connection)
	void SetOnPeerDetected(std::function<void()> callback);
	
	// Accept pending connection (call after user confirms)
	void AcceptConnection();

	// Set callback for connection lost
	void SetOnDisconnected(std::function<void()> callback);

	// Get peer address for display
	std::string GetPeerAddress() const;

	// Get local address for display
	std::string GetLocalAddress() const;

private:
	void DiscoveryThreadFunc();
	void NetworkThreadFunc();
	
	bool InitializeSocket();
	void CleanupSocket();
	
	void ProcessDiscoveryMessages();
	void ProcessGameMessages();
	
	void SendDiscoveryBroadcast();
	void HandleDiscoveryResponse(sockaddr_in& from);

	struct Impl;
	Impl* pImpl;

	static constexpr uint16_t DISCOVERY_PORT = 47777;
	static constexpr uint16_t GAME_PORT = 47778;
	static constexpr uint32_t DISCOVERY_MAGIC = 0x534E4B31; // "SNK1"
	static constexpr uint32_t GAME_MAGIC = 0x534E4B32;      // "SNK2"
};

// Message structures
#pragma pack(push, 1)

struct DiscoveryMessage
{
	uint32_t magic;
	uint8_t messageType; // 0 = announce, 1 = response
	char padding[3];
};

struct InputMessage
{
	uint32_t magic;
	uint32_t sequence;
	int8_t vx;
	int8_t vy;
	uint8_t jump;      // 0 or 1
	uint8_t padding;
	float movePeriod;
};

struct SnakeSegment
{
	int16_t x;
	int16_t y;
};

struct GameStateSnapshot
{
	uint32_t magic;
	uint32_t sequence;
	
	// Snake 1 data
	uint16_t snake1SegmentCount;
	SnakeSegment snake1Segments[500]; // Max segments
	int8_t snake1VelocityX;
	int8_t snake1VelocityY;
	float snake1MovePeriod;
	
	// Snake 2 data
	uint16_t snake2SegmentCount;
	SnakeSegment snake2Segments[500];
	int8_t snake2VelocityX;
	int8_t snake2VelocityY;
	float snake2MovePeriod;
	
	// Game state
	uint16_t player1Score;
	uint16_t player2Score;
	uint8_t gameOver;
	uint8_t crashedPlayer;
	
	// Board state (simplified - just food/poison/barrier locations)
	uint16_t foodCount;
	SnakeSegment foodLocations[100];
	uint16_t poisonCount;
	SnakeSegment poisonLocations[100];
	uint16_t barrierCount;
	SnakeSegment barrierLocations[200];
};

#pragma pack(pop)
