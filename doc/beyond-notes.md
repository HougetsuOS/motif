# Beyond the abstraction — 7.1 theming, 7.2 HiDPI, 7.3 a11y (2026-09-07)

Workstream notes for plan §7 (all three landed as one batch, after the
phase 0-7 sequence completed at 23bdef3).

## 7.1 Theming — XmLoadTheme + theme profiles

- `lib/Xm/Theme.c` (new): `XmLoadTheme(shell, name)` — public, additive.
  - Theme profiles are Xrm resource files looked up in
    `$XDG_CONFIG_HOME|~/.config/motif/themes/<name>`, then
    `$XDG_DATA_DIRS|/usr/share/motif/themes/<name>` (absolute paths pass
    through).
  - Merge: `XrmGetFileDatabase` + `XrmCombineDatabase` into the shell's
    screen database with override (Display.c precedent).
  - Live re-apply: the palette key is the theme's `*background`; the
    walker re-runs `XmChangeColor` per Manager/Primitive/Gadget so
    foreground/shadows/select stay derived from the one color-calc hook
    (no new color machinery).  Non-color resources take effect for
    widgets created afterwards (shell-realize-time contract).
  - `$MOTIF_THEME` auto-applies at the first vendor-shell realize
    (`_XmThemeEnvInit`) so apps get theming with zero code changes.
- `themes/{default,high-contrast,monochrome-legacy}`: the three
  committed profiles.  Verification: hellomotif with
  `MOTIF_THEME=high-contrast XDG_CONFIG_HOME=themes-root` renders
  differently (3.4% of screen bytes vs baseline) with zero app-code
  changes; plain runs stay byte-identical.
- UBSan caught a use-after-merge bug: `XrmCombineDatabase` may consume
  the source db; the palette probe now reads the merged screen database,
  not the freed theme db.

## 7.2 HiDPI — scale factor at the font seam

- `XmScreen` gains `XmNscaleFactor` (int, permille; default 0 = unset →
  `$MOTIF_SCALE` env → `Xft.dpi` heuristic ×1000/72 → 1000).
- The single consumer this round is the Xft font seam
  (`XmRenderT.c:ValidateAndLoadFont`): the rendition's FC_SIZE and
  FC_PIXEL_SIZE are multiplied by scale/1000 before `XftFontMatch`, so
  all text metrics (and thus text-driven geometry) follow the factor.
  Verified: a render-table label renders at 2× with `MOTIF_SCALE=2000`
  (pixel diff vs 1×: 1247 bytes).
- Known limitation (documented, matches the plan's phasing): apps using
  core-font fontLists (hellomotif's "fixed") don't scale — those fonts
  are server-side bitmaps with no scalable source.  The XmFont contract
  is the intended single seam once fontList→Xft re-emit (plan §3
  Phase 2's full exit) happens.  Icon blit scaling rides the Phase-6
  cairo backend when needed.

## 7.3 Accessibility — bridge skeleton

- `lib/Xm/XmA11y.c` (new): registration through `XtHooksOfDisplay` —
  the hook object's `createHook` callback list fires on every widget
  creation (name verified empirically against libXt; `XtNcallback`/
  `hookCallback`/`callback` do not resolve).
- v1 reporter writes JSON lines to `$MOTIF_A11Y_LOG`
  (widget/class/role/x/y/w/h); the ATSPI D-Bus transport is a second
  reporter behind the same seam, to be shipped as the optional
  dlopen-able library the plan describes.  Roles come from the class
  hierarchy (most-derived first: push-button before label, etc.).
- Verified: hellomotif with `MOTIF_A11Y_LOG` reports the bulletin
  board (panel), label, push button with geometry; UBSan clean.
- Deferred with justification: the `XmA11yRole` constraint resource
  (requires touching every class record; the hierarchy-derived default
  covers the common roles) and the D-Bus transport (its own library +
  protocol work; the seam is in place).
