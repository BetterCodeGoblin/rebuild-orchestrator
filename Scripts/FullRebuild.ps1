[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectPath,

    [Parameter(Mandatory = $true)]
    [string]$EngineDir,

    [Parameter(Mandatory = $true)]
    [ValidateSet("full", "build", "clean", "relaunch")]
    [string]$Mode,

    [Parameter(Mandatory = $true)]
    [string]$LogDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Status {
    param([string]$Message)
    Write-Host "[RebuildOrchestrator] $Message"
}

function Resolve-FullPath {
    param([string]$Path)
    return [System.IO.Path]::GetFullPath($Path)
}

function Wait-ForEditorExit {
    param(
        [string]$ProjectFullPath,
        [int]$TimeoutSeconds = 120
    )

    Write-Status "Waiting for UnrealEditor to exit for project: $ProjectFullPath"
    Write-Status "The in-editor plugin should already have requested a graceful shutdown."
    Write-Status "Timeout: $TimeoutSeconds s (graceful shutdown of a loaded editor can take a while)."

    $start = Get-Date
    $deadline = $start.AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $matches = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" -ErrorAction SilentlyContinue |
            Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($ProjectFullPath, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 })

        if ($matches.Count -eq 0) {
            $elapsed = [int]((Get-Date) - $start).TotalSeconds
            Write-Status "UnrealEditor has exited after $elapsed s. Continuing."
            return
        }

        $remaining = [int]($deadline - (Get-Date)).TotalSeconds
        Write-Status "Still waiting on UnrealEditor process id(s): $($matches.ProcessId -join ', ') ($remaining s left)"
        Start-Sleep -Seconds 2
    }

    throw "Timed out after $TimeoutSeconds s waiting for UnrealEditor to exit; refusing to continue while the editor may still be running. If the editor is mid-shutdown, wait for it to close and run the command again, or increase the timeout."
}

function Invoke-Clean {
    param([string]$ProjectDirectory)

    $binariesDir = Join-Path $ProjectDirectory "Binaries"
    if (Test-Path -LiteralPath $binariesDir) {
        Write-Status "Deleting project-local $binariesDir"
        Remove-Item -LiteralPath $binariesDir -Recurse -Force
    } else {
        Write-Status "Skipping missing $binariesDir"
    }

    $intermediateDir = Join-Path $ProjectDirectory "Intermediate"
    if (-not (Test-Path -LiteralPath $intermediateDir)) {
        Write-Status "Skipping missing $intermediateDir"
        return
    }

    Write-Status "Deleting project-local Intermediate contents, preserving ProjectFiles"
    foreach ($child in @(Get-ChildItem -LiteralPath $intermediateDir -Force)) {
        if ($child.Name -eq "ProjectFiles") {
            Write-Status "Preserving $($child.FullName)"
            continue
        }

        Write-Status "Deleting project-local $($child.FullName)"
        Remove-Item -LiteralPath $child.FullName -Recurse -Force
    }
}

function Invoke-Build {
    param(
        [string]$ProjectFullPath,
        [string]$EngineFullDir
    )

    $projectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFullPath)
    $buildBat = Join-Path $EngineFullDir "Build/BatchFiles/Build.bat"

    if (-not (Test-Path -LiteralPath $buildBat)) {
        throw "Build.bat not found at $buildBat"
    }

    Write-Status "Building $($projectName)Editor Win64 Development"
    & $buildBat "$($projectName)Editor" "Win64" "Development" "-Project=$ProjectFullPath" "-WaitMutex" "-FromMsBuild"
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "Build failed with exit code $exitCode"
    }
}

function Invoke-Relaunch {
    param(
        [string]$ProjectFullPath,
        [string]$EngineFullDir
    )

    $editorExe = Join-Path $EngineFullDir "Binaries/Win64/UnrealEditor.exe"
    if (-not (Test-Path -LiteralPath $editorExe)) {
        throw "UnrealEditor.exe not found at $editorExe"
    }

    Write-Status "Relaunching UnrealEditor: $editorExe $ProjectFullPath"
    $projectDirectory = Split-Path -Parent $ProjectFullPath
    $process = Start-Process -FilePath $editorExe -ArgumentList ('"{0}"' -f $ProjectFullPath) -WorkingDirectory $projectDirectory -PassThru
    if (-not $process) {
        throw "Start-Process did not return a process for UnrealEditor relaunch."
    }

    Write-Status "Relaunched UnrealEditor process id: $($process.Id)"
    Start-Sleep -Seconds 3
    $process.Refresh()
    if ($process.HasExited) {
        throw "Relaunched UnrealEditor exited immediately with code $($process.ExitCode)."
    }
}

$ProjectPath = Resolve-FullPath $ProjectPath
$EngineDir = Resolve-FullPath $EngineDir
$ProjectDir = Split-Path -Parent $ProjectPath
$LogDir = Resolve-FullPath $LogDir

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logPath = Join-Path $LogDir "RebuildOrchestrator-$Mode-$timestamp.log"

Start-Transcript -Path $logPath -Force | Out-Null
try {
    Write-Status "Mode: $Mode"
    Write-Status "ProjectPath: $ProjectPath"
    Write-Status "EngineDir: $EngineDir"
    Write-Status "Log: $logPath"

    if (-not (Test-Path -LiteralPath $ProjectPath)) {
        throw "Project file not found: $ProjectPath"
    }
    if (-not (Test-Path -LiteralPath $EngineDir)) {
        throw "Engine directory not found: $EngineDir"
    }

    switch ($Mode) {
        "clean" {
            Wait-ForEditorExit -ProjectFullPath $ProjectPath -TimeoutSeconds 300
            Invoke-Clean -ProjectDirectory $ProjectDir
        }
        "build" {
            Wait-ForEditorExit -ProjectFullPath $ProjectPath -TimeoutSeconds 300
            Invoke-Build -ProjectFullPath $ProjectPath -EngineFullDir $EngineDir
            Invoke-Relaunch -ProjectFullPath $ProjectPath -EngineFullDir $EngineDir
        }
        "relaunch" {
            Invoke-Relaunch -ProjectFullPath $ProjectPath -EngineFullDir $EngineDir
        }
        "full" {
            Wait-ForEditorExit -ProjectFullPath $ProjectPath -TimeoutSeconds 300
            Invoke-Clean -ProjectDirectory $ProjectDir
            Invoke-Build -ProjectFullPath $ProjectPath -EngineFullDir $EngineDir
            Invoke-Relaunch -ProjectFullPath $ProjectPath -EngineFullDir $EngineDir
        }
    }

    Write-Status "Completed successfully."
    exit 0
} catch {
    Write-Status "FAILED: $($_.Exception.Message)"
    exit 1
} finally {
    Stop-Transcript | Out-Null
}
