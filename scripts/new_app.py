#!/usr/bin/env python3
"""Generate a new Mini TV app skeleton and register it.

Usage:
    python scripts/new_app.py <name>            # e.g. python scripts/new_app.py weather
    python scripts/new_app.py <name> --remove   # unregister + delete the app

Creates main/apps/app_<name>/app_<name>.{h,cpp} from the ScreenApp template,
inserts the include/install lines at the <<APP_*>> markers in
main/apps/apps.cpp, and touches main/CMakeLists.txt so the next build
re-globs sources. Python 3.8+ stdlib only.
"""
import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
APPS_CPP = ROOT / "main" / "apps" / "apps.cpp"
MAIN_CMAKE = ROOT / "main" / "CMakeLists.txt"

HEADER_TMPL = """/**
 * @file app_{name}.h
 * @brief {Title} app.
 */
#pragma once
#include "../../ui/screen_app.h"

class App{Class} : public ui::ScreenApp {{
public:
    App{Class}() {{ setAppInfo().name = "{name}"; }}

protected:
    void build(lv_obj_t* root) override;  // create widgets (UI task, core 1)
    void tick() override;                 // throttled repaint; never block
    // void teardown() override;          // stop timers/threads if you add any

private:
    lv_obj_t* _title = nullptr;
}};
"""

CPP_TMPL = """/**
 * @file app_{name}.cpp
 * @brief See app_{name}.h.
 *
 * Rules (see .claude/skills/minitv-create-app/SKILL.md):
 *   - colors come from theme::palette(), never hardcoded
 *   - build()/tick() run on the UI task (core 1) and must never block;
 *     slow work (HTTP/JSON) goes on a core-0 std::thread -> atomics/queue
 *   - input is tap/hold on the top pad only — no positional touch
 */
#include "app_{name}.h"

void App{Class}::build(lv_obj_t* root)
{{
    const auto& pal = theme::palette();
    setRefreshMs(500);

    _title = theme::make_label(root, "{Title}", false, &lv_font_montserrat_24);
    lv_obj_set_style_text_color(_title, pal.accent, 0);
    lv_obj_align(_title, LV_ALIGN_CENTER, 0, 0);
}}

void App{Class}::tick()
{{
    // Repaint from the latest data here (cheap reads only).
}}
"""


def fail(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def to_class(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("name", help="app name: lowercase letters/digits/underscores, e.g. weather")
    ap.add_argument("--remove", action="store_true", help="unregister and delete the app")
    args = ap.parse_args()

    name = args.name.lower()
    if name.startswith("app_"):
        name = name[len("app_"):]
    if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
        fail("name must match [a-z][a-z0-9_]* (e.g. weather, net_meter)")

    app_dir = ROOT / "main" / "apps" / f"app_{name}"
    cls = to_class(name)
    title = name.replace("_", " ").title()

    src = APPS_CPP.read_text()
    include_line = f'#include "app_{name}/app_{name}.h"\n'
    install_line = f'    install("{name}", std::make_unique<App{cls}>());\n'

    if args.remove:
        if not app_dir.exists():
            fail(f"{app_dir} does not exist")
        shutil.rmtree(app_dir)
        src = src.replace(include_line, "").replace(install_line, "")
        APPS_CPP.write_text(src)
        MAIN_CMAKE.touch()
        print(f"removed app_{name} and unregistered it")
        return

    if app_dir.exists():
        fail(f"{app_dir} already exists")
    if include_line in src:
        fail(f"app '{name}' is already registered in apps.cpp")
    for marker in ("// <<APP_INCLUDES>>", "    // <<APP_INSTALL>>"):
        if marker not in src:
            fail(f"marker '{marker.strip()}' missing from {APPS_CPP}")

    app_dir.mkdir(parents=True)
    fmt = dict(name=name, Class=cls, Title=title)
    (app_dir / f"app_{name}.h").write_text(HEADER_TMPL.format(**fmt))
    (app_dir / f"app_{name}.cpp").write_text(CPP_TMPL.format(**fmt))

    src = src.replace("// <<APP_INCLUDES>>", f'{include_line.strip()}\n// <<APP_INCLUDES>>')
    src = src.replace("    // <<APP_INSTALL>>", f'{install_line}    // <<APP_INSTALL>>')
    APPS_CPP.write_text(src)
    MAIN_CMAKE.touch()  # force CMake re-glob on next build

    print(f"created {app_dir.relative_to(ROOT)}/ and registered '{name}'")
    print("next: implement build()/tick(), then ./scripts/flash.sh")


if __name__ == "__main__":
    main()
