"""Tests for the Stream Deck dispatcher.

Run from the server directory:  python -m pytest -q
"""

from __future__ import annotations

import datetime as dt
import os
import re
import sys
import time
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import server  # noqa: E402


def py(code: str) -> str:
    """A cross-platform shell command that runs Python code."""
    return f'"{sys.executable}" -c "{code}"'


def write_actions(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


@pytest.fixture
def paths(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> tuple[Path, Path]:
    actions = tmp_path / "actions.yaml"
    config = tmp_path / "config.yaml"
    write_actions(actions, f"ping:\n  run: '{py('print(1)')}'\n")
    config.write_text("diary:\n  enabled: false\n", encoding="utf-8")

    monkeypatch.setattr(server, "ACTIONS_PATH", actions)
    monkeypatch.setattr(server, "CONFIG_PATH", config)
    monkeypatch.setattr(server, "_actions_cache", None)
    monkeypatch.setattr(server, "_config_cache", None)
    server._state.clear()
    return actions, config


@pytest.fixture
def client(paths) -> TestClient:
    with TestClient(server.app) as c:
        yield c


def test_root_lists_actions(client):
    body = client.get("/").json()
    assert body["service"] == "wifi-stream-deck"
    assert body["actions"] == ["ping"]
    assert body["features"]["diary"] is False
    assert body["auth_required"] is False


def test_health(client):
    body = client.get("/health").json()
    assert body["ok"] is True
    assert re.fullmatch(r"\d{1,2}:\d{2} (AM|PM)", body["hhmm"])
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2}", body["date"])


def test_unknown_key_is_404(client):
    assert client.post("/tap", json={"key": "nope"}).status_code == 404


def test_empty_key_rejected(client):
    assert client.post("/tap", json={"key": ""}).status_code == 422


def test_tap_runs_command(client):
    body = client.post("/tap", json={"key": "ping"}).json()
    assert body["rc"] == 0
    assert body["timed_out"] is False


def test_toggle_flips_state(client, paths):
    actions, _ = paths
    write_actions(
        actions,
        "flip:\n"
        "  toggle: true\n"
        f"  on: '{py('print(1)')}'\n"
        f"  off: '{py('print(0)')}'\n",
    )
    os.utime(actions, None)

    first = client.post("/tap", json={"key": "flip"}).json()
    second = client.post("/tap", json={"key": "flip"}).json()
    assert (first["state"], second["state"]) == ("on", "off")
    assert client.get("/state").json()["toggles"] == {"flip": False}


def test_invalid_action_is_skipped(client, paths):
    actions, _ = paths
    write_actions(
        actions,
        f"good:\n  run: '{py('print(1)')}'\n"
        "bad: 42\n"
        "no_run:\n  label: hi\n",
    )
    os.utime(actions, None)
    assert client.get("/").json()["actions"] == ["good"]


def test_broken_yaml_keeps_last_good_copy(client, paths):
    actions, _ = paths
    assert client.get("/").json()["actions"] == ["ping"]  # populate the cache

    write_actions(actions, "ping: [unclosed\n")
    stamp = time.time() + 10
    os.utime(actions, (stamp, stamp))

    assert client.get("/").json()["actions"] == ["ping"]
    assert client.post("/tap", json={"key": "ping"}).status_code == 200


def test_timeout_is_killed_not_raised(client, paths):
    _, config = paths
    config.write_text(
        "diary:\n  enabled: false\nexec:\n  timeout: 0.5\n", encoding="utf-8"
    )
    actions, _ = paths
    write_actions(actions, f"slow:\n  run: '{py('import time; time.sleep(30)')}'\n")
    os.utime(actions, None)

    started = time.time()
    body = client.post("/tap", json={"key": "slow"}).json()
    elapsed = time.time() - started

    assert body["timed_out"] is True
    assert body["rc"] == -1
    assert elapsed < 5


def test_auth_token_enforced(client, paths):
    _, config = paths
    config.write_text(
        "diary:\n  enabled: false\nauth:\n  token: s3cret\n", encoding="utf-8"
    )

    assert client.post("/tap", json={"key": "ping"}).status_code == 401
    assert (
        client.post(
            "/tap", json={"key": "ping"}, headers={"X-Deck-Token": "wrong"}
        ).status_code
        == 401
    )
    assert (
        client.post(
            "/tap", json={"key": "ping"}, headers={"X-Deck-Token": "s3cret"}
        ).status_code
        == 200
    )
    assert client.get("/").json()["auth_required"] is True


def test_diary_disabled_returns_404(client):
    assert client.get("/diary/today.json").status_code == 404
    assert client.get("/diary/today/mtime").status_code == 404


def test_diary_renders_blocks(client, paths, tmp_path):
    _, config = paths
    config.write_text(
        f"diary:\n  enabled: true\n  path: '{tmp_path.as_posix()}'\n", encoding="utf-8"
    )
    today = dt.date.today().isoformat()
    (tmp_path / f"{today}.md").write_text(
        "# Today\n- [x] done\n- [ ] todo\n- bullet\n\npara\n", encoding="utf-8"
    )

    body = client.get("/diary/today.json").json()
    types = [b["type"] for b in body["blocks"]]
    assert types == ["h1", "todo", "todo", "bullet", "spacer", "paragraph"]
    assert body["blocks"][1]["done"] is True
    assert body["blocks"][2]["done"] is False

    text = client.get("/diary/today").text
    assert "TODAY" in text


def test_diary_reads_utf8(tmp_path, paths):
    _, config = paths
    config.write_text(
        f"diary:\n  enabled: true\n  path: '{tmp_path.as_posix()}'\n", encoding="utf-8"
    )
    today = dt.date.today().isoformat()
    (tmp_path / f"{today}.md").write_text("# Café ☕\n", encoding="utf-8")

    body = server.diary_today_json()
    assert "Café ☕" in body["blocks"][0]["text"]


@pytest.mark.parametrize(
    ("line", "block_type"),
    [
        ("# Title", "h1"),
        ("### Sub", "h3"),
        ("- [ ] open", "todo"),
        ("- [x] done", "todo"),
        ("- item", "bullet"),
        ("plain text", "paragraph"),
        ("\n", "spacer"),
    ],
)
def test_markdown_block_parser(line, block_type):
    blocks = server._parse_markdown_blocks(line)
    assert blocks[0]["type"] == block_type


def test_markdown_indent_is_halved():
    block = server._parse_markdown_blocks("    - nested")[0]
    assert block["indent"] == 2
