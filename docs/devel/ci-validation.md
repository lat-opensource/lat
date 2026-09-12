# CI validation tiers

Ordinary source pull requests run eight build/test jobs:

- AOSC, Debian and Fedora each run `build32 -c -a` and `build64 -c -a`.
  These retain the existing AVX options and the 64-bit script's KZT option.
- Debian runs `lat-pr-fast` with debug mode and ASan/UBSan enabled.
- Fedora runs the Clang build for both guest architectures.

The separate `build32-dbg`, `build64-dbg`, and `build-release` jobs are omitted
from ordinary PR coverage. The sanitizer test job deliberately retains debug
mode. Two short selection jobs run in addition to the eight build/test jobs.

## Full coverage

Full coverage retains all 15 GCC build jobs, all three `lat-pr-fast` jobs and
the Clang build. It runs on:

- pushes to `master`;
- the daily schedule at 18:00 UTC (02:00 China Standard Time);
- manual workflow runs;
- published releases, as a post-publication check;
- PRs changing build definitions, CI workflows/images/helpers, runtime
  installation code, or preprocessor directives.

The shared selector is `scripts/ci/lat_ci_matrix.py`. It checks the complete
paginated PR file list, including previous paths of renamed files, and added
or removed preprocessor directives. Unchanged preprocessor context alone does
not expand coverage. Missing/truncated patches, incomplete lists and API
failures select full coverage rather than silently dropping checks. This is a conservative
heuristic; changes to code inside an existing conditional block may still need
a manually requested full run.

## Before publishing a release

Run **Build and Release LAT** manually on the exact candidate branch or tag.
It runs the full GCC matrix and calls **LAT Tests** and **Clang Compile** at the
same revision. Wait for the entire run to succeed before publishing, and rerun
validation if the candidate changes. This workflow uploads artifacts but does
not publish a GitHub release.

The release-event trigger is a post-publication check. QEMU containers do not
replace native LoongArch compatibility, integration or performance acceptance.

## Avoiding repeated work

Each PR workflow cancels older runs of the same workflow and PR when a new run
starts. Different PRs do not cancel one another. Master, scheduled, manual and
release runs are not cancelled by this policy. GCC push builds run only on
`master`, and assigning a PR no longer triggers a build.

Test and Clang jobs persist ccache separately from product build caches, with
keys separating the container, sanitizer mode where applicable, Dockerfiles and
commit. Prefix restoration reuses prior compilations, and the container prints
ccache statistics even when compilation or testing fails. Meson build trees are
not restored across fresh runners. The Fedora and Clang images include ccache;
the test job can install it in older Fedora images during rollout.

The image builder publishes the existing image tags plus tags suffixed with
the SHA-256 of each Dockerfile. Clang first pulls its matching Dockerfile tag.
If that tag is not available (including a new Dockerfile in a PR), it builds the
checked-out Dockerfile locally. Thus the first run may still build an image;
subsequent runs use the image after the master image-builder run publishes it.
The Clang ccache identity includes the real compiler binaries and the wrapper,
so changing the toolchain behind the wrapper invalidates cached compilations.

## Local workflow checks

```sh
python3 -B tests/ci/test_lat_ci_matrix.py
python3 -B tests/ci/test_lat_ci_workflows.py
actionlint .github/workflows/release.yml .github/workflows/tests.yml \
  .github/workflows/clang.yml .github/workflows/build-docker-images.yml
git diff --check
```

These validate selection logic and workflow definitions. Actual queue time,
cache hit rates, image publication and build/test completion must be measured
in GitHub Actions after rollout; job-count reduction is not a timing guarantee.
