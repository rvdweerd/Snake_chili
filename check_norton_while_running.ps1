# Norton Permission Checker for Snake Game (While Game is Running)
# Run this script while the game is running

Write-Host "=== Norton 360 Permission Check for Snake Game ===" -ForegroundColor Cyan
Write-Host ""

# Check if game is running and using ports
Write-Host "1. Checking if game is running on ports 47777 and 47778:" -ForegroundColor Yellow
$gameEndpoints = Get-NetUDPEndpoint | Where-Object {$_.LocalPort -eq 47777 -or $_.LocalPort -eq 47778}

if ($gameEndpoints) {
    Write-Host "   ? Game (Engine.exe) is running and listening on ports:" -ForegroundColor Green
    foreach ($endpoint in $gameEndpoints) {
        $process = Get-Process -Id $endpoint.OwningProcess -ErrorAction SilentlyContinue
        Write-Host "     - Port $($endpoint.LocalPort): $($process.Name) (PID: $($endpoint.OwningProcess))" -ForegroundColor Cyan
    }
} else {
    Write-Host "   ? Game is not running or not listening on required ports" -ForegroundColor Red
    Write-Host "   Please start the game first before running this check." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Press any key to exit..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit
}
Write-Host ""

# Check Norton processes
Write-Host "2. Checking Norton 360 status:" -ForegroundColor Yellow
$nortonProcesses = Get-Process | Where-Object {$_.Name -like "*Norton*"}
if ($nortonProcesses) {
    Write-Host "   ? Norton 360 is running" -ForegroundColor Green
    Write-Host "   Active Norton processes:" -ForegroundColor Cyan
    $nortonProcesses | Select-Object Name, Id | Format-Table -AutoSize
} else {
    Write-Host "   ? Norton 360 not detected" -ForegroundColor Gray
}
Write-Host ""

# Check Windows Firewall status
Write-Host "3. Checking Windows Firewall:" -ForegroundColor Yellow
$firewallProfiles = Get-NetFirewallProfile
foreach ($profile in $firewallProfiles) {
    $status = if ($profile.Enabled) { "Enabled" } else { "Disabled" }
    $color = if ($profile.Enabled) { "Yellow" } else { "Green" }
    Write-Host "   $($profile.Name) Profile: $status" -ForegroundColor $color
}
Write-Host ""

# Check for Snake game firewall rules
Write-Host "4. Checking Windows Firewall rules for Snake game:" -ForegroundColor Yellow
$snakeRules = Get-NetFirewallRule | Where-Object {
    $_.DisplayName -like "*Snake*" -or 
    $_.DisplayName -like "*47777*" -or 
    $_.DisplayName -like "*47778*"
}

if ($snakeRules) {
    Write-Host "   ? Found Windows Firewall rules:" -ForegroundColor Green
    $snakeRules | Select-Object DisplayName, Enabled, Direction, Action | Format-Table -AutoSize
} else {
    Write-Host "   ? No specific Windows Firewall rules found" -ForegroundColor Red
    Write-Host "   Windows Firewall may use default rules or block the connection." -ForegroundColor Yellow
}
Write-Host ""

# Check for Engine.exe firewall rules
Write-Host "5. Checking Windows Firewall rules for Engine.exe:" -ForegroundColor Yellow
$engineRules = Get-NetFirewallApplicationFilter | Where-Object {$_.Program -like "*Engine.exe*"}
if ($engineRules.Count -gt 0) {
    Write-Host "   ? Found rules for Engine.exe" -ForegroundColor Green
    foreach ($filter in $engineRules) {
        $rule = Get-NetFirewallRule | Where-Object {$_.Name -eq $filter.InstanceID}
        if ($rule) {
            Write-Host "     - $($rule.DisplayName): $($rule.Direction) - $($rule.Action) (Enabled: $($rule.Enabled))" -ForegroundColor Cyan
        }
    }
} else {
    Write-Host "   ? No specific rules found for Engine.exe" -ForegroundColor Red
}
Write-Host ""

# Test UDP broadcast capability
Write-Host "6. Testing UDP broadcast capability:" -ForegroundColor Yellow
Write-Host "   Attempting to send UDP broadcast on port 47777..." -ForegroundColor Gray

try {
    $udpClient = New-Object System.Net.Sockets.UdpClient
    $udpClient.EnableBroadcast = $true
    $udpClient.Client.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, 
                                      [System.Net.Sockets.SocketOptionName]::ReuseAddress, 
                                      $true)
    
    $broadcastAddress = [System.Net.IPAddress]::Broadcast
    $endpoint = New-Object System.Net.IPEndPoint($broadcastAddress, 47777)
    
    $message = [System.Text.Encoding]::ASCII.GetBytes("TEST")
    $bytesSent = $udpClient.Send($message, $message.Length, $endpoint)
    
    $udpClient.Close()
    
    Write-Host "   ? Successfully sent UDP broadcast ($bytesSent bytes)" -ForegroundColor Green
    Write-Host "   This means your network adapter can send broadcasts." -ForegroundColor Cyan
} catch {
    Write-Host "   ? Failed to send UDP broadcast: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "   This could indicate firewall or network issues." -ForegroundColor Yellow
}
Write-Host ""

# Check network adapters
Write-Host "7. Checking active network adapters:" -ForegroundColor Yellow
$adapters = Get-NetAdapter | Where-Object {$_.Status -eq "Up"}
if ($adapters) {
    Write-Host "   Active network connections:" -ForegroundColor Cyan
    $adapters | Select-Object Name, InterfaceDescription, Status, LinkSpeed | Format-Table -AutoSize
} else {
    Write-Host "   ? No active network adapters found" -ForegroundColor Red
}
Write-Host ""

# Get local IP addresses
Write-Host "8. Local IP addresses:" -ForegroundColor Yellow
$ipAddresses = Get-NetIPAddress | Where-Object {
    $_.AddressFamily -eq "IPv4" -and 
    $_.IPAddress -notlike "127.*" -and
    $_.PrefixOrigin -ne "WellKnown"
}
if ($ipAddresses) {
    Write-Host "   Your computer's IP addresses:" -ForegroundColor Cyan
    foreach ($ip in $ipAddresses) {
        Write-Host "     - $($ip.IPAddress) (Interface: $($ip.InterfaceAlias))" -ForegroundColor Green
    }
} else {
    Write-Host "   ? No local IP addresses found" -ForegroundColor Red
}
Write-Host ""

# Summary and recommendations
Write-Host "=== Summary and Recommendations ===" -ForegroundColor Cyan
Write-Host ""

$issues = @()
$recommendations = @()

if (-not $snakeRules) {
    $issues += "No Windows Firewall rules for Snake game"
    $recommendations += "Create Windows Firewall rules (run check_and_setup_ports.ps1 as Administrator)"
}

if ($nortonProcesses) {
    $issues += "Norton 360 is running and may block traffic"
    $recommendations += "Configure Norton 360 using Method 1 or 2 in MULTIPLAYER_README.md"
    $recommendations += "Test by temporarily disabling Norton Smart Firewall (Method 3)"
}

if ($issues.Count -eq 0) {
    Write-Host "? Configuration looks good!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Your game should be able to discover other players on the network." -ForegroundColor Green
    Write-Host "If you still can't connect, check:" -ForegroundColor Yellow
    Write-Host "  1. The other computer has the same configuration" -ForegroundColor Yellow
    Write-Host "  2. Both computers are on the same subnet" -ForegroundColor Yellow
    Write-Host "  3. Norton 360 is configured to allow the game" -ForegroundColor Yellow
} else {
    Write-Host "? Issues detected:" -ForegroundColor Yellow
    foreach ($issue in $issues) {
        Write-Host "  • $issue" -ForegroundColor Red
    }
    Write-Host ""
    Write-Host "?? Recommended actions:" -ForegroundColor Cyan
    $counter = 1
    foreach ($rec in $recommendations) {
        Write-Host "  $counter. $rec" -ForegroundColor Yellow
        $counter++
    }
}

Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
