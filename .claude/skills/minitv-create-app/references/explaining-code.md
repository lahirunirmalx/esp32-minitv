# Explaining Mini TV code to beginners

Load this when `Audience: beginner` is set in the project CLAUDE.md (the
default). Deliver the explanation AFTER the build/flash result, as the last
part of the response. When `Audience: expert`, skip all of this — just list
the changed files and the build outcome.

## Goal

The reader is new to embedded C++ and this codebase. After reading your
explanation they should be able to say what each new piece of code is for,
find it again later, and guess where to make their next change. Optimize for
that — not for completeness.

## Structure (per change, in this order)

For each file you created or edited, one short block:

1. **Where** — the file path and which part of it (e.g. "in `build()`, the
   function that runs once when the app opens"). Use clickable
   `file.cpp:line` references.
2. **Why** — the reason this code exists, tied to what the user asked for
   ("you wanted the text to bounce, and LVGL needs an animation object to
   move things over time").
3. **What it does** — what happens on the device when it runs, in cause →
   effect order ("every 16 ms LVGL nudges the label's y-position a little,
   so your eye sees smooth motion").

End with a short **"How it all connects"** paragraph: the path from power-on
(or app-open) through the new code, in 3–5 sentences.

## Language rules

- Simple terms first, jargon second: say "a small worker that runs in the
  background" and only then name it ("called a *task* in FreeRTOS").
- Define each technical word ONCE, in parentheses, on first use. After that,
  use it normally.
- Analogies are welcome when they map cleanly (the two CPU cores = two
  workers: one only paints the screen, the other talks to the internet —
  so the painter never stops to wait for a download).
- Numbers need meaning: not "28 KB buffer" but "a 28 KB buffer — about an
  eighth of the device's working memory, which is why we keep it small".
- Show the 2–5 most important lines inline when explaining them; don't
  re-paste whole files.
- Explain *this device's* quirks when they shaped the code (the screen is
  not a touchscreen; the pad on top only gives tap/hold; colors come from
  the theme so all apps match the shell color).
- Keep the whole explanation under ~40 lines for a typical one-app change.
  One concept per sentence. No walls of text.

## What NOT to explain

- Unchanged scaffold/boilerplate (the generator made it; one sentence max:
  "`new_app.py` generated this file and registered the app — you rarely edit
  the registration yourself").
- Build system internals, include lines, header guards.
- Anything you'd be repeating from a previous explanation in the same
  conversation — refer back instead ("`tick()` works the same way as in the
  clock app we made earlier").

## Example (tone + size target)

> **`main/apps/app_hello/app_hello.cpp` — the new app**
>
> *Where:* `build()` runs once when the app opens; `tick()` runs twice a
> second afterwards.
>
> *Why:* you asked for an animated hello screen. On this device every app
> builds its widgets in `build()` and refreshes them in `tick()` — the
> framework handles opening, closing, and cleanup for you.
>
> *What it does:* `build()` creates a text label and hands it to an LVGL
> *animation* (a helper that changes one number smoothly over time). We tell
> it: move the label's y-position from 40 to 90 over 0.9 s, then back, then
> repeat forever — that's the bounce. The color comes from
> `theme::palette().accent`, so the text automatically matches your device's
> shell color instead of being hardcoded.
>
> **How it connects:** when you tap the pad on top, the supervisor closes the
> current app and opens this one → `build()` makes the label and starts the
> animation → the UI loop redraws the moving label ~60 times a second → when
> you tap again, the framework deletes the screen and the animation with it.
