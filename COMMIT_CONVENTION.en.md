# Git Commit Convention (derived from existing history)

To keep commits traceable, follow the rules below based on patterns already used in the project.

## Recommended title format
- **Suggested structure**: `<scope>[, <type>]: <concise description>`.
- **Scope**: module or subproject name (for example `LATX`, `lat`, `KZT`, `linux-user`), consistent with the path or subsystem.
- **Type**: common values are `feat` (feature), `fix` (bug fix), `opt` (optimization); for special cases use `temporary fix` (short-term workaround) or `Build(deps)` (dependency or CI maintenance).
- **Subject**: one short imperative/declarative English phrase summarizing the change; do not end with a period.

## Type suggestions (extend as needed)
| Type             | Meaning / scope                                           | Example title snippet               |
| ---------------- | --------------------------------------------------------- | ----------------------------------- |
| `LATX, infra`    | Infrastructure, build system, CI/CD changes not affecting product behavior | `LATX, infra: Add tracing for ...` |
| `LATX, feat`     | Product-facing feature additions                          | `LATX, feat: introduce smc ...` |
| `LATX, fix`      | Bug fixes                                                 | `LATX, fix: Handle null ...`       |
| `LATX, opt`      | Performance or implementation optimizations               | `LATX, opt: Refine glue ...`       |
| `LATX, refactor` | Function/feature refactors                                | `LATX, refactor: Split ...`        |
| `LATX, style`    | Formatting or comment tweaks that do not change behavior  | `LATX, style: Reflow comments`     |
| `LATX-X64, *`    | Changes specific to 64-bit; `*` matches the types above (e.g., fix) | `LATX-X64, fix: ...`               |
| `linux-user`     | linux-user related (e.g., syscall changes)                | `linux-user, fix: ...`             |
| `LATX, docs`     | Documentation                                              | `LATX, docs: Update convention`    |
| Other historical types | Special cases like `temporary fix` or `Build(deps)` are also allowed | `Build(deps): Bump ...`            |

> Separate scope and type with a comma, and separate type and description with a colon. If no type is used, keep only the colon, for example `cpus: Make {start,end}_exclusive() recursive`.

### `feat` vs `infra` decision guide

#### `feat` — feature-level additions

**If the change gives the translator a new visible capability or behavior, it is `feat`.**

Typical cases:
- New APIs, instruction translation, or instruction support
- New command-line options or feature flags
- New business logic or user-visible behavior

**Decision rule:**
> “Would users care if this appears in the release notes?”
**Yes → feat**

---

#### `infra` — engineering infrastructure

**Changes to engineering environment, build system, CI/CD, toolchains, or development scripts that do not alter product behavior.**

Typical cases:
- Modifications to Makefile / CMake / Meson / Bazel
- Developer-only tracing or analysis tools

**Decision rule:**
> “Can users perceive any behavior change?”
**No → infra**

## Title examples (copy as-is)
- `LATX, feat: Support CONFIG_LATX_GLUE_MASK in indirect jmp glue`
- `LATX, opt: Reduce thread contention in fast path`
- `Build(deps): Update meson to 1.3.0`

## Commit body requirements
1. **Prefer keeping a body**: avoid empty bodies except for `style`, `docs`, or trivial commits where the first line fully explains the change.
2. **`fix` must name the target**: state which application/test set or git issue is fixed to aid traceability.
3. **Body content suggestions**:
   - Provide background, motivation, solution, and validation; no need to expand details in the title.
   - Keep a single commit focused on one topic; avoid grouping unrelated changes.
   - For temporary workarounds or risky changes, describe impact scope, alternatives, and cleanup plan to ease follow-up.

## Pre-push checklist
- Keep the title short and without a trailing period.
- Does it include a scope consistent with the subsystem?
- Does the type match the scenario? Should you mark it as `temporary fix` or `Build(deps)`?
- Is the description clear about the change/reason, avoiding vague phrases like “update code” or “fix bug”?
- Is the commit body filled in as required (especially for `fix`)?
- If it is a temporary solution or needs follow-up, is TODO / follow-up noted in the body?
