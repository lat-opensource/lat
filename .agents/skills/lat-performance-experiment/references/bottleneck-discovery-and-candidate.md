# Bottleneck discovery and candidate formation

Read this reference when the task starts from a performance problem rather than
an already defined candidate. The output is either an evidence-backed candidate
contract or a bounded conclusion that no justified candidate exists yet.

## Frame the product question

Record the scenario, workload and input, expected completion, primary metric,
regression budget, and the decision the measurement must support. Distinguish
startup, cold translation or AOT generation, warm execution, steady state, and
shutdown when their costs matter differently.

Repeat the unmodified baseline enough to characterize normal variation and
failure behavior. If the baseline is unstable or the workload does not reach
the expected endpoint, repair the experiment before optimizing code.

## Select evidence by question

Choose observations that separate the current hypotheses with the least
perturbation. The following are evidence types, not a mandatory tool sequence:

- endpoint timing and completion evidence for the product symptom;
- sampling or call-stack evidence for where CPU time accumulates;
- PMU evidence for cycles, instructions, branches, cache behavior, or stalls;
- LAT counters or traces for translated-block frequency, code expansion,
  helpers, flag work, dispatch, or cache behavior;
- guest and generated-host disassembly for a hot translated sequence;
- syscall, loader, memory, synchronization, or external-I/O evidence when time
  may be outside generated code; and
- artifact and load sentinels when AOT or another reusable cache is involved.

Prefer existing observability. Add narrow temporary instrumentation only when
it distinguishes a named hypothesis, measure its overhead, and remove it from
the final candidate unless observability itself is the approved deliverable.

## Attribute before optimizing

Locate the material cost at the narrowest justified boundary. Possible
boundaries include the guest workload, loader or syscall layer, translation
front end, IR optimization, host code generation, TB dispatch or linking,
generated host sequence, memory and page handling, cache or branch behavior,
synchronization, and external waiting.

Rank opportunities by measured contribution, product relevance, semantic risk,
and likely implementation cost. Static occurrence counts or large-looking host
sequences identify candidates; weight them by execution and workload evidence
before claiming they are bottlenecks.

## Write a falsifiable candidate contract

Before editing, state:

- the evidence that identifies the cost;
- the invariant and current mechanism responsible for it;
- the smallest proposed change;
- the mechanism metric expected to change;
- the expected end-to-end effect for the declared workload;
- correctness, fallback, capability, and cache-identity obligations; and
- the result that would reject the hypothesis or stop the direction.

Keep causal language conditional until the candidate changes the predicted
mechanism metric. A changed mechanism metric without end-to-end improvement is
a valid negative product result, not permission to invent a new explanation.

## Form the smallest useful candidate

When implementation is authorized, change one principal variable and keep the
candidate reversible. Without that authorization, deliver the candidate
contract and implementation plan without editing production code. Separate
instrumentation, behavior-neutral preparation, and the optimization so their
effects remain attributable. Do not hard-code an application, benchmark address,
or one machine's observed layout into product logic.

The candidate must expose a sentinel proving that the intended path ran and a
fallback or negative case showing that unsupported paths retain correct
behavior. The main Skill defines the subsequent correctness gates, isolated
paired measurement, and product decision.

## Stop or redirect honestly

- If the baseline is unstable, improve the experiment rather than tune code.
- If evidence points outside LAT, report that boundary instead of forcing a
  translator change.
- If no cost is material, stop the direction or select a more representative
  workload.
- If the candidate misses its predicted mechanism effect, reject or revise the
  hypothesis before broader benchmarking.
- If the mechanism works but the complete workload does not improve, retain the
  bounded mechanism conclusion and keep the product gate closed.
