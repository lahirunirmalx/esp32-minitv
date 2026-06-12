"""Tests for scripts/new_app.py (run from the project root: pytest).

The generator resolves all paths relative to its own location, so each test
copies the script + the real apps.cpp into a temp tree and runs it there —
the actual project is never touched.
"""
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parent.parent


@pytest.fixture()
def tree(tmp_path):
    (tmp_path / "scripts").mkdir()
    (tmp_path / "main" / "apps").mkdir(parents=True)
    shutil.copy(REPO / "scripts" / "new_app.py", tmp_path / "scripts" / "new_app.py")
    shutil.copy(REPO / "main" / "apps" / "apps.cpp", tmp_path / "main" / "apps" / "apps.cpp")
    (tmp_path / "main" / "CMakeLists.txt").write_text("# stub\n")
    return tmp_path


def run(tree, *args):
    return subprocess.run(
        [sys.executable, str(tree / "scripts" / "new_app.py"), *args],
        capture_output=True, text=True,
    )


def test_create_generates_and_registers(tree):
    # use a name no shipped app will ever take — the fixture copies the real
    # apps.cpp, so real app names (demo/info/weather/...) would collide
    r = run(tree, "zztest")
    assert r.returncode == 0, r.stderr
    assert (tree / "main" / "apps" / "app_zztest" / "app_zztest.h").exists()
    assert (tree / "main" / "apps" / "app_zztest" / "app_zztest.cpp").exists()
    src = (tree / "main" / "apps" / "apps.cpp").read_text()
    assert '#include "app_zztest/app_zztest.h"' in src
    assert 'install("zztest", std::make_unique<AppZztest>());' in src
    # markers must survive for the next run
    assert "// <<APP_INCLUDES>>" in src
    assert "// <<APP_INSTALL>>" in src


def test_underscore_name_class_case(tree):
    r = run(tree, "net_meter")
    assert r.returncode == 0, r.stderr
    hdr = (tree / "main" / "apps" / "app_net_meter" / "app_net_meter.h").read_text()
    assert "class AppNetMeter" in hdr


def test_duplicate_rejected(tree):
    assert run(tree, "zzclock").returncode == 0
    r = run(tree, "zzclock")
    assert r.returncode != 0
    assert "already" in r.stderr


def test_invalid_name_rejected(tree):
    # note: uppercase is allowed — the generator lowercases names by design
    for bad in ("9lives", "hello-world", "app.dot"):
        assert run(tree, bad).returncode != 0
    assert run(tree, "--", "").returncode != 0


def test_remove_cleans_up(tree):
    assert run(tree, "zzclock").returncode == 0
    r = run(tree, "zzclock", "--remove")
    assert r.returncode == 0, r.stderr
    assert not (tree / "main" / "apps" / "app_zzclock").exists()
    src = (tree / "main" / "apps" / "apps.cpp").read_text()
    assert "zzclock" not in src
    # generator must be re-runnable after a remove
    assert run(tree, "zzclock").returncode == 0
