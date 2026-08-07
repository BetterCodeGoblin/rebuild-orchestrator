# Rebuild Orchestrator

Rebuild Orchestrator is an editor-only Unreal Engine plugin for a safer C++ clean/build/relaunch loop.

It does not rebuild inside the running editor. Instead, it launches a fixed bundled script, asks the editor to exit, then lets the script wait for shutdown, clean/build through UBT, and relaunch UnrealEditor.

The first Windows dogfood pass is verified on UE 5.8 with StrengthERG: `rebuild.relaunch`, `rebuild.clean`, `rebuild.build`, and `rebuild.full` all completed successfully.

## Install

1. Drop this `RebuildOrchestrator` folder into `<Project>/Plugins/`.
2. Regenerate project files.
3. Build the project editor target.
4. Enable the plugin if Unreal does not enable it automatically.

## Commands

Open the Unreal console and run:

- `rebuild.full` - launches the external clean/build/relaunch script, then requests graceful editor shutdown.
- `rebuild.build` - launches the external build/relaunch script, then requests graceful editor shutdown.
- `rebuild.clean` - launches the external clean-only script path, then requests graceful editor shutdown.
- `rebuild.relaunch` - launches a second UnrealEditor instance for the current project.
- `rebuild.logs` - opens the plugin log folder and prints its path to the Unreal log.
- `rebuild.status` - prints project/engine paths plus the latest run, bootstrap, and launcher logs.

## MCP Toolset (AI agent control)

The stock Unreal MCP toolsets expose read/search tools but no generic
console-command execution, so an MCP-connected AI agent (Claude Code, Cursor,
etc.) can plan a rebuild but cannot fire it. This plugin ships a narrow Python
toolset that closes that gap for exactly the `rebuild.*` commands.

Files: `Content/Python/rebuild_orchestrator_toolset.py`, imported at editor
start-up by `Content/Python/init_unreal.py` (requires the `Unreal MCP` +
`All Toolsets` plugins to be enabled).

`init_unreal.py` is NOT optional. Unreal runs only that filename from a
plugin's `Content/Python/`; other modules in the folder are placed on
`sys.path` but never imported, so without the hook the `@unreal.uclass()`
decorator never runs and the module's `register()` function never hands the
toolset class to `unreal.ToolsetRegistry` (no error is logged -- it just
silently never appears).

Exposed MCP tools:

- `run_rebuild_command(command)` - runs one whitelisted `rebuild.*` command.
- `list_rebuild_commands()` - lists the allowed commands.

Only the six known `rebuild.*` commands are accepted; no arbitrary console
execution is exposed, matching the plugin's safety model.

### Activation (required once after install/update)

The toolset registers when Unreal imports `init_unreal.py`. That happens
automatically at editor start-up, so the simplest path is:

1. Enable the `Unreal MCP` and `All Toolsets` plugins.
2. **Restart the editor** (or launch it fresh). On start-up it runs
   `init_unreal.py`, which registers the toolset.
3. Connect your MCP agent. It should now see `run_rebuild_command` and
   `list_rebuild_commands`.

If the toolset was already registered and you only need MCP to republish the
current registry, run this in the editor console:

```text
ModelContextProtocol.RefreshTools
```

`RefreshTools` does not import Python modules or register missing toolsets. If
`run_rebuild_command` / `list_rebuild_commands` are absent after an install or
update, restart the editor so Unreal runs `init_unreal.py`.

> Bootstrap gotcha: `ModelContextProtocol.RefreshTools` is itself a console
> command, and running console commands is the exact gap this toolset fills.
> So an agent cannot use it to self-load a missing rebuild toolset. The FIRST
> activation after install/update should be an editor restart. For classroom
> setups, bake "restart the editor once after install" into the setup steps and
> this never surfaces.

> Note: `CanContainContent` is `true` so the plugin can ship `Content/Python/`.

## Toolbar

The Level Editor toolbar gets a `Full Rebuild & Relaunch` button. It triggers the same handler as `rebuild.full`.

## Logs

Logs are written under:

```text
<Project>/Saved/RebuildOrchestrator/Logs/
```

Each script-backed run creates:

- `RebuildOrchestrator-<mode>-<timestamp>.log` - PowerShell transcript / script log.
- `RebuildOrchestrator-bootstrap-<mode>-<timestamp>.log` - launcher bootstrap log.
- `RebuildOrchestrator-launch-<mode>-<timestamp>.cmd` - generated Windows launcher.

These files are diagnostic artifacts under `Saved/` and should not be shipped in a packaged plugin.

## Safety Model

- No arbitrary shell command execution.
- The plugin only launches its bundled `Scripts/FullRebuild.ps1` on Windows or `Scripts/FullRebuild.sh` on Mac/Linux.
- The mode argument is whitelisted to `full`, `build`, `clean`, or `relaunch`.
- Clean mode deletes project-local `Binaries/` and generated contents under `Intermediate/`, but preserves `Intermediate/ProjectFiles/` so existing Visual Studio project files keep loading.
- Clean/build/full modes fail closed if the editor does not exit before the script timeout.
- Engine directories are never deleted or modified by the clean step.
- Project, engine, and log paths are resolved dynamically at runtime through Unreal path APIs.

## Packaging

Package from a clean plugin folder. Do not include project `Saved/`, `Intermediate/`, `Binaries/`, generated logs, or generated launcher files.

Example UE 5.8 packaging command:

```powershell
& "D:\EPIC\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="D:\Perforce\StrengthERG\UE5 5.8\Plugins\RebuildOrchestrator\RebuildOrchestrator.uplugin" `
  -Package="D:\RebuildOrchestrator-Packages\RebuildOrchestrator-0.1.2-UE5.8" `
  -Rocket
```

After packaging, install into a clean UE 5.8 C++ project and verify `rebuild.status`, `rebuild.relaunch`, `rebuild.clean`, `rebuild.build`, and `rebuild.full`.

## Shutdown Timeout

`build`, `clean`, and `full` first wait for the editor to close (the in-editor
plugin requests a graceful shutdown via `RequestEngineExit`). The wait timeout
is **300 seconds** for all three modes. Graceful shutdown of a large, loaded
UE 5.8 project can take well over 30 seconds, so a short timeout (the old 30 s
default) would fail agent-driven and classroom rebuilds even though the editor
was closing normally. If a build still times out, the editor is genuinely stuck
-- wait for it to close and re-run, or check `rebuild.status` for a stray
`UnrealEditor.exe`.

## Caveats

- Windows UE 5.8 dogfood passed in StrengthERG.
- Mac/Linux scripts remain syntax-checked only until dogfooded on those platforms.
- The toolbar button is a thin first pass; console commands are currently the most reliable UX.

## Classroom-Hardening TODO

- Dynamic path resolution: DONE.
- Add a settings panel.
- Add a setup-check command for script presence, engine tools, permissions, and expected targets.
- Validate packaged-plugin install on a clean UE 5.8 project.
- Validate Mac/Linux behavior and process matching.
- Add per-student path/config support where lab machines differ.
