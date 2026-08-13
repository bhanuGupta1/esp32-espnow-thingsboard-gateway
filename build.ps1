<#
  build.ps1 - thin wrapper around the arduino-cli that ships inside Arduino IDE 2.

  The CLI is bundled with the IDE and is not on PATH, so every command would
  otherwise start with a 90-character quoted path. It shares the IDE's data
  directory and sketchbook, so anything it does is visible in the IDE too.

  Examples:
    .\build.ps1 -Ports                      list attached boards and COM ports
    .\build.ps1 -All                        compile every sketch
    .\build.ps1 -Sketch 01                  compile just sketch 01
    .\build.ps1 -Sketch 01 -Upload -Port COM3
    .\build.ps1 -Monitor -Port COM3 -Seconds 20
    .\build.ps1 -Sketch 01 -Upload -Monitor -Port COM3

  Only ever connect ONE board while uploading. With two attached it is very easy
  to flash the gateway code onto the sensor node.
#>

[CmdletBinding()]
param(
  [string]$Sketch,
  [string]$Port,
  [switch]$All,
  [switch]$Upload,
  [switch]$Monitor,
  [switch]$Ports,
  [switch]$NoReset,
  [int]$Seconds = 20,
  [int]$Baud = 115200,
  [int]$UploadSpeed = 0
)

$Cli  = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$Fqbn = "esp32:esp32:esp32"
$Root = $PSScriptRoot

if (-not (Test-Path $Cli)) {
  Write-Host "arduino-cli not found at:" -ForegroundColor Red
  Write-Host "  $Cli"
  Write-Host "Install Arduino IDE 2, or edit `$Cli at the top of this script."
  exit 1
}

$script:failures = 0

function Get-AllSketchDirs {
  Get-ChildItem -Directory $Root | Where-Object { $_.Name -match '^\d\d_' } | Sort-Object Name
}

function Resolve-SketchDir {
  param([string]$Name)
  $all = Get-AllSketchDirs
  $hit = $all | Where-Object { $_.Name -eq $Name }
  if (-not $hit) { $hit = $all | Where-Object { $_.Name -like "$Name*" } }
  if (-not $hit) {
    Write-Host "No sketch matches '$Name'. Available:" -ForegroundColor Red
    $all | ForEach-Object { Write-Host "  $($_.Name)" }
    exit 1
  }
  if (@($hit).Count -gt 1) {
    Write-Host "'$Name' is ambiguous:" -ForegroundColor Red
    $hit | ForEach-Object { Write-Host "  $($_.Name)" }
    exit 1
  }
  return @($hit)[0].FullName
}

function Invoke-CompileDir {
  param([string]$Dir)
  Write-Host ""
  Write-Host "==> compile $(Split-Path $Dir -Leaf)" -ForegroundColor Cyan
  & $Cli compile --fqbn $Fqbn $Dir
  if ($LASTEXITCODE -ne 0) {
    $script:failures++
    Write-Host "    FAILED" -ForegroundColor Red
  } else {
    Write-Host "    OK" -ForegroundColor Green
  }
}

function Invoke-UploadDir {
  param([string]$Dir, [string]$SerialPort)
  if (-not $SerialPort) {
    Write-Host "-Upload needs -Port, for example -Port COM3. Run -Ports to find it." -ForegroundColor Red
    exit 1
  }
  $target = $Fqbn
  if ($UploadSpeed -gt 0) { $target = "$Fqbn`:UploadSpeed=$UploadSpeed" }

  Write-Host ""
  Write-Host "==> upload $(Split-Path $Dir -Leaf) to $SerialPort" -ForegroundColor Cyan
  & $Cli upload -p $SerialPort --fqbn $target $Dir
  if ($LASTEXITCODE -ne 0) {
    $script:failures++
    Write-Host "    UPLOAD FAILED" -ForegroundColor Red
    Write-Host "    If it stalled on 'Connecting...', hold the BOOT button while it retries."
    Write-Host "    If it fails partway through, retry with -UploadSpeed 115200."
  } else {
    Write-Host "    UPLOADED" -ForegroundColor Green
  }
}

function Read-SerialFor {
  param([string]$SerialPort, [int]$BaudRate, [int]$DurationSeconds)
  if (-not $SerialPort) {
    Write-Host "-Monitor needs -Port, for example -Port COM3." -ForegroundColor Red
    exit 1
  }

  # COM10 and above need the extended device path to open reliably.
  $devicePath = $SerialPort
  if ($SerialPort -match '^COM\d{2,}$') { $devicePath = "\\.\$SerialPort" }

  Write-Host ""
  Write-Host "==> reading $SerialPort at $BaudRate for $DurationSeconds s" -ForegroundColor Cyan

  $sp = New-Object System.IO.Ports.SerialPort $devicePath, $BaudRate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
  $sp.ReadTimeout = 400
  $sp.NewLine = "`n"

  # DTR stays de-asserted so GPIO0 is left high and the board boots the sketch
  # instead of dropping into the ROM bootloader. A short RTS pulse drives EN low,
  # which resets the board, so the startup banner is captured rather than missed.
  $sp.DtrEnable = $false
  $sp.RtsEnable = $false

  try {
    $sp.Open()
  } catch {
    Write-Host "Could not open $SerialPort : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Close any Serial Monitor that already has the port open." -ForegroundColor Yellow
    exit 1
  }

  # -NoReset attaches to a board that is already running, leaving its state
  # intact. Needed when the thing being observed is accumulated state -- node
  # liveness, sequence numbers, counters -- that a reset would erase.
  if ($NoReset) {
    Write-Host "    (attaching without reset, board keeps running)" -ForegroundColor DarkGray
  } else {
    $sp.RtsEnable = $true
    Start-Sleep -Milliseconds 120
    $sp.RtsEnable = $false
  }

  $lines    = 0
  $deadline = (Get-Date).AddSeconds($DurationSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      $line = $sp.ReadLine()
      Write-Output $line.TrimEnd()
      $lines++
    } catch {
      # ReadTimeout expiring is the normal idle case, not an error.
    }
  }

  $sp.Close()
  $sp.Dispose()

  Write-Host ""
  Write-Host "==> $lines line(s) captured" -ForegroundColor Cyan
  if ($lines -eq 0) {
    Write-Host "No output. Check the baud rate is $BaudRate and that a sketch is flashed." -ForegroundColor Yellow
  }
}

# ---------------------------------------------------------------------------

if ($Ports) {
  Write-Host "==> attached boards" -ForegroundColor Cyan
  & $Cli board list
  Write-Host ""
  Write-Host "==> COM ports known to Windows" -ForegroundColor Cyan
  $names = [System.IO.Ports.SerialPort]::GetPortNames()
  if ($names.Count -eq 0) {
    Write-Host "  none. If a board is plugged in, the USB-serial driver is missing." -ForegroundColor Yellow
    Write-Host "  Check Device Manager for an unknown device, then install the CH340 or CP2102 driver."
  } else {
    $names | ForEach-Object { Write-Host "  $_" }
  }
  exit 0
}

if (-not $All -and -not $Sketch -and -not $Monitor) {
  Write-Host "Nothing to do. Try:" -ForegroundColor Yellow
  Write-Host "  .\build.ps1 -Ports"
  Write-Host "  .\build.ps1 -All"
  Write-Host "  .\build.ps1 -Sketch 01 -Upload -Monitor -Port COM3"
  exit 1
}

if ($All) {
  Get-AllSketchDirs | ForEach-Object { Invoke-CompileDir $_.FullName }
} elseif ($Sketch) {
  $dir = Resolve-SketchDir $Sketch
  Invoke-CompileDir $dir
  if ($Upload -and $script:failures -eq 0) { Invoke-UploadDir $dir $Port }
}

if ($Monitor -and $script:failures -eq 0) {
  Read-SerialFor $Port $Baud $Seconds
}

Write-Host ""
if ($script:failures -eq 0) {
  Write-Host "All steps succeeded." -ForegroundColor Green
  exit 0
}
Write-Host "$($script:failures) step(s) failed." -ForegroundColor Red
exit 1
