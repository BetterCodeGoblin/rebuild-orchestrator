#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IConsoleObject;

class FRebuildOrchestratorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterConsoleCommands();
    void UnregisterConsoleCommands();
    void RegisterMenus();

    void HandleFullRebuild();
    void HandleBuild();
    void HandleClean();
    void HandleRelaunch();
    void HandleLogs();
    void HandleStatus();

    bool LaunchRebuildScript(const FString& Mode);

    IConsoleObject* FullCommand = nullptr;
    IConsoleObject* BuildCommand = nullptr;
    IConsoleObject* CleanCommand = nullptr;
    IConsoleObject* RelaunchCommand = nullptr;
    IConsoleObject* LogsCommand = nullptr;
    IConsoleObject* StatusCommand = nullptr;
};
