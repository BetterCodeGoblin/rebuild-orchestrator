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
  * Registered at editor start-up: the sibling Content/Python/init_unreal.py
    imports this module so the @unreal.uclass() decorator runs and the Toolset
    Registry sees it. (Unreal only auto-runs init_unreal.py from a plugin's
    Content/Python/; a bare module here would never be imported on its own.)

Refresh after editing without restarting the editor:
    ModelContextProtocol.RefreshTools
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

    @staticmethod
    @toolset_registry.tool_call
    def run_rebuild_command(command: str) -> str:
        """Run one whitelisted Rebuild Orchestrator console command.

        Use this to compile C++ changes without asking the user to type the
        console command by hand. `rebuild.build` requests a graceful editor
        shutdown, builds through UBT, and does NOT auto-relaunch; follow with
        `rebuild.relaunch` (or use `rebuild.full` to build and relaunch in one
        step). Check `rebuild.status` if a build fails with a file lock.

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
            "Note: rebuild.build does not auto-relaunch -- follow with "
            "rebuild.relaunch, or use rebuild.full."
        )

    @staticmethod
    @toolset_registry.tool_call
    def list_rebuild_commands() -> list[str]:
        """List the Rebuild Orchestrator console commands this tool can run.

        Returns:
            The whitelisted `rebuild.*` command names.
        """
        return sorted(_ALLOWED_COMMANDS)
