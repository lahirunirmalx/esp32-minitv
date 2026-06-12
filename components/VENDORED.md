# Vendored components

This project is fully standalone: both UI dependencies are copied into this
directory so `idf.py build` works offline on a fresh checkout — no component
registry fetch, no git clones.

| Component | Version | Upstream | Stripped |
|-----------|---------|----------|----------|
| `lvgl/` | v9.2.0 | <https://github.com/lvgl/lvgl.git> | `.git`, `tests/`, `examples/`, `demos/`, `docs/`, `.github/` |
| `mooncake/` | v2.1.0 | <https://github.com/Forairaaaaa/mooncake.git> | `.git`, `tests/`, `examples/`, `docs/`, `.github/` |

Notes:

- `lvgl/examples/` and `lvgl/demos/` exist as empty placeholder directories —
  LVGL's IDF component registers them as include dirs, so CMake needs them to
  exist even though their sources are stripped.
- LVGL is configured through Kconfig (`CONFIG_LV_*` in `sdkconfig.defaults`),
  not a hand-edited `lv_conf.h`.
- To update a component: re-clone the upstream tag, strip the same paths,
  replace the directory, and update this table.
