# Rebuild Orchestrator Test Guide

This guide is for dogfooding the `RebuildOrchestrator` Unreal editor plugin in StrengthERG.

The goal is not to prove this is classroom-ready yet. The goal is to prove the thin slice can:

1. Load in Unreal.
2. Register its console commands.
3. Open its log folder.
4. Launch the external rebuild script.
5. Exit the editor.
6. Clean project-local generated folders.
7. Build the editor target externally.
8. Relaunch the project.

## Safety Rules

- Test on a synced Perforce workspace with no unsaved Unreal assets.
- Submit or shelve unrelated work before testing.
- Do not run `rebuild.full` while maps, Blueprints, or assets have unsaved changes.
- Expect Unreal to close during `rebuild.full`, `rebuild.build`, and `rebuild.clean`.
- The plugin should only delete these project-local folders:
  - `Binaries/`
  - generated contents under `Intermediate/`, while preserving `Intermediate/ProjectFiles/`
- It must not delete `Content/`, `Config/`, `Source/`, engine folders, or Perforce metadata.

## Install Location

For StrengthERG UE 5.8, place the plugin here:

```text
<StrengthERG workspace>/UE5 5.8/Plugins/RebuildOrchestrator/
```

Expected project file:

```text
<StrengthERG workspace>/UE5 5.8/StrengthERG.uproject
```

## Preflight

1. Sync the workspace.
2. Confirm no files are open that you care about:

```powershell
p4 opened
```

3. Confirm the project opens normally before testing the plugin.
4. Close Unreal.
5. Regenerate project files for `StrengthERG.uproject`.
6. Build the `StrengthERGEditor` target from Visual Studio or Unreal's generated build files.

If the plugin fails to compile, check these likely API assumptions first:

- `UToolMenus::RegisterStartupCallback`
- `UToolMenus::UnRegisterStartupCallback`
- `LevelEditor.LevelEditorToolBar`
- `RequestEngineExit(TEXT(...))`
- `FPlatformProcess::CreateProc(...)`

## Test 1 - Plugin Loads

1. Open `StrengthERG.uproject`.
2. Go to `Edit > Plugins`.
3. Search for `Rebuild Orchestrator`.
4. Confirm it is enabled.
5. Restart Unreal if prompted.

Pass condition:

- Unreal opens without a plugin load error.

## Test 2 - Console Commands Exist

Open the Unreal console and try:

```text
rebuild.logs
rebuild.status
```

Pass condition:

- Unreal logs the Rebuild Orchestrator log folder.
- The folder opens in Explorer:

```text
<Project>/Saved/RebuildOrchestrator/Logs/
```
- `rebuild.status` prints the project path, engine path, log path, and latest log artifacts.

## Test 3 - Relaunch Only

Run:

```text
rebuild.relaunch
```

Pass condition:

- A second Unreal Editor process starts for the same project.
- No files are cleaned.
- No build is run.

After the test, close the extra editor instance before continuing.

## Test 4 - Clean Only

Only run this after closing/saving unrelated work.

Run:

```text
rebuild.clean
```

Expected behavior:

1. Plugin launches the script.
2. Plugin requests graceful editor shutdown.
3. Script waits for the matching Unreal Editor process to exit.
4. Script deletes only:
   - `<Project>/Binaries/`
   - generated contents under `<Project>/Intermediate/`, while preserving `<Project>/Intermediate/ProjectFiles/`
5. Script writes a log file under `Saved/RebuildOrchestrator/Logs/`.

Pass condition:

- Unreal closes.
- `Binaries/` and generated intermediate contents are removed.
- `Intermediate/ProjectFiles/` remains if it existed.
- The latest log ends with `Completed successfully.`

Fail condition:

- Anything under `Content/`, `Config/`, `Source/`, or engine directories is touched.
- The script continues after Unreal refuses to exit.

## Test 5 - Build Only

Run:

```text
rebuild.build
```

Expected behavior:

1. Plugin launches the script.
2. Plugin requests graceful editor shutdown.
3. Script waits for Unreal to exit.
4. Script calls:

```text
<EngineDir>/Build/BatchFiles/Build.bat StrengthERGEditor Win64 Development -Project=<ProjectPath> -WaitMutex -FromMsBuild
```

Pass condition:

- Unreal closes.
- `StrengthERGEditor` builds successfully.
- The log ends with `Completed successfully.`

## Test 6 - Full Rebuild And Relaunch

This is the main product test.

Run:

```text
rebuild.full
```

Expected behavior:

1. Plugin launches the script.
2. Plugin requests graceful editor shutdown.
3. Script waits for Unreal to exit.
4. Script deletes project-local `Binaries/` and `Intermediate/`.
5. Script builds `StrengthERGEditor Win64 Development`.
6. Script relaunches Unreal Editor with `StrengthERG.uproject`.
7. Script writes a timestamped log.

Pass condition:

- Unreal closes cleanly.
- The external build succeeds.
- Unreal relaunches into StrengthERG.
- The latest log ends with `Completed successfully.`

## What To Capture

For the first real dogfood run, save:

- Screenshot of the plugin enabled in `Edit > Plugins`.
- Screenshot or copied output of `rebuild.logs`.
- The latest `Saved/RebuildOrchestrator/Logs/RebuildOrchestrator-full-*.log`.
- Any compile errors from Visual Studio or Unreal.
- Whether the toolbar button appears.
- Whether the toolbar button works the same as `rebuild.full`.

## Verified Windows Dogfood

Verified on SANS / StrengthERG / UE 5.8 through changelist 132:

- `rebuild.relaunch` opened a second editor.
- `rebuild.clean` waited for Unreal to exit, deleted `Binaries/` and generated intermediate contents, and completed successfully.
- `rebuild.build` waited for Unreal to exit, ran UBT/Build.bat, and completed successfully.
- `rebuild.full` closed Unreal, cleaned, built, and relaunched Unreal.

## Remaining Risks

- The toolbar button still needs a UX pass and should be tested as the main workflow.
- Packaged-plugin installation has not yet been validated in a clean project.
- Mac/Linux behavior and process matching are not dogfooded yet.
- Classroom hardening still needs a setup-check command and a friendlier error surface.

## Current Verdict

Windows UE 5.8 project-local distribution candidate. It is still intentionally narrow, but the safety model is right: fixed commands, bundled scripts, project-local cleanup, external build, relaunch, and logs.
