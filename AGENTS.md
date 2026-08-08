# AGENTS.md — OrcaSlicer-ZR / ZR Spectrum

Agent operating rules for this **product** repository. Adapted from Ledgerful’s `AGENTS.md` shape;
stack is **C++17 / CMake / wxWidgets / Catch2**, not Rust/Cargo.

**Claude.md** is `@AGENTS.md` — keep this file as the single product SoT for agent rules.

---

## Product vs planning

| Path | Role |
|------|------|
| `C:\dev\Orca\OrcaSlicer-ZR\` (**this repo**) | Public AGPL product code only |
| `C:\dev\Orca\` (outside product) | Planning, ADRs, research, conductor |
| `C:\dev\Orca\conductor\` | Track `spec.md` / `plan.md` / `review.md`, `conductor.md`, `deferred.md` |

**Never** commit planning docs, ADRs, wayfinder, `SHARED-UNDERSTANDING`, or conductor trees into
this repo. Read them; do not ship them.

Mission: **ZR Spectrum** — inject Full Spectrum Near-Parity into an OrcaSlicer fork; v1 done =
CMYK multi-mix showpiece on Windows. Port onto **Orca pin**; compare to Snapmaker FS pin — do not
use Snapmaker as long-term base.

---

## Plan fidelity (implementors)

When executing a conductor track:

```
plan_fidelity{
  source: "C:\\dev\\Orca\\conductor\\<track>\\{spec.md,plan.md}"
  must:
    - implement DoD and plan phases as written
    - stay inside in-scope / out-of-scope from spec
    - not invent alternate architecture, extra features, or “while we’re here” refactors
  may:
    - follow live code layout when aspirational paths in the plan are wrong (note drift in review.md)
    - fix obvious compile/test breakages caused by this work
  must_not:
    - expand milestone scope (e.g. start FS port on a green-build track)
    - re-litigate ADRs or invent product decisions
    - skip plan phases without recording why and owner-approved re-scope
  if_plan_wrong_or_blocked:
    - stop and report; ask owner or re-plan — do not silently rewrite the plan in code
}
```

Reviews flag **scope creep** and **missing plan phases** as defects (medium+ when they affect DoD).

---

## Deferred (implementor owns at finish)

```
deferred{
  path: "C:\\dev\\Orca\\conductor\\deferred.md"
  at_start:
    - read entire register
    - absorb related open lows into this track if already claimed in spec §9, else do not expand scope
  at_finish_mandatory:
    - APPEND every residual low that was not implemented (review/codex P3, easy skips, verification gaps)
    - include date · area · track · sev=low · finding · why deferred · follow-up
    - resolve (strike-through) rows this track fully landed
  never:
    - silently drop a low
    - put medium/high/critical in deferred as a substitute for fixing (those block completion)
  verification_gap:
    - reviewer "verification gap, not a defect in this diff" → deferred.md row before merge if low;
      if it leaves DoD unmet → block (not deferrable)
}
```

Completing a track without updating `deferred.md` for unfinished lows is a process failure.

---

## PowerShell (Windows default)

```
powershell{
  forbid: "&& | [[ | ]] | then | fi | done | echo -e"
  prefer: "Get-ChildItem | Get-Content | Test-Path | Join-Path | Copy-Item | Remove-Item"
  rules:
    - use $_ and object properties for pipelines
    - use backslashes for shell-level Windows paths
    - avoid Bash shims for complex logic
    - chain commands with ; or separate lines
}
```

---

## Ledgerful (when inited in this repo)

Init and run **only** from `C:\dev\Orca\OrcaSlicer-ZR` — never planning root.

```
ledgerful{
  before:
    - Daily 5 = doctor / change-context / ledger status / search / verify --scope fast — see skill Daily 5
    - ledgerful doctor (prefer --json when parsing)
    - ledgerful audit
    - ledgerful ledger status --compact (or --json)
    - ledgerful change-context --json for meaningful code/config/policy edits
    - escalate: ledgerful scan --impact --json only on B2 triggers (readSetCapped, high multi-module risk,
      unclear public surface, user/DoD requires full impact, change-context error/not-ready —
      NOT solely status:empty)
    - prefer --json when parsing CLI stdout
    - prefer --auto-index on search|ask|hotspots|dead-code when index may be stale
  edit:
    - do not edit .ledgerful state files
    - inspect hotspots and temporal couplings when signals exist
  after:
    - ledgerful verify --scope fast during work if configured; else native ctest/build
    - report risk, verification, pending tx, drift when tools used
  skip_for:
    - format-only
    - scratch files
    - binary/media-only
    - explicit user bypass
  fail:
    unavailable: "continue with native checks; report missing signals"
    drift: "reconcile/adopt before continuing unless user says otherwise"
    verify: "report exact failed command and justified fallback"
  note: "C++ symbol indexing may be weak vs Rust; do not invent symbol hits — read code"
}
```

```
ledger{
  start: "ledgerful ledger start <entity> --category <CATEGORY> --message <intent>"
  commit: "ledgerful ledger commit <tx-id> --summary <what> --reason <why>"
  status: "ledgerful ledger status --compact"
  categories: "ARCHITECTURE | FEATURE | BUGFIX | REFACTOR | INFRA | SECURITY | TOOLING | DOCS | CHORE"
  optional: "use when ledgerful is inited and provenance is desired; not required if tool not inited"
}
```

If not inited: skip ledgerful CLIs; note in track `review.md`.

---

## AI-Brains (when inited in this repo)

```
aibrains{
  cwd: "C:\\dev\\Orca\\OrcaSlicer-ZR only"
  preflight: "ai-brains preflight --summary"
  pre_edit: "ai-brains preflight --summary before risky edits"
  query: "ai-brains sync query \"<query>\""
  recall: "ai-brains recall \"<query>\" --semantic"
  pin: "ai-brains pin \"<DECISION/CONSTRAINT/HOTSPOT: message>\""
}
```

---

## Verify / build / test (C++ — not Cargo)

```
verify{
  scope: "targeted during work; track DoD + PR CI before finalizing"
  windows_primary:
    - x64 Native Tools / VS Desktop C++
    - CMake 4.x before Strawberry on PATH
    - build_release_vs.bat  (or deps then slicer)
  cmake_build_examples:
    - "Windows: cmake --build . --config RelWithDebInfo --target ALL_BUILD -- -m  (from configured build dir)"
    - "Linux: cmake --build build --config RelWithDebInfo --target all --"
    - "macOS: cmake --build build/arm64 --config RelWithDebInfo --target all --"
  tests:
    - Catch2 under tests/ — see tests/AGENTS.md for placement
    - ctest --output-on-failure
    - ctest --test-dir ./tests/libslic3r
    - ctest --test-dir ./tests/fff_print
  hygiene:
    - no secrets
    - no .env commits
    - no planning-tree files in product commits
    - remove temporary output before finish unless required
  never:
    - claim CI green without observing it
    - --no-verify / skip hooks unless user explicitly requests
  ci:
    - PR CI ~3 minutes — do not busy-poll; wait ~3–4 min or gh pr checks --watch once
    - squash-merge only when CI green
}
```

Map full gate to the **track DoD** (e.g. green Windows + smoke for 0001; not every track needs a
cold 40‑minute rebuild if DoD is narrower — but do not under-claim).

---

## C++ standards (this codebase)

```
cpp{
  standard: "C++17, selective C++20"
  style:
    - PascalCase classes/types
    - snake_case functions and variables
    - #pragma once for headers
    - prefer smart pointers and RAII
    - TBB parallelization — be mindful of shared state
  gui:
    - SetSizerAndFit(sizer) on top-level windows
    - if SetSizer must run before full layout, call sizer->SetSizeHints(window) after
  forbid_patterns:
    - silent behavior change when an option is disabled
    - drive-by refactors outside the track plan
    - bulk-merge Snapmaker product mass as base
  deps: "built separately in deps/build/, then linked to main app"
}
```

### Key entry points

| Area | Path |
|------|------|
| App startup | `src/OrcaSlicer.cpp` |
| Slicing pipeline | `src/libslic3r/Print.cpp` |
| Print/printer/material settings | `src/libslic3r/PrintConfig.cpp` |
| GUI | `src/slic3r/GUI/` |
| Core algorithms | `src/libslic3r/` (`GCode/`, `Fill/`, `Support/`, `Geometry/`, `Format/`, `Arachne/`) |
| Printer profiles | `resources/profiles/[manufacturer].json` |

### Critical constraints

- **Backward compatibility** for `.3mf` project files and printer profiles
- **Cross-platform** intent — changes must not knowingly break Windows, macOS, Linux (v1 *priority* is Windows-first per ADR-0007; still avoid platform-hostile code)
- Profile/format changes need version migration handling
- Features gated by options must not affect existing behavior when those options are disabled

### Code review focus

- No regressions in existing functionality, defaults, profiles, or project compatibility
- Follow existing style and architecture; justify architectural changes in comments + PR body
- Prefer reuse over new helpers; avoid duplication
- Keep code concise; simplify bloated AI-generated code before review
- Targeted tests or documented verification for behavior changes (slicing, profiles, formats, GUI defaults)

---

## Localization & translations

Catalogs: `localization/i18n/<lang>/OrcaSlicer_<lang>.po`; template `OrcaSlicer.pot`.  
Guides: [Localization guide](https://github.com/OrcaSlicer/OrcaSlicer_WIKI/blob/main/guides/localization_guide.md),
[glossary](https://github.com/OrcaSlicer/OrcaSlicer_WIKI/blob/main/guides/localization_glossary.md).

### Terminology

- Glossary is SoT for recurring terms; brand names / G-code / formats often stay English
- Translate meaning, not word-for-word (`Flow ratio` ≠ `Flow Rate` ≠ `Flow Dynamics`)
- Reuse one template per recurring message shape

### Editing rules

- Only edit `msgstr` — **never** change `msgid` or “fix” English only in the translation
- Preserve placeholders, `\n`, spaces, HTML, `℃`, encoding/line endings
- **Never reorder** positional arguments in `c-format` strings
- Read `msgctxt` for homonyms (`Back`/`Top` senses differ)
- Disambiguate in source (`_L_CONTEXT` / `_u8L_CONTEXT`), not only in translation
- Literal `%` that confuses xgettext: fix in source with xgettext comments — do not mangle msgstr
- Plurals: honor catalog `nplurals` (not always 2)
- AI translations: `# AI Translated` comment; don’t reflow unrelated entries

### Verifying

- `scripts/run_gettext.bat --full` (Windows) must exit 0 when touching i18n pipeline
- Or `msgfmt --check-format -o <out>.mo localization/i18n/<lang>/OrcaSlicer_<lang>.po`
- Clear `fuzzy` after correcting an entry or the fix never ships

---

## Git

```
git{
  forbid:
    - push to main/master for track delivery without owner exception
    - force-push without explicit approval
    - destructive operations without explicit approval
    - committing secrets/.env
    - committing planning-tree paths into this product repo
  require:
    - inspect diff before commit
    - commit only intentional files
    - keep unrelated fixes separate where practical
    - feature branch + PR; CI green before squash-merge
}
```

---

## Review / clearance (tracks)

```
review{
  log: "C:\\dev\\Orca\\conductor\\<track>\\review.md"
  plan_source: "spec.md + plan.md — deviation is a finding"
  critical_high_medium: "must be verified_fixed before clearance"
  regression_caused_by_work: "high; never deferrable"
  low: "fix if easy; else APPEND to deferred.md (mandatory at finish)"
  closure: "code change alone is not closure — review + DoD evidence"
  cross_model: "codex-review skill; fresh clean pass as final gate (no open >low)"
}
```

---

## Contracts / profiles / formats

```
contracts{
  required_when:
    - printer or filament profile schema changes
    - .3mf / project format changes
    - public print config option semantics change
    - G-code flavor or toolchange contract changes
  update:
    - resources/profiles as appropriate
    - version migration if format/profile version bumps
    - user-facing docs only if product README/help requires it
  missing: "high finding when compatibility breaks silently"
}
```

No separate frontend contract pair (unlike Ledgerful dashboard) — slicer + profiles + G-code are the product surface.

---

## Stop before (ask owner)

```
stop_before:
  - destructive git operation
  - force-push
  - push to main/master (default track flow uses PR)
  - missing secrets / tokens
  - ambiguous product scope not resolvable from code + plan (AMS, multi-OS required, Snapmaker-as-base, etc.)
  - broad unrelated failures
  - unsafe dependency upgrade
  - scope exceeds current track
  - plan is wrong and requires re-scope (do not freestyle)
```

---

## Unrelated failures

```
unrelated_failures{
  fix_only_if: "obvious + low-risk + blocking validation"
  otherwise: "document and report"
  commit: "separate where practical"
}
```

---

## Skills (this repo)

| Skill | Path |
|-------|------|
| onboarding | `.agents/skills/onboarding` |
| implement | `.agents/skills/implement` |
| codex-review | `.agents/skills/codex-review` |
| ledgerful | `.agents/skills/ledgerful` |
| ai-brains | `.agents/skills/ai-brains` |

Planning-only skills live under `C:\dev\Orca\.agents\skills\` (`plan`, `review-track`, planning onboarding).
