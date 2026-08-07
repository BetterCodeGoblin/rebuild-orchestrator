"""Editor start-up hook for the Rebuild Orchestrator MCP toolset.

Unreal runs `init_unreal.py` (and only that filename) from every enabled
plugin's `Content/Python/` directory at editor start-up, and adds that
directory to `sys.path`. It does NOT auto-import other modules sitting beside
it. Without this hook, `rebuild_orchestrator_toolset.py` is importable but
never imported: the `@unreal.uclass()` decorator never runs,
`RebuildOrchestratorToolset` is never defined, and the Toolset Registry never
learns about it -- so the MCP client sees every stock toolset and none of ours.

Importing the module here executes the decorator for its registration side
effect, which is what makes `run_rebuild_command` / `list_rebuild_commands`
show up over MCP.
"""

import unreal

try:
    import rebuild_orchestrator_toolset  # noqa: F401  (imported for side effect)
    unreal.log("[RebuildOrchestrator] MCP toolset module imported; tools registered.")
except Exception as exc:  # pragma: no cover - surfaced in the editor log
    unreal.log_error(
        "[RebuildOrchestrator] Failed to import rebuild_orchestrator_toolset: "
        f"{exc}"
    )
