"""Editor start-up hook for the Rebuild Orchestrator MCP toolset.

Unreal runs `init_unreal.py` (and only that filename) from every enabled
plugin's `Content/Python/` directory at editor start-up, and adds that
directory to `sys.path`. It does NOT auto-import other modules sitting beside
it, so this hook imports `rebuild_orchestrator_toolset` to get the class
defined.

Importing is necessary but NOT sufficient. The `@unreal.uclass()` decorator
only creates the UClass -- it does not add the toolset to the Toolset Registry.
Registration is an explicit call. The toolset module owns that call through
its register() function, following Epic's UE 5.8 ToolsetRegistry pattern.

`ModelContextProtocol.RefreshTools` does not help when the toolset was never
registered: it only re-publishes the current registry, so an unregistered
toolset stays unregistered no matter how many times you refresh.
"""

import unreal

try:
    import rebuild_orchestrator_toolset

    if rebuild_orchestrator_toolset.register():
        unreal.log(
            "[RebuildOrchestrator] MCP toolset registered; "
            "run_rebuild_command / list_rebuild_commands available."
        )
    else:
        unreal.log_error(
            "[RebuildOrchestrator] Toolset registry unavailable; "
            "rebuild tools will NOT appear over MCP."
        )
except Exception as exc:  # pragma: no cover - surfaced in the editor log
    unreal.log_error(
        "[RebuildOrchestrator] Failed to register rebuild_orchestrator_toolset: "
        f"{exc}"
    )
