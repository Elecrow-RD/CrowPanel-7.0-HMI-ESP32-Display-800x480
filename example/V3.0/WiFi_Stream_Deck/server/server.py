"""Stream Deck dispatcher.

The ESP32 panel POSTs {"key": "<id>"} to /tap; we look up the action in
actions.yaml and run its shell command. actions.yaml and config.yaml are
re-read when their mtime changes, so edits apply without a restart.
"""

from __future__ import annotations

import datetime as dt
import hmac
import logging
import os
import re
import signal
import subprocess
import threading
import time
from pathlib import Path

import yaml
from fastapi import Depends, FastAPI, Header, HTTPException
from fastapi.responses import PlainTextResponse
from pydantic import BaseModel, Field

ROOT = Path(__file__).parent
ACTIONS_PATH = ROOT / "actions.yaml"
CONFIG_PATH = ROOT / "config.yaml"

DEFAULT_TIMEOUT = 10.0
MAX_OUTPUT = 2000

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("wifi_stream_deck")

# Mirror to a file so taps can be audited without keeping the console open.
try:
    _file_handler = logging.FileHandler(ROOT / "server.log", encoding="utf-8")
    _file_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))
    log.addHandler(_file_handler)
except OSError:
    pass

app = FastAPI(title="WiFi Stream Deck server")

_state: dict[str, bool] = {}
_state_lock = threading.Lock()

# (mtime, parsed) caches; reset to None by tests or when the files change.
_actions_cache: tuple[float, dict] | None = None
_config_cache: tuple[float, dict] | None = None


# --------------------------------------------------------------------------- #
# Config / actions loading
# --------------------------------------------------------------------------- #
def _read_yaml(path: Path) -> dict:
    """Parse a YAML file as UTF-8 and guarantee a mapping comes back."""
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    if data is None:
        return {}
    if not isinstance(data, dict):
        raise ValueError(f"{path.name} must contain a mapping at the top level")
    return data


def get_config() -> dict:
    global _config_cache
    try:
        stamp = CONFIG_PATH.stat().st_mtime
    except OSError:
        return {}
    if _config_cache is not None and _config_cache[0] == stamp:
        return _config_cache[1]
    try:
        cfg = _read_yaml(CONFIG_PATH)
    except (OSError, ValueError, yaml.YAMLError) as exc:
        log.error("config.yaml unreadable (%s); using defaults", exc)
        cfg = {}
    _config_cache = (stamp, cfg)
    return cfg


def _normalise_action(action: dict) -> dict:
    """PyYAML implements YAML 1.1, so an unquoted `on:`/`off:` key parses as the
    boolean True/False. That silently broke every toggle action, so accept both
    spellings instead of forcing users to quote the keys.
    """
    fixed: dict = {}
    for key, value in action.items():
        if key is True:
            key = "on"
        elif key is False:
            key = "off"
        fixed[key] = value
    return fixed


def _valid_action(name: str, action: object) -> bool:
    if not isinstance(action, dict):
        log.warning("actions.yaml: %s is not a mapping, skipping", name)
        return False
    if action.get("toggle"):
        ok = isinstance(action.get("on"), str) and isinstance(action.get("off"), str)
        if not ok:
            log.warning("actions.yaml: %s needs string 'on' and 'off', skipping", name)
        return ok
    ok = isinstance(action.get("run"), str) and bool(action["run"].strip())
    if not ok:
        log.warning("actions.yaml: %s needs a non-empty 'run', skipping", name)
    return ok


def load_actions() -> dict:
    """Return validated actions, falling back to the last good copy on error."""
    global _actions_cache
    try:
        stamp = ACTIONS_PATH.stat().st_mtime
    except OSError:
        log.error("actions.yaml is missing at %s", ACTIONS_PATH)
        return _actions_cache[1] if _actions_cache else {}

    if _actions_cache is not None and _actions_cache[0] == stamp:
        return _actions_cache[1]

    try:
        raw = _read_yaml(ACTIONS_PATH)
    except (OSError, ValueError, yaml.YAMLError) as exc:
        log.error("actions.yaml invalid (%s); keeping previous copy", exc)
        return _actions_cache[1] if _actions_cache else {}

    actions = {}
    for key, value in raw.items():
        if not isinstance(key, str):
            continue
        action = _normalise_action(value) if isinstance(value, dict) else value
        if _valid_action(key, action):
            actions[key] = action
    _actions_cache = (stamp, actions)
    return actions


# --------------------------------------------------------------------------- #
# Optional shared-secret auth
# --------------------------------------------------------------------------- #
def _auth_token() -> str:
    auth = get_config().get("auth") or {}
    return str(auth.get("token") or "")


def require_token(x_deck_token: str | None = Header(default=None)) -> None:
    token = _auth_token()
    if not token:
        return
    if not hmac.compare_digest((x_deck_token or "").encode(), token.encode()):
        raise HTTPException(status_code=401, detail="missing or invalid X-Deck-Token")


# --------------------------------------------------------------------------- #
# Command execution
# --------------------------------------------------------------------------- #
def _kill_tree(pid: int) -> None:
    try:
        if os.name == "nt":
            subprocess.run(
                ["taskkill", "/F", "/T", "/PID", str(pid)],
                capture_output=True,
                check=False,
            )
        else:
            os.killpg(os.getpgid(pid), signal.SIGKILL)
    except (OSError, subprocess.SubprocessError):
        pass


def run_command(cmd: str) -> tuple[int, str, bool]:
    """Run a shell command. Returns (returncode, output, timed_out).

    Never raises: a hung action is killed (including children) so one bad
    command cannot pin a request thread forever.
    """
    timeout = float((get_config().get("exec") or {}).get("timeout", DEFAULT_TIMEOUT))
    log.info("run: %s", cmd)

    kwargs: dict = {
        "shell": True,
        "stdout": subprocess.PIPE,
        "stderr": subprocess.STDOUT,
        "text": True,
        "encoding": "utf-8",
        "errors": "replace",
    }
    if os.name != "nt":
        kwargs["start_new_session"] = True
    else:
        # No console window flash for each tap.
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW

    proc = subprocess.Popen(cmd, **kwargs)
    try:
        out, _ = proc.communicate(timeout=timeout)
        return proc.returncode if proc.returncode is not None else 0, (out or "").strip(), False
    except subprocess.TimeoutExpired:
        _kill_tree(proc.pid)
        try:
            out, _ = proc.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            out = ""
        log.warning("command timed out after %.1fs: %s", timeout, cmd)
        return -1, (out or "").strip(), True


# --------------------------------------------------------------------------- #
# API
# --------------------------------------------------------------------------- #
class Tap(BaseModel):
    key: str = Field(min_length=1, max_length=64)


@app.post("/tap", dependencies=[Depends(require_token)])
def tap(body: Tap):
    actions = load_actions()
    action = actions.get(body.key)
    if action is None:
        log.warning("unknown key: %s", body.key)
        raise HTTPException(status_code=404, detail=f"unknown key: {body.key}")

    if action.get("toggle"):
        with _state_lock:
            new_state = not _state.get(body.key, False)
            _state[body.key] = new_state
        cmd = action["on"] if new_state else action["off"]
        started = time.monotonic()
        rc, out, timed_out = run_command(cmd)
        log.info("tap %s rc=%s timed_out=%s %.0f ms out=%r", body.key, rc, timed_out,
                 (time.monotonic() - started) * 1000, out[-60:])
        return {
            "key": body.key,
            "state": "on" if new_state else "off",
            "rc": rc,
            "timed_out": timed_out,
            "out": out[-MAX_OUTPUT:],
        }

    started = time.monotonic()
    rc, out, timed_out = run_command(action["run"])
    log.info("tap %s rc=%s timed_out=%s %.0f ms out=%r", body.key, rc, timed_out,
             (time.monotonic() - started) * 1000, out[-60:])
    return {"key": body.key, "rc": rc, "timed_out": timed_out, "out": out[-MAX_OUTPUT:]}


@app.get("/state")
def state():
    with _state_lock:
        return {"toggles": dict(_state)}


def _diary_settings() -> tuple[bool, Path]:
    diary = get_config().get("diary") or {}
    enabled = bool(diary.get("enabled", False))
    raw = str(diary.get("path") or "")
    return enabled, Path(raw).expanduser() if raw else ROOT


def _require_diary() -> Path:
    enabled, path = _diary_settings()
    if not enabled:
        raise HTTPException(404, "diary feature disabled in config.yaml")
    return path


@app.get("/")
def root():
    cfg = get_config()
    return {
        "service": "wifi-stream-deck",
        "actions": sorted(load_actions()),
        "features": {"diary": bool((cfg.get("diary") or {}).get("enabled", False))},
        "auth_required": bool(_auth_token()),
    }


@app.get("/health")
def health():
    # The panel mirrors this clock and shows it in the header, so both sides
    # always agree on the time even without NTP access on the device. IST has no
    # DST, so a fixed +05:30 offset avoids needing tzdata on Windows.
    now = dt.datetime.now(dt.timezone(dt.timedelta(hours=5, minutes=30)))
    return {
        "ok": True,
        "hhmm": now.strftime("%I:%M %p").lstrip("0"),
        "date": now.strftime("%Y-%m-%d"),
    }


# --------------------------------------------------------------------------- #
# Diary (opt-in)
# --------------------------------------------------------------------------- #
def _parse_markdown_blocks(text: str) -> list[dict]:
    """Parse markdown into a flat block list suitable for rendering on the ESP32.

    Block types: h1, h2, h3, todo, bullet, paragraph, spacer.
    """
    blocks: list[dict] = []
    for raw in text.splitlines():
        line = raw.rstrip()
        if not line.strip():
            blocks.append({"type": "spacer"})
            continue
        m = re.match(r"^(#{1,6})\s+(.*)", line)
        if m:
            level = min(len(m.group(1)), 3)
            blocks.append({"type": f"h{level}", "text": m.group(2).strip()})
            continue
        m = re.match(r"^\s*[-*]\s+\[( |x|X)\]\s+(.*)", line)
        if m:
            done = m.group(1).lower() == "x"
            blocks.append({"type": "todo", "done": done, "text": m.group(2).strip()})
            continue
        m = re.match(r"^(\s*)[-*]\s+(.*)", line)
        if m:
            indent = len(m.group(1)) // 2
            blocks.append({"type": "bullet", "indent": indent, "text": m.group(2).strip()})
            continue
        blocks.append({"type": "paragraph", "text": line.strip()})
    return blocks


def _simplify_markdown(text: str) -> str:
    """Strip markdown sigils the ESP32 can't render, keep the content readable."""
    lines = []
    for line in text.splitlines():
        m = re.match(r"^(#{1,6})\s+(.*)", line)
        if m:
            lines.append(m.group(2).upper() if len(m.group(1)) <= 2 else m.group(2))
            continue
        m = re.match(r"^(\s*)[-*]\s+\[( |x|X)\]\s+(.*)", line)
        if m:
            mark = "[x]" if m.group(2).lower() == "x" else "[ ]"
            lines.append(f"{m.group(1)}{mark} {m.group(3)}")
            continue
        m = re.match(r"^(\s*)[-*]\s+(.*)", line)
        if m:
            lines.append(f"{m.group(1)}- {m.group(2)}")
            continue
        lines.append(line)
    return "\n".join(lines)


def _today_path() -> tuple[Path, dt.date]:
    today = dt.date.today()
    return _require_diary() / f"{today.isoformat()}.md", today


def _read_text(path: Path) -> str:
    # Explicit UTF-8: the default codec on Windows is cp1252 and would blow up
    # on any emoji or non-Latin character in a diary entry.
    return path.read_text(encoding="utf-8", errors="replace")


@app.get("/diary/today", response_class=PlainTextResponse)
def diary_today():
    path, today = _today_path()
    if not path.exists():
        return f"No diary for {today.isoformat()}\n\n(file: {path})"
    try:
        return _simplify_markdown(_read_text(path))
    except OSError as exc:
        raise HTTPException(500, f"can't read {path}: {exc}") from exc


@app.get("/diary/today/mtime")
def diary_today_mtime():
    path, today = _today_path()
    exists = path.exists()
    return {
        "date": today.isoformat(),
        "exists": exists,
        "mtime": path.stat().st_mtime if exists else 0,
    }


@app.get("/diary/today.json")
def diary_today_json():
    path, today = _today_path()
    if not path.exists():
        return {
            "date": today.isoformat(),
            "exists": False,
            "mtime": 0,
            "blocks": [{"type": "paragraph", "text": f"No diary for {today.isoformat()}"}],
        }
    try:
        text = _read_text(path)
    except OSError as exc:
        raise HTTPException(500, f"can't read {path}: {exc}") from exc
    return {
        "date": today.isoformat(),
        "exists": True,
        "mtime": path.stat().st_mtime,
        "blocks": _parse_markdown_blocks(text),
    }
