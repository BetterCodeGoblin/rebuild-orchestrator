# Rebuild Orchestrator Distribution Checklist

Use this checklist before handing the plugin to another Unreal project or packaging it for a release zip.

## Release Baseline

- Version: `0.1.2`
- Engine target: Unreal Engine 5.8
- Verified project: StrengthERG on Windows
- Verified commands: `rebuild.status`, `rebuild.relaunch`, `rebuild.clean`, `rebuild.build`, `rebuild.full`

## Pre-Package

1. Sync the plugin source.
2. Confirm there are no local generated artifacts inside the plugin folder.
3. Confirm project-generated folders are not included:
   - `Saved/`
   - `Intermediate/`
   - `Binaries/`
   - `.vs/`
4. Confirm docs are present:
   - `INSTALL.txt`
   - `README.md`
   - `TESTING.md`
   - `DISTRIBUTION.md`

## Package Command

```powershell
& "D:\EPIC\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="D:\Perforce\StrengthERG\UE5 5.8\Plugins\RebuildOrchestrator\RebuildOrchestrator.uplugin" `
  -Package="D:\RebuildOrchestrator-Packages\RebuildOrchestrator-0.1.2-UE5.8" `
  -Rocket
```

## Clean-Project Verification

Install the packaged plugin into a separate UE 5.8 C++ project and verify:

1. Project regenerates files and builds.
2. Plugin loads without startup errors.
3. `rebuild.status` prints project, engine, and log paths.
4. `rebuild.relaunch` opens a second editor instance.
5. `rebuild.clean` deletes `Binaries/` and generated intermediate contents while preserving `Intermediate/ProjectFiles/`.
6. `rebuild.build` exits the editor and builds successfully.
7. `rebuild.full` exits, cleans, builds, and relaunches.

## Do Not Ship

- Project `Saved/` logs.
- Generated `RebuildOrchestrator-launch-*.cmd` files.
- Generated `RebuildOrchestrator-bootstrap-*.log` files.
- Project `Binaries/`, `Intermediate/`, or `.vs/` directories.
