"""Editor start-up hook for the Rebuild Orchestrator MCP toolset.

Unreal runs `init_unreal.py` (and only that filename) from every enabled
plugin's `Content/Python/` directory at editor start-up, and adds that
directory to `sys.path`. It does NOT auto-import other modules sitting beside
it, so this hook imports `rebuild_orchestrator_toolset` to get the class
defined.

Importing is necessary but NOT sufficient. The `@unreal.uclass()` decorator
only creates the UClass -- it does not add the toolset to the Toolset Registry.
Registration is an explicit call, exactly as the stock engine toolsets do it in
Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/init_unreal.py:

    toolsets._registration.register()

Without the `Registration(...).register()` call below, the module imports
cleanly, no error is logged, and the toolset is still invisible over MCP --
`list_toolsets` shows every stock toolset and none of ours. Note that
`ModelContextProtocol.RefreshTools` does not help in that state: it only
re-publishes the toolsets already in the registry, so an unregistered toolset
stays unregistered no matter how many times you refresh.
"""

import unreal

from toolset_registry.registration import Registration

# Module-level handle so the registration survives past start-up and can be
# unregistered on reload.
_registration = None

try:
    import rebuild_orchestrator_toolset

    _registration = Registration([
        rebuild_orchestrator_toolset.RebuildOrchestratorToolset,
    ])

    if _registration.register():
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
