# Evidence and closure

Fix closure is a claim about a defined scope, not a synonym for compiling,
passing one test, receiving a green check, or merging a pull request. Report
each evidence class independently and limit every conclusion to the evidence
actually satisfied.

## Evidence classes

These classes are ordered for reporting, but they are not interchangeable and
do not always run sequentially.

| Evidence | What it can establish | What it does not establish by itself |
| --- | --- | --- |
| Source and history | The suspected mechanism, protected invariant, and fit of the proposed change | That the code builds or the path executes |
| Build | The pinned source compiles and links under the recorded configuration | Runtime correctness or use of the changed path |
| Focused regression | A controlled RED becomes GREEN and relevant negative or fallback cases pass | General compatibility or the original application result |
| LoongArch target | Target-specific build or runtime behavior on a pinned LoongArch environment | Coverage of a real application or the supported product matrix |
| Real application | The original supported workload reaches its expected terminal behavior on the tested identity | Untested configurations, workloads, or release acceptance |
| CI | Required automated checks pass for the exact tested source identity | Tests absent from CI, independent review, or product acceptance |
| Independent review | The pinned diff has been checked against tracked requirements, standards, and protected behavior | Runtime evidence or owner acceptance |
| Product acceptance | The required evidence for the declared support scope is complete and the responsible owner accepts any stated residual risk | Broader support not included in that scope |

A pass in one evidence class must not be reported as a different class.
Preserve repeatable negative results even when another gate passes;
contradictions are evidence to investigate, not results to average away.

## Identify support scope and acceptance ownership

Derive a support claim from current tracked evidence such as project
documentation, build or test configuration, release policy, an accepted issue,
or an explicit maintainer decision. A historical chat, stale plan, or successful
fallback is not a support source.

Name the maintainer or product owner who is authorized to accept the declared
scope and any residual risk. `CODEOWNERS` or file history may help find that
person, but neither constitutes acceptance. If no current support source or
responsible owner can be identified, report that gap and do not use
**product-accepted**.

## Bind CI and review to source identity

For CI, record the exact commit tested, the required checks, and their terminal
statuses. If the service tests a generated merge revision, record both the pull
request head and the tested merge revision. A green result for an earlier head
does not validate later commits, and queued, skipped, cancelled, or rerun checks
must not be reported as passed.

Keep mergeability, DCO, approvals, and other repository status gates separate
from behavioral evidence. They may block integration without proving or
disproving the fix.

For review, pin the base and head of the diff. If the head changes after review,
state which findings still apply and obtain new review for behavior not covered
by the pinned diff. Review approval does not replace missing runtime gates.

## Choose an honest terminal state

Use one of these states and name every remaining gate:

- **Continue investigating:** the original failure, root cause, actual path, or
  proposed fix is not established, or the evidence is contradictory.
- **Evidence-bounded diagnosis:** the source, synthetic, or environment-limited
  evidence is valid, but a required target-runtime or field gate is unavailable;
  state the highest evidence class reached and keep the missing gate open.
- **Fix validated within scope:** the declared evidence and acceptance criteria
  pass for the pinned identity, but a broader integration, CI, review,
  real-application, or product gate remains.
- **Product-accepted:** the required real path, any required CI and review
  gates, supported scope, and responsible owner acceptance are complete.

An unavailable environment can explain why a gate is not run; it cannot turn
that gate into a pass. Likewise, a moved error or partial improvement can be a
useful result without being fix closure.

## Prepare a continuation handoff

Make the handoff self-contained. Include:

- expected behavior, observed failure, support source, and accepted scope;
- pinned source, worktree, build, binary, runtime, switches, cache, environment,
  workload, and exact commands;
- confirmed root cause, remaining hypotheses, and protected prior behavior;
- changed files and the intended general invariant;
- RED/GREEN evidence, actual-path sentinel, original-application terminal
  result, and decoded exit or signal namespaces;
- an evidence table marking each class **passed**, **failed**, **not run**, or
  **blocked**, with artifact locations and concise results;
- pull request head, exact CI-tested identity and terminal checks, and the pinned
  review range when those gates apply; and
- remaining gates, next safe experiment or command, required environment,
  responsible owner, and any authorization boundary.

Do not require the next developer to reconstruct essential facts from chat or
from mutable branch names. Keep failures and incomplete gates in the handoff;
do not rewrite them as success because ownership is changing.
