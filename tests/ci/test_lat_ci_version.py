#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the real Meson version rules and AOT footer macros in isolation.

Requires Meson, Ninja, ccache and a native C compiler. This checks build/cache
semantics; it does not execute the LoongArch translator or an AOT workload.
"""

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def version_rules():
    meson = (ROOT / "meson.build").read_text()
    start = meson.index("latx_version = get_option('latx_version')")
    return meson[start:meson.index("# The path to glib.h", start)]


class VersionBuild:
    def __init__(self, directory, debug=False, rules=None, ordinary_count=1):
        self.source = directory / "source"
        self.build = directory / "build"
        self.source.mkdir(parents=True)
        compiler = f"{shutil.which('ccache')} {shutil.which('cc')}"
        self.env = dict(os.environ, CC=compiler,
                        CCACHE_DIR=str(directory / "cache"),
                        CCACHE_COMPILERCHECK="content")
        config = directory / "ccache.conf"
        config.write_text("direct_mode = true\n")
        self.env["CCACHE_CONFIGPATH"] = str(config)
        aot = (ROOT / "target/i386/latx/include/aot.h").read_text()
        footer = re.search(
            r"#ifdef CONFIG_LATX_DEBUG\n#define AOT_VERSION.*?#endif",
            aot, re.DOTALL).group()
        header = '#ifndef LATX_VERSION\n#include "latx-version.h"\n#endif\n'
        (self.source / "version.c").write_text(
            header + 'const char *version(void) { return LATX_VERSION; }\n')
        (self.source / "footer.c").write_text(
            header + footer +
            '\nconst char *footer(void) { return AOT_VERSION; }\n')
        (self.source / "main.c").write_text(
            '#include <stdio.h>\nconst char *version(void);\n'
            'const char *footer(void);\nint ordinary_0(void);\n'
            'int main(void) { printf("%s\\n%s\\n%d\\n", '
            'version(), footer(), ordinary_0()); return 0; }\n')
        sources = ["main.c", "version.c", "footer.c"]
        for index in range(ordinary_count):
            name = f"ordinary_{index}.c"
            (self.source / name).write_text(
                '#include <stdint.h>\n#include <stdlib.h>\n'
                '#include <string.h>\n'
                '#include <stdio.h>\n#include "qemu/queue.h"\n'
                f'int ordinary_{index}(void) {{ return 42; }}\n')
            sources.append(name)
        project = ("project('lat-version-probe', 'c', "
                   "default_options: ['buildtype=release'])\n")
        project += rules if rules is not None else version_rules()
        if debug:
            project += ("add_project_arguments('-DCONFIG_LATX_DEBUG', "
                        "language: 'c')\n")
        project += "executable('probe', "
        project += ", ".join(repr(name) for name in sources)
        project += (", include_directories: "
                    f"include_directories('{ROOT / 'include'}'))\n")
        (self.source / "meson.build").write_text(project)
        (self.source / "meson_options.txt").write_text(
            "option('latx_version', type: 'string', value: '')\n")

    def run(self, *args):
        result = subprocess.run(
            args, env=self.env, cwd=self.source, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f"{args!r}\n{result.stdout}")
        return result.stdout

    def configure(self, version, reconfigure=False):
        args = ["meson", "setup"]
        if reconfigure:
            args.append("--reconfigure")
        return self.run(*args, str(self.build), str(self.source),
                        f"-Dlatx_version={version}")

    def compile(self):
        return self.run("ninja", "-C", str(self.build), "-j2")

    def output(self):
        return self.run(str(self.build / "probe")).splitlines()

    def clean_objects(self):
        self.run("ninja", "-C", str(self.build), "-t", "clean")

    def reset_stats(self):
        self.run("ccache", "--zero-stats")

    def stats(self):
        rows = self.run("ccache", "--print-stats").splitlines()
        return {name: int(value) for name, value in
                (line.split() for line in rows)}


@unittest.skipUnless(
    all(shutil.which(tool) for tool in ("meson", "ninja", "ccache", "cc")),
    "requires meson, ninja, ccache and cc")
class VersionCacheTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="lat-version-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def test_version_change_keeps_unrelated_objects_in_incremental_build(self):
        fixture = VersionBuild(self.root)
        fixture.configure("version-a")
        fixture.compile()
        fixture.configure("version-b", reconfigure=True)
        output = fixture.compile()
        self.assertNotIn("ordinary_0.c.o", output)
        self.assertNotIn("main.c.o", output)
        self.assertEqual(fixture.output(),
                         ["version-b", "Version: version-b-release", "42"])

    def test_clean_build_direct_hits_survive_version_change(self):
        fixture = VersionBuild(self.root)
        fixture.configure("version-a")
        fixture.compile()
        fixture.configure("version-b", reconfigure=True)
        fixture.clean_objects()
        fixture.reset_stats()
        fixture.compile()
        self.assertGreaterEqual(fixture.stats()["direct_cache_hit"], 2)
        self.assertEqual(fixture.output()[0], "version-b")

    def test_debug_and_release_footer_identity_remains_versioned(self):
        for debug, suffix in ((False, "release"), (True, "debug")):
            with self.subTest(debug=debug):
                fixture = VersionBuild(self.root / suffix, debug=debug)
                fixture.configure("old-version")
                fixture.compile()
                old_footer = fixture.output()[1]
                fixture.configure("new-version", reconfigure=True)
                fixture.compile()
                new_footer = fixture.output()[1]
                self.assertEqual(old_footer, f"Version: old-version-{suffix}")
                self.assertEqual(new_footer, f"Version: new-version-{suffix}")
                self.assertNotEqual(old_footer, new_footer)


if __name__ == "__main__":
    unittest.main()
