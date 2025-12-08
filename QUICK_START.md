# Snake Game - Quick Start Guide

## ?? Game Modes

### Single Player
- Set `numPlayers = 1` in `data.txt`
- Control snake with Numpad keys:
  - **4** = Left
  - **6** = Right  
  - **8** = Up
  - **2** = Down
  - **5** = Jump

### Local 2-Player (Same Computer)
- Set `numPlayers = 2` in `data.txt`
- **Player 1** (Magenta snake, top-right corner):
  - Numpad **4** = Left, **6** = Right, **8** = Up, **2** = Down
  - Numpad **5** = Jump
  - Numpad **7** = Slower, **1** = Faster, **9** = Stop
  
- **Player 2** (Green snake, bottom-left corner):
  - **A** = Left, **D** = Right, **W** = Up, **X** = Down
  - **S** = Jump
  - **Z** = Slower, **Q** = Faster, **E** = Stop

### Network Multiplayer (Two Computers)
1. Set `numPlayers = 2` in `data.txt` on BOTH computers
2. Make sure both computers are on the same network
3. Run game on BOTH computers
4. On the title screen, press **'N'** on BOTH computers to start network discovery
5. Wait for "Network Player Found" message
6. Press **'Y'** to accept or **'O'** to decline
7. Once connected, press **ENTER** to start game

## ?? Game Controls

- **ENTER** = Start game / Restart after game over
- **N** = Enable network discovery (title screen only)
- **ESC** = Quit game

## ?? Score Display

- **Top Right**: Player 1 score
- **Top Left**: Player 2 score (only in 2-player mode)

## ?? Gameplay

- **Objective**: Eat blue food to grow and increase your score
- **Avoid**: 
  - Hitting your own body
  - Hitting the other player's snake
  - Hitting white barriers
  
- **Magenta orbs** (poison): Make your snake move faster

## ?? Configuration (data.txt)

```
[Tile Size]
15                  # Size of each grid cell

[Board Size]
50 38              # Width and height of play area

[Speedup Rate]
1.01f              # How much faster after eating poison

[Poison Amount]
0                  # Number of poison orbs (0 = none)

[Food Amount]
50                 # Number of food items on board

[Initial Speed]
.5f                # Starting speed (lower = faster)

[Initial Snakelength]
5                  # Starting snake length

[Num Players]
2                  # 1 = single player, 2 = two players
```

## ?? Troubleshooting

### Game crashes on startup
- Make sure `data.txt` exists in the same folder as `Engine.exe`
- Check that all values in `data.txt` are valid numbers
- Try setting `numPlayers = 1` first to test single player

### Network mode not working
- Check firewall settings (ports 47777 and 47778)
- Make sure both computers are on the same local network
- Try disabling Norton/antivirus temporarily
- If networking fails, you can still play local 2-player mode!

### Can't find the other player
- Both players must press **'N'** on title screen
- Wait 5-10 seconds for discovery
- Make sure no other instances of the game are running
- Check that both computers have different IP addresses

## ?? Tips

1. **Practice in single player** first to learn the controls
2. **Start slow** - don't eat poison orbs until you're comfortable
3. **Coordinate in 2-player** - don't crash into each other!
4. **Use the jump** button strategically to move faster
5. **Control your speed** - faster isn't always better

## ?? Strategy

- **Early game**: Focus on eating food and avoiding barriers
- **Mid game**: Control the center of the board
- **Late game**: With a long snake, defensive play is key
- **2-player**: Try to box in your opponent or force them into barriers

## ?? Credits

Based on Chili DirectX Framework
Snake game with 2-player support and networking by [Your Name]

Enjoy the game! ????
