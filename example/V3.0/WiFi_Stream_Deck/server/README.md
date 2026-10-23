# WiFi Stream Deck server

Companion HTTP dispatcher for the CrowPanel firmware. See the
[project README](../README.md) for setup; this file is the API reference.

```bash
./run.sh                          # macOS / Linux
.\run.ps1                         # Windows PowerShell
```

The server binds `0.0.0.0:8787`, reloads `actions.yaml` and `config.yaml` when
they change, and keeps the last good copy if an edit leaves invalid YAML.

On Windows the shipped actions call `win\deckctl.exe` (Core Audio volume/mute,
media keys, Esc/Ctrl/Alt chords). Rebuild it after editing `win\deckctl.cs`:

```powershell
.\win\build-deckctl.ps1
```

## Endpoints

```
GET  /                    service info: actions, features, auth_required
GET  /health              {"ok": true, "hhmm": "7:42 PM", "date": "2026-10-21"}
POST /tap                 body {"key": "<id>"} -> runs actions.yaml[key]
GET  /state               in-memory toggle states
GET  /diary/today         plain text, simplified markdown      (opt-in)
GET  /diary/today.json    block AST for rich rendering         (opt-in)
GET  /diary/today/mtime   cheap poll target                    (opt-in)
```

`/health` also feeds the panel header clock: `hhmm` is IST (UTC+05:30) in
12-hour form, so the device never needs NTP or a timezone setting.

`/tap` returns `rc`, `timed_out`, and the last 2000 characters of
`stdout+stderr`. A command that exceeds `exec.timeout` (default 10 s) is killed
with its child processes and reported as `rc: -1, timed_out: true`.

If `auth.token` is set, `POST /tap` requires `X-Deck-Token: <token>` (401
otherwise). The panel sends it when `SERVER_TOKEN` is non-empty in
`include/secrets.h`.

## Response shape

```json
{"key": "play_pause", "rc": 0, "timed_out": false, "out": ""}
{"key": "mute_mic", "state": "on", "rc": 0, "timed_out": false, "out": ""}
```

## Diary block types

`h1` `h2` `h3` `todo` `bullet` `paragraph` `spacer`, with `text`, `done`, and
`indent` where applicable.

## Tests

```bash
python -m pip install -r requirements-dev.txt
python -m pytest -q
```
