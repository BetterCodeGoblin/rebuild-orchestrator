#include "RebuildOrchestratorModule.h"

#include "Framework/Commands/UIAction.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY_STATIC(LogRebuildOrchestrator, Log, All);

namespace
{
    FString QuoteArg(const FString& Value)
    {
        return FString::Printf(TEXT("\"%s\""), *Value.Replace(TEXT("\""), TEXT("\\\"")));
    }

    bool IsWhitelistedMode(const FString& Mode)
    {
        return Mode == TEXT("full") || Mode == TEXT("build") || Mode == TEXT("clean") || Mode == TEXT("relaunch");
    }

    FString FindLatestFile(const FString& Directory, const FString& Pattern, const FString& ExcludedPrefix = FString())
    {
        TArray<FString> FileNames;
        IFileManager::Get().FindFiles(FileNames, *FPaths::Combine(Directory, Pattern), true, false);

        FDateTime LatestTime = FDateTime::MinValue();
        FString LatestPath;
        for (const FString& FileName : FileNames)
        {
            if (!ExcludedPrefix.IsEmpty() && FileName.StartsWith(ExcludedPrefix))
            {
                continue;
            }

            const FString CandidatePath = FPaths::Combine(Directory, FileName);
            const FDateTime CandidateTime = IFileManager::Get().GetTimeStamp(*CandidatePath);
            if (LatestPath.IsEmpty() || CandidateTime > LatestTime)
            {
                LatestTime = CandidateTime;
                LatestPath = CandidatePath;
            }
        }

        return LatestPath;
    }
}

void FRebuildOrchestratorModule::StartupModule()
{
    RegisterConsoleCommands();

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRebuildOrchestratorModule::RegisterMenus));
    }
}

void FRebuildOrchestratorModule::ShutdownModule()
{
    UnregisterConsoleCommands();

    if (UObjectInitialized() && UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }
}

void FRebuildOrchestratorModule::RegisterConsoleCommands()
{
    IConsoleManager& ConsoleManager = IConsoleManager::Get();

    FullCommand = ConsoleManager.RegisterConsoleCommand(
        TEXT("rebuild.full"),
        TEXT("Launch the external full rebuild script, close the editor, rebuild, and relaunch."),
        FConsoleCommandDelegate::CreateRaw(this, &FRebuildOrchestratorModule::HandleFullRebuild),
        ECVF_Default);

    BuildCommand = ConsoleManager.RegisterConsoleCommand(
        TEXT("rebuild.build"),
        TEXT("Launch the external project editor build script."),
        FConsoleCommandDelegate::CreateRaw(this, &FRebuildOrchestratorModule::HandleBuild),
        ECVF_Default);

    CleanCommand = ConsoleManager.RegisterConsoleCommand(
        TEXT("rebuild.clean"),
        TEXT("Launch the external project clean script."),
        FConsoleCommandDelegate::CreateRaw(this, &FRebuildOrchestratorModule::HandleClean),
        ECVF_Default);

    RelaunchCommand = ConsoleManager.RegisterConsoleCommand(
        TEXT("rebuild.relaunch"),
        TEXT("Launch the external editor relaunch script."),
        FConsoleCommandDelegate::CreateRaw(this, &FRebuildOrchestratorModule::HandleRelaunch),
        ECVF_Default);

    LogsCommand = ConsoleManager.RegisterConsoleCommand(
        TEXT("rebuild.logs"),
        TEXT("Open the Rebuild Orchestrator log directory."),
        FConsoleCommandDelegate::CreateRaw(this, &FRebuildOrchestratorModule::HandleLogs),
        ECVF_Default);

    StatusCommand = ConsoleManager.RegisterConsoleCommand(
        TEXT("rebuild.status"),
        TEXT("Print Rebuild Orchestrator project paths and latest log files."),
        FConsoleCommandDelegate::CreateRaw(this, &FRebuildOrchestratorModule::HandleStatus),
        ECVF_Default);
}

void FRebuildOrchestratorModule::UnregisterConsoleCommands()
{
    IConsoleManager& ConsoleManager = IConsoleManager::Get();

    if (FullCommand)
    {
        ConsoleManager.UnregisterConsoleObject(FullCommand);
        FullCommand = nullptr;
    }
    if (BuildCommand)
    {
        ConsoleManager.UnregisterConsoleObject(BuildCommand);
        BuildCommand = nullptr;
    }
    if (CleanCommand)
    {
        ConsoleManager.UnregisterConsoleObject(CleanCommand);
        CleanCommand = nullptr;
    }
    if (RelaunchCommand)
    {
        ConsoleManager.UnregisterConsoleObject(RelaunchCommand);
        RelaunchCommand = nullptr;
    }
    if (LogsCommand)
    {
        ConsoleManager.UnregisterConsoleObject(LogsCommand);
        LogsCommand = nullptr;
    }
    if (StatusCommand)
    {
        ConsoleManager.UnregisterConsoleObject(StatusCommand);
        StatusCommand = nullptr;
    }
}

void FRebuildOrchestratorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar"));
    FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("Settings"));

    Section.AddEntry(FToolMenuEntry::InitToolBarButton(
        TEXT("RebuildOrchestratorFullRebuild"),
        FUIAction(FExecuteAction::CreateRaw(this, &FRebuildOrchestratorModule::HandleFullRebuild)),
        FText::FromString(TEXT("Full Rebuild & Relaunch")),
        FText::FromString(TEXT("Launches the external rebuild script, exits the editor, rebuilds, and relaunches."))));
}

void FRebuildOrchestratorModule::HandleFullRebuild()
{
    if (LaunchRebuildScript(TEXT("full")))
    {
        UE_LOG(LogRebuildOrchestrator, Display, TEXT("External full rebuild launched. Requesting graceful editor shutdown."));

        /*
         * The external script is responsible for waiting for editor shutdown,
         * rebuilding through UBT/Build.bat or Build.sh, and relaunching UnrealEditor.
         */
        RequestEngineExit(TEXT("RebuildOrchestrator full rebuild"));
    }
}

void FRebuildOrchestratorModule::HandleBuild()
{
    if (LaunchRebuildScript(TEXT("build")))
    {
        UE_LOG(LogRebuildOrchestrator, Display, TEXT("External build launched. Requesting graceful editor shutdown."));
        RequestEngineExit(TEXT("RebuildOrchestrator build"));
    }
}

void FRebuildOrchestratorModule::HandleClean()
{
    if (LaunchRebuildScript(TEXT("clean")))
    {
        UE_LOG(LogRebuildOrchestrator, Display, TEXT("External clean launched. Requesting graceful editor shutdown."));
        RequestEngineExit(TEXT("RebuildOrchestrator clean"));
    }
}

void FRebuildOrchestratorModule::HandleRelaunch()
{
    const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
    const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    const FString EditorExecutable = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/Win64/UnrealEditor.exe")));

    if (!FPaths::FileExists(EditorExecutable))
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("UnrealEditor.exe not found: %s"), *EditorExecutable);
        return;
    }

    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Launching UnrealEditor directly: %s %s"), *EditorExecutable, *ProjectFilePath);

    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *EditorExecutable,
        *QuoteArg(ProjectFilePath),
        true,
        false,
        false,
        nullptr,
        0,
        *ProjectDir,
        nullptr,
        nullptr);

    if (!ProcessHandle.IsValid())
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("Failed to launch UnrealEditor directly: %s %s"), *EditorExecutable, *ProjectFilePath);
        return;
    }

    FPlatformProcess::CloseProc(ProcessHandle);
}

void FRebuildOrchestratorModule::HandleLogs()
{
    const FString LogDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RebuildOrchestrator/Logs/")));
    IFileManager::Get().MakeDirectory(*LogDir, true);

    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Rebuild Orchestrator logs: %s"), *LogDir);
    FPlatformProcess::ExploreFolder(*LogDir);
}

void FRebuildOrchestratorModule::HandleStatus()
{
    const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
    const FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    const FString LogDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RebuildOrchestrator/Logs/")));

    IFileManager::Get().MakeDirectory(*LogDir, true);

    const FString LatestRunLog = FindLatestFile(LogDir, TEXT("RebuildOrchestrator-*.log"), TEXT("RebuildOrchestrator-bootstrap-"));
    const FString LatestBootstrapLog = FindLatestFile(LogDir, TEXT("RebuildOrchestrator-bootstrap-*.log"));
    const FString LatestLauncher = FindLatestFile(LogDir, TEXT("RebuildOrchestrator-launch-*.cmd"));

    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Rebuild Orchestrator status"));
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Project: %s"), *ProjectFilePath);
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Engine: %s"), *EngineDir);
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Logs: %s"), *LogDir);
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Latest run log: %s"), LatestRunLog.IsEmpty() ? TEXT("(none)") : *LatestRunLog);
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Latest bootstrap log: %s"), LatestBootstrapLog.IsEmpty() ? TEXT("(none)") : *LatestBootstrapLog);
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Latest launcher: %s"), LatestLauncher.IsEmpty() ? TEXT("(none)") : *LatestLauncher);
}

bool FRebuildOrchestratorModule::LaunchRebuildScript(const FString& Mode)
{
    // Safety: this plugin never executes arbitrary shell text. It only launches the bundled script with a whitelisted mode.
    if (!IsWhitelistedMode(Mode))
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("Rejected unsupported rebuild mode: %s"), *Mode);
        return false;
    }

    const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
    const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    const FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    const FString LogDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RebuildOrchestrator/Logs/")));

    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RebuildOrchestrator"));
    if (!Plugin.IsValid())
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("Unable to locate RebuildOrchestrator plugin base directory."));
        return false;
    }

    IFileManager::Get().MakeDirectory(*LogDir, true);

#if PLATFORM_WINDOWS
    const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Scripts/FullRebuild.ps1")));
    const FString PowerShellExecutable = TEXT("C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe");
    const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
    const FString BootstrapLogPath = FPaths::Combine(LogDir, FString::Printf(TEXT("RebuildOrchestrator-bootstrap-%s-%s.log"), *Mode, *Timestamp));
    const FString LauncherPath = FPaths::Combine(LogDir, FString::Printf(TEXT("RebuildOrchestrator-launch-%s-%s.cmd"), *Mode, *Timestamp));
    const FString LauncherContents = FString::Printf(
        TEXT("@echo off\r\n")
        TEXT("setlocal\r\n")
        TEXT("echo [RebuildOrchestrator] launcher started %%DATE%% %%TIME%% > %s\r\n")
        TEXT("echo PowerShell: %s >> %s\r\n")
        TEXT("%s -NoProfile -ExecutionPolicy Bypass -File %s -ProjectPath %s -EngineDir %s -Mode %s -LogDir %s >> %s 2>&1\r\n")
        TEXT("set EXITCODE=%%ERRORLEVEL%%\r\n")
        TEXT("echo [RebuildOrchestrator] PowerShell exit code: %%EXITCODE%% >> %s\r\n")
        TEXT("exit /b %%EXITCODE%%\r\n"),
        *QuoteArg(BootstrapLogPath),
        *QuoteArg(PowerShellExecutable),
        *QuoteArg(BootstrapLogPath),
        *QuoteArg(PowerShellExecutable),
        *QuoteArg(ScriptPath),
        *QuoteArg(ProjectFilePath),
        *QuoteArg(EngineDir),
        *QuoteArg(Mode),
        *QuoteArg(LogDir),
        *QuoteArg(BootstrapLogPath),
        *QuoteArg(BootstrapLogPath));

    if (!FFileHelper::SaveStringToFile(LauncherContents, *LauncherPath))
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("Failed to write rebuild launcher: %s"), *LauncherPath);
        return false;
    }
#else
    const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Scripts/FullRebuild.sh")));
    const FString Executable = TEXT("/bin/bash");
    const FString Arguments = FString::Printf(
        TEXT("%s %s %s %s %s"),
        *QuoteArg(ScriptPath),
        *QuoteArg(ProjectFilePath),
        *QuoteArg(EngineDir),
        *QuoteArg(Mode),
        *QuoteArg(LogDir));
#endif

    if (!FPaths::FileExists(ScriptPath))
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("Rebuild script not found: %s"), *ScriptPath);
        return false;
    }

    UE_LOG(
        LogRebuildOrchestrator,
        Display,
        TEXT("Launching Rebuild Orchestrator mode '%s' for project '%s' from project dir '%s'. Logs: %s"),
        *Mode,
        *ProjectFilePath,
        *ProjectDir,
        *LogDir);

#if PLATFORM_WINDOWS
    UE_LOG(LogRebuildOrchestrator, Display, TEXT("Rebuild Orchestrator launcher: %s"), *LauncherPath);
    FPlatformProcess::LaunchFileInDefaultExternalApplication(*LauncherPath);
    return true;
#endif

#if !PLATFORM_WINDOWS
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *Executable,
        *Arguments,
        true,
        false,
        false,
        nullptr,
        0,
        *ProjectDir,
        nullptr,
        nullptr);

    if (!ProcessHandle.IsValid())
    {
        UE_LOG(LogRebuildOrchestrator, Error, TEXT("Failed to launch rebuild script: %s %s"), *Executable, *Arguments);
        return false;
    }

    FPlatformProcess::CloseProc(ProcessHandle);
    return true;
#endif
}

IMPLEMENT_MODULE(FRebuildOrchestratorModule, RebuildOrchestrator)
