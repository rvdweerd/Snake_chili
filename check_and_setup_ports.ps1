# Snake Game - Port Checker and Firewall Setup Script
# Run this script as Administrator

Write-Host "=== Snake Game Network Port Checker ===" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "WARNING: This script is not running as Administrator." -ForegroundColor Yellow
    Write-Host "Some checks and firewall rule creation will fail." -ForegroundColor Yellow
    Write-Host "Right-click PowerShell and select 'Run as Administrator' to fix this." -ForegroundColor Yellow
    Write-Host ""
}

# Define ports
$discoveryPort = 47777
$gamePort = 47778

Write-Host "Checking UDP ports $discoveryPort and $gamePort..." -ForegroundColor Green
Write-Host ""

# Check if ports are currently listening
Write-Host "1. Checking if ports are currently in use:" -ForegroundColor Yellow
$listeningPorts = Get-NetUDPEndpoint | Where-Object {$_.LocalPort -eq $discoveryPort -or $_.LocalPort -eq $gamePort}
if ($listeningPorts) {
    Write-Host "   ? Ports are currently active (game is running or another app is using them):" -ForegroundColor Green
    $listeningPorts | Format-Table LocalAddress, LocalPort, OwningProcess -AutoSize
    
    # Get process names
    foreach ($port in $listeningPorts) {
        $process = Get-Process -Id $port.OwningProcess -ErrorAction SilentlyContinue
        if ($process) {
            Write-Host "   Port $($port.LocalPort) is used by: $($process.Name) (PID: $($port.OwningProcess))" -ForegroundColor Cyan
        }
    }
} else {
    Write-Host "   ? Ports are not currently listening (game is not running)" -ForegroundColor Gray
}
Write-Host ""

# Check Windows Firewall rules
Write-Host "2. Checking Windows Firewall rules:" -ForegroundColor Yellow
$firewallRules = Get-NetFirewallRule | Where-Object {
    $_.DisplayName -like "*Snake*" -or 
    $_.DisplayName -like "*47777*" -or 
    $_.DisplayName -like "*47778*"
}

if ($firewallRules) {
    Write-Host "   ? Found existing firewall rules:" -ForegroundColor Green
    $firewallRules | Select-Object DisplayName, Enabled, Direction, Action | Format-Table -AutoSize
} else {
    Write-Host "   ? No firewall rules found for Snake game" -ForegroundColor Red
    Write-Host ""
    
    if ($isAdmin) {
        $create = Read-Host "   Would you like to create firewall rules now? (Y/N)"
        if ($create -eq "Y" -or $create -eq "y") {
            Write-Host "   Creating firewall rules..." -ForegroundColor Cyan
            
            try {
                # Inbound rule for discovery
                New-NetFirewallRule -DisplayName "Snake Game - Discovery (UDP 47777)" `
                    -Direction Inbound -Protocol UDP -LocalPort $discoveryPort `
                    -Action Allow -Profile Any | Out-Null
                Write-Host "   ? Created inbound rule for port $discoveryPort" -ForegroundColor Green
                
                # Inbound rule for game sync
                New-NetFirewallRule -DisplayName "Snake Game - Sync (UDP 47778)" `
                    -Direction Inbound -Protocol UDP -LocalPort $gamePort `
                    -Action Allow -Profile Any | Out-Null
                Write-Host "   ? Created inbound rule for port $gamePort" -ForegroundColor Green
                
                # Outbound rules
                New-NetFirewallRule -DisplayName "Snake Game - Discovery Out (UDP 47777)" `
                    -Direction Outbound -Protocol UDP -LocalPort $discoveryPort `
                    -Action Allow -Profile Any | Out-Null
                Write-Host "   ? Created outbound rule for port $discoveryPort" -ForegroundColor Green
                
                New-NetFirewallRule -DisplayName "Snake Game - Sync Out (UDP 47778)" `
                    -Direction Outbound -Protocol UDP -LocalPort $gamePort `
                    -Action Allow -Profile Any | Out-Null
                Write-Host "   ? Created outbound rule for port $gamePort" -ForegroundColor Green
                
                Write-Host ""
                Write-Host "   Firewall rules created successfully!" -ForegroundColor Green
            } catch {
                Write-Host "   ? Error creating firewall rules: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
    } else {
        Write-Host "   Run this script as Administrator to create firewall rules automatically." -ForegroundColor Yellow
    }
}
Write-Host ""

# Check for Norton 360
Write-Host "3. Checking for Norton 360:" -ForegroundColor Yellow
$nortonProcesses = Get-Process | Where-Object {$_.Name -like "*Norton*" -or $_.Name -like "*NortonSecurity*"}
if ($nortonProcesses) {
    Write-Host "   ! Norton 360 or Norton Security is running" -ForegroundColor Yellow
    Write-Host "   You need to configure Norton separately (see MULTIPLAYER_README.md)" -ForegroundColor Yellow
    Write-Host "   Norton processes found:" -ForegroundColor Cyan
    $nortonProcesses | Select-Object Name, Id | Format-Table -AutoSize
} else {
    Write-Host "   ? Norton 360 not detected" -ForegroundColor Green
}
Write-Host ""

# Test if you can bind to the ports (requires game to be closed)
Write-Host "4. Testing if ports can be bound:" -ForegroundColor Yellow
Write-Host "   (This test requires the game to be closed)" -ForegroundColor Gray

function Test-UDPPort {
    param([int]$Port)
    try {
        $udpClient = New-Object System.Net.Sockets.UdpClient
        $udpClient.Client.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, 
                                          [System.Net.Sockets.SocketOptionName]::ReuseAddress, 
                                          $true)
        $udpClient.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, $Port))
        $udpClient.Close()
        return $true
    } catch {
        return $false
    }
}

$canBindDiscovery = Test-UDPPort -Port $discoveryPort
$canBindGame = Test-UDPPort -Port $gamePort

if ($canBindDiscovery) {
    Write-Host "   ? Port $discoveryPort is available" -ForegroundColor Green
} else {
    Write-Host "   ? Port $discoveryPort is in use or blocked" -ForegroundColor Red
}

if ($canBindGame) {
    Write-Host "   ? Port $gamePort is available" -ForegroundColor Green
} else {
    Write-Host "   ? Port $gamePort is in use or blocked" -ForegroundColor Red
}
Write-Host ""

# Summary
Write-Host "=== Summary ===" -ForegroundColor Cyan
Write-Host ""
if ($firewallRules -and $canBindDiscovery -and $canBindGame) {
    Write-Host "? Your system is configured correctly for multiplayer!" -ForegroundColor Green
    Write-Host "  You can now run the game and it should find other players on your network." -ForegroundColor Green
} elseif (-not $firewallRules) {
    Write-Host "? Firewall rules are missing" -ForegroundColor Red
    Write-Host "  Follow the instructions in MULTIPLAYER_README.md to configure your firewall." -ForegroundColor Yellow
} elseif (-not $canBindDiscovery -or -not $canBindGame) {
    Write-Host "! Ports may be in use or blocked" -ForegroundColor Yellow
    Write-Host "  If the game is running, this is normal. Otherwise, check for conflicts." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
