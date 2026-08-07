"""Rebuild Orchestrator MCP toolset.

Exposes a safe, whitelisted way for an MCP-connected AI agent to trigger the
Rebuild Orchestrator console commands (rebuild.build / rebuild.clean /
rebuild.full / rebuild.relaunch / rebuild.logs / rebuild.status) from inside
the running Unreal Editor.

The stock Unreal MCP toolsets that ship with the engine expose read/search
tools but no generic console-command execution. Without a tool like this, an
agent can plan a rebuild but cannot fire it -- the user has to type the console
command by hand. This toolset closes that gap for exactly the Rebuild
Orchestrator commands, and nothing else.

Design constraints (kept deliberately narrow for classroom / multi-machine use):
  * Only the six known `rebuild.*` commands are accepted. No arbitrary console
    execution is exposed, matching the plugin's existing safety model.
  * Commands run on the game thread via the editor console executor.
  * Registered at editor start-up by the sibling Content/Python/init_unreal.py,
    which imports this module and calls register(). Unreal only auto-runs
    init_unreal.py from a plugin's Content/Python/, so a bare module here would
    never be imported on its own. The @unreal.uclass() decorator alone only
    creates the UClass; it does not add the toolset to the Toolset Registry.

Picking up edits to this file requires an editor restart.
ModelContextProtocol.RefreshTools is NOT sufficient: it only re-publishes the
toolsets already in the registry and never re-imports Python.
"""

import unreal
import toolset_registry


# Whitelist mirrors RebuildOrchestratorModule.cpp IsWhitelistedMode + the
# read-only helper commands. Keep this in sync with the registered console
# commands if new ones are added.
_ALLOWED_COMMANDS = {
    "rebuild.full",
    "rebuild.build",
    "rebuild.clean",
    "rebuild.relaunch",
    "rebuild.logs",
    "rebuild.status",
}


@unreal.uclass()
class RebuildOrchestratorToolset(unreal.ToolsetDefinition):
    """Tools for driving the Rebuild Orchestrator clean/build/relaunch loop.

    These tools let an AI agent trigger the project's whitelisted rebuild
    console commands from inside the editor. Only the fixed set of
    `rebuild.*` commands is permitted; no arbitrary console execution is
    exposed.
    """

    @toolset_registry.tool_call
    @staticmethod
    def run_rebuild_command(command: str) -> str:
        """Run one whitelisted Rebuild Orchestrator console command.

        Use this to compile C++ changes without asking the user to type the
        console command by hand. `rebuild.build` requests a graceful editor
        shutdown, builds through UBT, and relaunches the editor. Use
        `rebuild.full` when a clean build is needed. Check `rebuild.status` if
        a build fails with a file lock.

        Before triggering `rebuild.build`, `rebuild.clean`, or `rebuild.full`,
        confirm PIE is not running and no asset editors have unsaved work,
        because these request an editor shutdown.

        Args:
            command: The console command to run. Must be one of:
                rebuild.full, rebuild.build, rebuild.clean,
                rebuild.relaunch, rebuild.logs, rebuild.status.

        Returns:
            A short status string describing what was dispatched, or an error
            message if the command is not on the whitelist.
        """
        normalized = (command or "").strip().lower()
        if normalized not in _ALLOWED_COMMANDS:
            allowed = ", ".join(sorted(_ALLOWED_COMMANDS))
            return (
                f"Rejected: '{command}' is not a Rebuild Orchestrator command. "
                f"Allowed: {allowed}."
            )

        world = unreal.EditorLevelLibrary.get_editor_world()
        unreal.SystemLibrary.execute_console_command(world, normalized)
        return (
            f"Dispatched '{normalized}'. "
            "Logs land in <Project>/Saved/RebuildOrchestrator/Logs/. "
            "Note: rebuild.build and rebuild.full both relaunch after a "
            "successful build."
        )

    @toolset_registry.tool_call
    @staticmethod
    def list_rebuild_commands() -> list[str]:
        """List the Rebuild Orchestrator console commands this tool can run.

        Returns:
            The whitelisted `rebuild.*` command names.
        """
        return sorted(_ALLOWED_COMMANDS)


def register() -> bool:
    """Register this toolset with Unreal's ToolsetRegistry."""
    registry = unreal.ToolsetRegistry
    if not registry.is_available():
        unreal.log_warning("[RebuildOrchestrator] ToolsetRegistry is not available.")
        return False

    if registry.is_toolset_class_registered(RebuildOrchestratorToolset):
        registry.unregister_toolset_class(RebuildOrchestratorToolset)

    registry.register_toolset_class(RebuildOrchestratorToolset)
    unreal.log("[RebuildOrchestrator] RebuildOrchestratorToolset registered.")
    return True


def unregister() -> None:
    """Unregister this toolset if it is currently registered."""
    registry = unreal.ToolsetRegistry
    if registry.is_available() and registry.is_toolset_class_registered(RebuildOrchestratorToolset):
        registry.unregister_toolset_class(RebuildOrchestratorToolset)
