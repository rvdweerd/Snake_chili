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
struct BoardDelta;

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

	// Send local player input to peer (actions is a bitfield of InputAction flags)
	void SendInput(int8_t vx, int8_t vy, uint8_t actions, float movePeriod);

	// Send full game state (host only) - used for initial sync
	void SendGameState(const GameStateSnapshot& state);
	
	// Send board delta (host only) - used for incremental updates
	void SendBoardDelta(const BoardDelta& delta);

	// Send start game command (host only)
	void SendStartCommand();

	// Send heartbeat keepalive (both host and client)
	void SendHeartbeat();

	// Set callback for received input (host uses this)
	void SetOnInputReceived(std::function<void(const InputMessage&)> callback);

	// Set callback for received game state (client uses this)
	void SetOnGameStateReceived(std::function<void(const GameStateSnapshot&)> callback);
	
	// Set callback for received board delta (client uses this)
	void SetOnBoardDeltaReceived(std::function<void(const BoardDelta&)> callback);

	// Set callback for received start game command (client uses this)
	void SetOnGameStartReceived(std::function<void()> callback);

	// Set callback for connection established
	void SetOnConnected(std::function<void()> callback);

	// Set callback for peer detected (before accepting connection)
	void SetOnPeerDetected(std::function<void()> callback);
	
	// Accept pending connection (call after user confirms)
	void AcceptConnection();

	// Decline pending connection (call when user rejects)
	void DeclineConnection();

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

// Action flags for InputMessage - one-shot actions (edge-triggered)
namespace InputAction
{
	constexpr uint8_t None    = 0;
	constexpr uint8_t Jump    = 1 << 0;  // Perform a jump
	constexpr uint8_t Faster  = 1 << 1;  // Speed up one step
	constexpr uint8_t Slower  = 1 << 2;  // Slow down one step
	constexpr uint8_t Stall   = 1 << 3;  // Stop moving (set velocity to 0,0)
}

struct InputMessage
{
	uint32_t magic;
	uint32_t sequence;
	int8_t vx;              // Direction X (state - continuous)
	int8_t vy;              // Direction Y (state - continuous)
	uint8_t actions;        // Bitfield of one-shot actions (InputAction flags)
	uint8_t padding;
	float movePeriod;       // Current move period (for sync, not for actions)
};

struct StartCommand
{
	uint32_t magic;
	uint8_t commandType; // 0 = start game
	char padding[3];
};

struct Heartbeat
{
	uint32_t magic;
	uint8_t messageType;  // 0xFF to distinguish from StartCommand (which has 0)
	char padding[3];
	uint32_t timestamp; // For ping calculation if needed
};

struct SnakeSegment
{
	int16_t x;
	int16_t y;
};

// Board delta change types
enum class BoardChangeType : uint8_t
{
	FoodAdded = 0,
	FoodRemoved = 1,
	PoisonAdded = 2,
	PoisonRemoved = 3,
	BarrierAdded = 4,
	BarrierRemoved = 5
};

// Single board change entry
struct BoardChange
{
	uint8_t changeType;  // BoardChangeType
	int16_t x;
	int16_t y;
};

// Board delta message - contains multiple changes
struct BoardDelta
{
	uint32_t magic;
	uint32_t sequence;
	uint8_t changeCount;      // Number of changes in this delta (max 20)
	uint8_t padding[3];
	BoardChange changes[20];  // Up to 20 changes per message
};

// Lightweight snake-only state for frequent updates (no board data)
struct SnakeStateUpdate
{
	uint32_t magic;
	uint32_t sequence;
	
	// Snake 1 data
	uint16_t snake1SegmentCount;
	SnakeSegment snake1Segments[500];
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
