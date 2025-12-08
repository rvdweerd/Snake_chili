# Client Screen Not Updating - Debugging Guide

## Issue
The client game screen does not show moving snakes (no updates visible).

## Root Causes Identified

### 1. **Client Does NOT Process Game Logic Locally**
```cpp
// In UpdateModel():
bool shouldProcessGameLogic = !networkingEnabled || isNetworkHost;
// For CLIENT: this is FALSE, so snakes never move locally
```

? **This is CORRECT behavior** - Client should receive state from host, not simulate locally.

### 2. **Client Relies Entirely on Network Updates**
The client ONLY updates snakes through `ApplyGameStateSnapshot()` callback, which is called when GameStateSnapshot messages arrive from the host.

## Debugging Steps

### Step 1: Check if Updates are Being Received
Run the game with Visual Studio attached and check the **Output window** for debug messages:

**HOST side should show:**
```
HOST: Sending game state update
HOST: Sending game state update
...
```

**CLIENT side should show:**
```
CLIENT: Received game state update
Snake1 segments: 5, Snake2 segments: 5
CLIENT: Received game state update
Snake1 segments: 6, Snake2 segments: 5
...
```

If you don't see CLIENT messages, the network updates aren't arriving!

### Step 2: Verify Network Connection
Check the bottom-right corner of the game screen:
- Should show: `CLIENT` or `HOST`
- Should show: `P1:NETWORK` and `P2:LOCAL` (or vice versa)

If these don't appear, the game thinks it's not in network mode.

### Step 3: Check Windows Firewall
The game uses UDP ports **47777** (discovery) and **47778** (game data).

Run PowerShell script to check:
```powershell
.\check_and_setup_ports.ps1
```

### Step 4: Verify Game State Serialization
Add breakpoint in `ApplyGameStateSnapshot()` and check:
- Is the function being called?
- What are the values of `state.snake1SegmentCount` and `state.snake2SegmentCount`?
- Are they > 0?

### Step 5: Check Snake.SetSegments()
Add breakpoint in `Snake::SetSegments()` to verify:
- Is it being called?
- Is `segmentCount` valid (> 0)?
- Are the locations being set correctly?

## Potential Issues & Solutions

### Issue A: Callback Not Being Invoked
**Symptom:** No "CLIENT: Received" messages in Output window

**Possible Causes:**
1. Network packets not arriving (firewall/router)
2. UDP packets being dropped
3. Wrong IP address being used
4. Network thread not running

**Solution:**
- Check firewall rules
- Verify both machines are on same subnet
- Check Output window for "NETWORK ERROR" messages

### Issue B: Thread Safety Problem
**Symptom:** Callbacks are invoked but screen doesn't update

**Possible Cause:** The callback modifies snake objects from network thread while main thread tries to draw them.

**Current Protection:**
```cpp
// In NetworkManager, callbacks use mutex
std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
```

But the Snake objects themselves don't have thread protection!

**Solution:** Already implemented - callbacks should be fast and atomic operations like `SetSegments()` should be safe.

### Issue C: SetSegments() Not Working Correctly
**Symptom:** Callbacks invoked, but snakes have wrong positions

**Check:**
```cpp
void Snake::SetSegments(const Location* locations, int segmentCount)
{
    if (segmentCount > 0 && segmentCount < nSegmentsMax)
    {
        segments.resize(segmentCount);  // Resize vector
        
        for (int i = 0; i < segmentCount; i++)
        {
            segments[i].loc = locations[i];  // Copy positions
        }
    }
}
```

Verify `nSegmentsMax` (should be 2000) and that `locations` array is valid.

### Issue D: Client Not Entering Network Mode
**Symptom:** No network status shown, game acts like single-player

**Check in Game.cpp constructor:**
```cpp
networkMgr.SetOnConnected([this]() {
    isNetworkHost = (networkMgr.GetRole() == NetworkRole::Host);
    networkingEnabled = true;  // ? This must be set!
    userWantsNetworking = true;
    gVar.numPlayers = 2;
});
```

If `networkingEnabled` is false, client won't process network updates.

## Quick Test

### Minimal Working Test:
1. **HOST Machine:**
   - Launch game
   - Press `N` (start network mode)
   - Wait for "SEARCHING FOR PLAYERS..."
   - When peer found, press `Y` to accept
   - Press `ENTER` to start game
   - **Check Visual Studio Output window for "HOST: Sending" messages**

2. **CLIENT Machine:**
   - Launch game
   - Press `N` (start network mode)
   - Wait for peer detection
   - Press `Y` to accept
   - Wait for "WAITING FOR HOST TO START..."
   - **Check Visual Studio Output window for "CLIENT: Received" messages**
   - Screen should update automatically when host starts

### Expected Behavior:
- HOST screen: Shows both snakes moving
- CLIENT screen: Shows both snakes moving (synchronized with host)
- CLIENT Output: "CLIENT: Received game state update" every ~3 seconds

## If Still Not Working

### Enable Full Debug Logging
Modify `ApplyGameStateSnapshot` to log EVERY update:
```cpp
// Remove "% 60" check to log every single update
OutputDebugStringA("CLIENT: Received game state update\n");
```

### Check Packet Contents
In NetworkThreadFunc, add logging:
```cpp
else if (received == sizeof(GameStateSnapshot))
{
    OutputDebugStringA("Network: Received GameStateSnapshot packet\n");
    // ... rest of code
}
```

### Verify ComposeFrame() is Drawing
The client should ALWAYS call:
```cpp
if (isStarted)
{
    brd.DrawBorders();
    brd.DrawCellContents();
    snk1.Draw(brd);  // ? Should draw snake 1
    if (gVar.numPlayers == 2)
    {
        snk2.Draw(brd);  // ? Should draw snake 2
    }
}
```

Add debug output before `snk1.Draw()`:
```cpp
std::string debugMsg = "Drawing snake1 with " + std::to_string(snk1.GetSegmentCount()) + " segments\n";
OutputDebugStringA(debugMsg.c_str());
```

## Summary Checklist

- [ ] Both machines have firewall rules allowing UDP 47777 and 47778
- [ ] Output window shows "HOST: Sending" messages on host
- [ ] Output window shows "CLIENT: Received" messages on client
- [ ] Connection status ("CLIENT"/"HOST") visible on screen
- [ ] `networkingEnabled` is true on client
- [ ] `ApplyGameStateSnapshot` is being called
- [ ] `Snake::SetSegments()` is being called with valid data
- [ ] `segmentCount` > 0 in received updates
- [ ] Snakes have valid segment locations
- [ ] ComposeFrame() is calling Draw() for both snakes

## Known Working Configuration

- Windows 10/11
- Same subnet (192.168.x.x)
- Firewall rules configured
- UDP ports 47777 and 47778 open
- Both machines running Debug build
- Visual Studio Output window visible
- Game running at 60 FPS
- Network updates at 20 Hz

## Contact
If still not working after following this guide, provide:
1. Output window logs from both HOST and CLIENT
2. Screenshot of game screen on both machines
3. Network configuration (ipconfig output)
4. Firewall status
