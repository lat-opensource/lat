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
- version tag pushes, including `1.6.8`, `v1.6.8`, and `1.6.8-rc1`;
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

## Automatic releases

After these workflow changes are integrated, update `VERSION` in the candidate
commit and push its version tag to `lat-opensource/lat`. The repository's existing
numeric tag convention is supported, as are optional `v` prefixes and prerelease
suffixes. For example, tag `1.6.8` requires `VERSION` to contain `1.6.8`;
`1.6.8-rc1` accepts `VERSION` containing either `1.6.8` or `1.6.8-rc1`.
Mismatches fail before building. Existing tags are not republished automatically.

**Build and Release LAT** then runs the full GCC matrix and invokes **LAT Tests**
and **Clang Compile** as reusable workflows at the same commit. The final
**Full release validation** job requires all five GCC configurations, all three
test containers, and successful build/test/Clang jobs. Failed, cancelled or
skipped dependencies cannot pass this gate. Its summary records the tested SHA.

Only after that gate passes does **Publish verified release** create a GitHub
Release on the upstream repository. It downloads packages from this run only,
checks their commit/version, and gives the three packages unique names:

```text
lat-1.6.8-aosc-loongarch64.tar.xz
lat-1.6.8-debian-loongarch64.tar.xz
lat-1.6.8-fedora-loongarch64.tar.xz
SHA256SUMS
```

The publisher verifies that the remote tag still resolves to the tested commit
(including annotated tags), creates a draft with a CI link and release notes,
uploads all four assets, rechecks the tag, and finally publishes the draft.
Tags with a suffix such as `-rc1` produce GitHub prereleases. Normal version tags
produce stable releases. Only the publishing job receives `contents: write`;
the repository's built-in `GITHUB_TOKEN` is sufficient.

If validation fails, no release is created. If uploading fails, the release
remains a draft. Rerunning the failed job resumes a draft created by this
workflow for the same commit. A completed release is not overwritten; reruns
verify that its expected assets exist. Maintainer-created drafts or releases
are not modified. A moved/deleted tag prevents publication.

Tag/manual candidate runs have 19 build/test jobs, two selection jobs and one
final validation job. Automatic upstream tag releases add one publishing job.
The automatic flow publishes the validated candidate; finish any separate
native acceptance work before pushing a stable version tag.

## Optional validation without publication

The Actions page's manual **Build and Release LAT** entry point remains useful
for checking a candidate branch or tag before deciding to release. It runs the
same full validation gate but does not publish, even when a tag is selected.
Push a new version tag when ready to use the automatic publication flow.

The `release: published` trigger is an additional check for releases published
outside this flow; it is not a pre-publication gate. Releases created using
`GITHUB_TOKEN` do not start another release-event run. These workflows use QEMU
LoongArch containers and do not replace native LoongArch compatibility,
integration or performance acceptance testing.

## Avoiding repeated work

Each PR workflow cancels older runs of the same workflow and PR when a new run
starts. Different PRs do not cancel one another. Master, scheduled, manual and
release runs are not cancelled by this policy. GCC push builds run only on
`master` or version tags, and assigning a PR no longer triggers a build.

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
python3 -B tests/ci/test_lat_ci_release.py
actionlint .github/workflows/release.yml .github/workflows/tests.yml \
  .github/workflows/clang.yml .github/workflows/build-docker-images.yml
git diff --check
```

These validate selection logic and workflow definitions. Actual queue time,
cache hit rates, image publication and build/test completion must be measured
in GitHub Actions after rollout; job-count reduction is not a timing guarantee.
