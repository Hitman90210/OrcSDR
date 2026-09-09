# OrcSDR Tab5 UI design system

The Tab5 interface is a dense instrument panel, not a collection of unrelated
demo screens. Shared navigation and status chrome uses the tokens in
`apps/orcsdr-tab5/ui/ui_theme.hpp`; dashboard-specific spectrum and waterfall
palettes may remain local because they encode data rather than product chrome.

## Core rules

- Use `theme::background`, `surface`, or `surface_deep` for structural layers.
- Use `theme::primary` for navigation and ordinary interactive controls.
- Use `theme::success`, `warning`, and `danger` only for meaningful state.
- Use `theme::text_muted` and `divider` for secondary information and grids.
- Keep touch targets at least `theme::minimum_touch_size` (48 px) in both axes.
- Use `theme::control_radius` for buttons and `theme::panel_radius` for cards.
- Reset the display font inside shared components; callers may leave a custom
  M5GFX font selected.
- Clear the full region before replacing variable-length text to prevent stale
  glyphs from longer prior values.

## Component behavior

| Component | Default | Active | Disabled/error |
|---|---|---|---|
| Header icon | Dark surface, primary border | Success border where applicable | Muted or danger border |
| Radio action | Semantic dark fill, muted outline | Success/warning/danger fill | Muted fill |
| Navigation row | Background with primary outline | Selected surface with success outline | Muted text |
| Status value | White primary value | Success where healthy | Warning/danger where action is required |

The shared audio header, Home dashboard, and generic radio controls are the
reference implementations. New dashboards should reuse those components and
tokens before introducing new hard-coded RGB565 values.

## Incremental migration

Existing dashboard-local colors are intentionally not changed in one sweep.
Move shared chrome first, capture the affected screens with `UI_DOC_SHOW` and
`UI_CAPTURE`, then migrate one dashboard per hardware-verified change. This
keeps visual cleanup separate from demodulator and SDIO risk.
