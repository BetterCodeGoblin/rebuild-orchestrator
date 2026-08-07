#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 <ProjectPath> <EngineDir> <Mode: full|build|clean|relaunch> <LogDir>" >&2
}

if [[ $# -ne 4 ]]; then
  usage
  exit 2
fi

PROJECT_PATH="$1"
ENGINE_DIR="$2"
MODE="$3"
LOG_DIR="$4"

case "$MODE" in
  full|build|clean|relaunch) ;;
  *)
    echo "[RebuildOrchestrator] Unsupported mode: $MODE" >&2
    exit 2
    ;;
esac

realpath_portable() {
  python3 -c 'import os, sys; print(os.path.abspath(sys.argv[1]))' "$1"
}

PROJECT_PATH="$(realpath_portable "$PROJECT_PATH")"
ENGINE_DIR="$(realpath_portable "$ENGINE_DIR")"
PROJECT_DIR="$(dirname "$PROJECT_PATH")"
LOG_DIR="$(realpath_portable "$LOG_DIR")"

mkdir -p "$LOG_DIR"
TIMESTAMP="$(date +"%Y%m%d-%H%M%S")"
LOG_PATH="$LOG_DIR/RebuildOrchestrator-$MODE-$TIMESTAMP.log"
exec > >(tee -a "$LOG_PATH") 2>&1

status() {
  echo "[RebuildOrchestrator] $*"
}

wait_for_editor_exit() {
  local timeout_seconds="${1:-120}"
  local deadline=$((SECONDS + timeout_seconds))

  status "Waiting for UnrealEditor to exit for project: $PROJECT_PATH"
  status "The in-editor plugin should already have requested a graceful shutdown."

  while (( SECONDS < deadline )); do
    # Best-effort process matching differs across macOS and Linux.
    if ! pgrep -af "UnrealEditor.*$PROJECT_PATH" >/dev/null 2>&1; then
      status "No matching UnrealEditor process found."
      return 0
    fi

    status "Still waiting on matching UnrealEditor process."
    sleep 2
  done

  status "Timed out waiting for UnrealEditor to exit; refusing to continue while the editor may still be running."
  return 1
}

clean_project() {
  for name in Binaries Intermediate; do
    local target="$PROJECT_DIR/$name"
    if [[ -e "$target" ]]; then
      status "Deleting project-local $target"
      rm -rf -- "$target"
    else
      status "Skipping missing $target"
    fi
  done
}

build_project() {
  local project_name
  project_name="$(basename "$PROJECT_PATH" .uproject)"

  local platform
  case "$(uname -s)" in
    Darwin) platform="Mac" ;;
    Linux) platform="Linux" ;;
    *)
      status "Unsupported Unix platform: $(uname -s)"
      exit 2
      ;;
  esac

  local build_script="$ENGINE_DIR/Build/BatchFiles/$platform/Build.sh"
  if [[ ! -x "$build_script" ]]; then
    build_script="$ENGINE_DIR/Build/BatchFiles/Build.sh"
  fi
  if [[ ! -x "$build_script" ]]; then
    status "Build.sh not found or not executable under $ENGINE_DIR/Build/BatchFiles"
    exit 1
  fi

  status "Building ${project_name}Editor $platform Development"
  "$build_script" "${project_name}Editor" "$platform" "Development" "-Project=$PROJECT_PATH" "-WaitMutex" "-FromMsBuild"
}

relaunch_editor() {
  local editor_bin=""

  case "$(uname -s)" in
    Darwin)
      # macOS installs vary; prefer the app bundle binary when present.
      if [[ -x "$ENGINE_DIR/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" ]]; then
        editor_bin="$ENGINE_DIR/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
      else
        editor_bin="$ENGINE_DIR/Binaries/Mac/UnrealEditor"
      fi
      ;;
    Linux)
      editor_bin="$ENGINE_DIR/Binaries/Linux/UnrealEditor"
      ;;
    *)
      status "Unsupported Unix platform: $(uname -s)"
      exit 2
      ;;
  esac

  if [[ ! -x "$editor_bin" ]]; then
    status "UnrealEditor binary not found or not executable: $editor_bin"
    exit 1
  fi

  status "Relaunching UnrealEditor: $editor_bin $PROJECT_PATH"
  nohup "$editor_bin" "$PROJECT_PATH" >/dev/null 2>&1 &
}

status "Mode: $MODE"
status "ProjectPath: $PROJECT_PATH"
status "EngineDir: $ENGINE_DIR"
status "Log: $LOG_PATH"

if [[ ! -f "$PROJECT_PATH" ]]; then
  status "Project file not found: $PROJECT_PATH"
  exit 1
fi

if [[ ! -d "$ENGINE_DIR" ]]; then
  status "Engine directory not found: $ENGINE_DIR"
  exit 1
fi

case "$MODE" in
  clean)
    wait_for_editor_exit 300
    clean_project
    ;;
  build)
    wait_for_editor_exit 300
    build_project
    relaunch_editor
    ;;
  relaunch)
    relaunch_editor
    ;;
  full)
    wait_for_editor_exit 300
    clean_project
    build_project
    relaunch_editor
    ;;
esac

status "Completed successfully."
