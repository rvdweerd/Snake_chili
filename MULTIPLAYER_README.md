# Snake Game - LAN Multiplayer Setup Guide

## Overview
The game now supports networked multiplayer over LAN. Two computers on the same local network can automatically discover each other and play together.

## Configuration

### Setting Player Mode (`data.txt`)

The game respects the `[Num Players]` setting in `Engine/data.txt`:

- **`[Num Players] = 1`** (Single Player Mode)
  - Game starts in single-player mode
  - If another player is detected on the network, you'll see a prompt:
    - **Press Y** to accept multiplayer (switches to 2-player network mode)
    - **Press N** to decline and continue in single-player
  - You maintain control over whether to play networked or solo

- **`[Num Players] = 2`** (Multiplayer Mode)
  - Game automatically accepts network connections
  - If no network peer is found, plays local 2-player (both snakes on same keyboard)
  - When network peer connects, switches to networked mode automatically

## How It Works

### Automatic Discovery
- When the game starts, it automatically begins broadcasting on the local network looking for other players
- The first two computers running the game will automatically pair together
- The computer with the lower IP address becomes the "Host"
- The other computer becomes the "Client"

### Roles
- **Host**: Runs the full game simulation and controls player 1 (numpad keys)
- **Client**: Sends input for player 2 (WASD keys) and receives game state updates

## Controls

### Player 1 (Host)
- Numpad 6: Move right
- Numpad 4: Move left
- Numpad 2: Move down
- Numpad 8: Move up
- Numpad 7: Slow down
- Numpad 1: Speed up
- Numpad 9: Stop
- Numpad 5: Jump

### Player 2 (Client)
- D: Move right
- A: Move left
- X: Move down
- W: Move up
- Z: Slow down
- Q: Speed up
- E: Stop
- S: Jump

### Both Players
- Enter: Start/Restart game

## Network Requirements

### Firewall Configuration
You need to allow the following UDP ports through your firewall:
- **Port 47777**: Used for discovery (finding other players)
- **Port 47778**: Used for game synchronization

### Windows Firewall Instructions
1. Open "Windows Defender Firewall with Advanced Security"
2. Click "Inbound Rules" ? "New Rule"
3. Select "Port" ? Next
4. Select "UDP" and enter "47777, 47778" ? Next
5. Select "Allow the connection" ? Next
6. Check all profiles (Domain, Private, Public) ? Next
7. Name it "Snake Game" ? Finish

### Norton 360 Firewall Instructions
If you have Norton 360 installed, you need to configure it separately:

#### Method 1: Add Program Rule (Recommended)
1. Open Norton 360
2. Click "Settings" (gear icon)
3. Click "Firewall" ? "Program Control"
4. Click "Add" or find your Snake game executable in the list
5. Browse to your game's `.exe` file (e.g., `Engine.exe` or `Snek.exe`)
6. Set access to "Allow" or "Custom"
7. If using Custom:
   - Click "Configure" next to the program
   - Under "Network Access", ensure "Allow all network activity" is checked
   - Or manually add rules for UDP ports 47777 and 47778
8. Click "Apply" and "OK"

#### Method 2: Add Traffic Rules
1. Open Norton 360
2. Click "Settings" (gear icon)
3. Click "Firewall" ? "Traffic Rules"
4. Click "Add" to create a new rule
5. Configure the rule:
   - **Rule name**: Snake Game Discovery
   - **Action**: Allow
   - **Protocol**: UDP
   - **Direction**: Both (or create separate Inbound/Outbound rules)
   - **Local ports**: 47777
   - **Remote ports**: Any
6. Click "OK"
7. Repeat steps 4-6 for port 47778 (Snake Game Sync)

#### Method 3: Temporarily Disable Smart Firewall (Testing Only)
1. Open Norton 360
2. Click "Settings"
3. Click "Firewall" ? "Smart Firewall"
4. Toggle "Smart Firewall" to OFF
5. Select duration (15 minutes, 1 hour, etc.)
6. Test if game connects
7. **Important**: Re-enable the firewall after testing and use Method 1 or 2 for permanent solution

#### Troubleshooting Norton 360
- **Check Program Trust**: In Norton ? Security ? Advanced ? Program Control, verify your game is set to "Allow" or "Trust"
- **Check Intrusion Prevention**: Go to Settings ? Firewall ? Intrusion Prevention, and ensure it's not blocking your game
- **Network Trust Level**: Ensure your home network is set to "Trust" level in Norton (Settings ? Firewall ? Network Trust)
- **Check Firewall Log**: Settings ? Firewall ? View Recent Firewall Activity to see if Norton is blocking your game

### Testing on Same Computer
You can test the networking by running two instances of the game on the same computer. They will find each other and connect automatically.

## Connection Status
- **Single Player Mode** (`numPlayers = 1` in `data.txt`):
  - Game searches for network peers in the background
  - When a peer is detected, a yellow prompt appears on the title screen
  - Press **Y** to accept multiplayer or **N** to decline
  - If declined, continues in single-player mode

- **Multiplayer Mode** (`numPlayers = 2` in `data.txt`):
  - Game automatically accepts the first network peer found
  - Switches to 2-player mode when connection is established
  
- **During Gameplay**:
  - If the connection is lost (timeout > 5 seconds), the game reverts to your original `numPlayers` setting
  - In single-player mode: only snake 1 remains active
  - In local 2-player mode: both snakes remain active on the same keyboard

- **Score Display**:
  - Player 1 score: Top right
  - Player 2 score: Top left (only in 2-player mode)

## Troubleshooting

### Can't Find Other Player
1. Verify both computers are on the same network (same subnet)
2. Check that firewall allows UDP ports 47777 and 47778
3. Disable antivirus temporarily to test if it's blocking
4. Make sure both computers are running the game

### Connection Drops
- Check network stability
- Reduce distance to router/access point
- Close bandwidth-heavy applications

### Lag or Stuttering
- The host sends game state at 20Hz (50ms intervals)
- High network latency will cause stuttering
- Try connecting both computers to router via Ethernet cable

## Technical Details

### Architecture
- **Authoritative Host Model**: The host runs the full game simulation
- **State Synchronization**: Host sends complete game state to client 20 times per second
- **Input Prediction**: Client can see their own input immediately while waiting for host confirmation
- **UDP Protocol**: Uses connectionless UDP for low latency

### Network Messages
- **Discovery Broadcast**: Sent every 500ms to find peers
- **Input Messages**: Client sends velocity changes to host
- **State Snapshots**: Host sends full game state including both snakes, scores, and game status

## Known Limitations
1. Only supports 2 players (1 vs 1)
2. No support for more than 2 players simultaneously
3. Board items (food/poison/barriers) are controlled by host only
4. No spectator mode
5. Connection must be on same LAN (no internet play)

## Future Enhancements (Not Implemented)
- NAT traversal for playing over internet
- Host migration (if host disconnects, client could become new host)
- Full snake body synchronization (currently only head position)
- Board state delta compression for reduced bandwidth
- Latency compensation and interpolation
- Lobby system for multiple game rooms
