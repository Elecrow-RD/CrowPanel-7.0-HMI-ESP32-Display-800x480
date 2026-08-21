#!/usr/bin/env python3
"""Dry-run validate every component declared in the upload workflow.

Parses .github/workflows/upload_components.yml and resolves component
names exactly like espressif/upload-components-ci-action (split on
`[;\\n]`, then on the first `:`; an omitted name defaults to the path's
basename, except the repo root which requires an explicit name). Each
resolved component is packed and validated with `compote ... --dry-run`,
so a malformed `components:` list fails CI here instead of on push.
"""
import re
import subprocess
import sys
from pathlib import Path

ACTION = "espressif/upload-components-ci-action"
# Repo root is this script's parent directory's parent (ci/ -> repo root);
# component paths in the workflow are resolved relative to it.
ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github/workflows/upload_components.yml"


def find_action_step(workflow):
    for job in (workflow.get("jobs") or {}).values():
        for step in job.get("steps") or []:
            if isinstance(step, dict) and str(step.get("uses", "")).startswith(ACTION):
                return step
    return None


def parse_components(components_str):
    """Mirror upload.py: split on [;\\n], then first ':' into name/path."""
    items = []
    for raw in re.split(r"[;\n]", components_str):
        entry = raw.strip()
        if not entry:
            continue
        if ":" in entry:
            name, path = entry.split(":", 1)
            items.append((name.strip(), path.strip(), entry))
        else:
            items.append((None, entry, entry))
    return items


def main():
    import yaml

    if not WORKFLOW.is_file():
        sys.exit(f"Workflow not found: {WORKFLOW}")

    step = find_action_step(yaml.safe_load(WORKFLOW.read_text()))
    if step is None:
        sys.exit(f"No '{ACTION}' step found in {WORKFLOW}")

    with_ = step.get("with") or {}
    namespace = (with_.get("namespace") or "espressif").strip()
    components = parse_components(with_.get("components") or "")
    if not components:
        sys.exit("No components listed in the workflow 'components' input")

    failures = []
    for name, path, entry in components:
        # Name resolution mirrors upload-components-ci-action v2 exactly: an
        # omitted name defaults to the path basename, except the repo root
        # which must be named explicitly. All other validation is delegated
        # to the compote dry-run below, as the action does.
        full = (ROOT / path).resolve()
        if name is None:
            if full == ROOT:
                failures.append(
                    f"{entry!r}: component in repo root needs an explicit "
                    f'name (e.g. "rmaker_common: .")'
                )
                continue
            name = full.name

        print(f"==> Validating {namespace}/{name} ({path})", flush=True)
        result = subprocess.run(
            [
                "compote", "component", "upload", "--dry-run",
                "--namespace", namespace, "--name", name,
                "--project-dir", str(full),
            ],
            capture_output=True,
            text=True,
        )
        # Echo compote's output so CI logs remain self-explanatory.
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        if result.returncode != 0:
            # A version that is already published is not a validation error:
            # the component packed and validated fine, it just exists already.
            # The real upload-components-ci-action skips such versions rather
            # than failing, so mirror that here instead of breaking every MR
            # that doesn't bump a version.
            combined = f"{result.stdout}\n{result.stderr}"
            if "already on the registry" in combined:
                print(
                    f"    (version already published; skipping like the "
                    f"upload action)",
                    flush=True,
                )
            else:
                failures.append(f"{namespace}/{name} ({path}): dry-run failed")

    if failures:
        print("\nComponent validation FAILED:")
        for f in failures:
            print(f"  - {f}")
        sys.exit(1)
    print(f"\nAll {len(components)} components valid.")


if __name__ == "__main__":
    main()
