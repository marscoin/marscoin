#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Check preprocessor tests of build-configuration macros.

1. A file that tests a macro defined by configure (AC_DEFINE in configure.ac)
   must include <config/bitcoin-config.h> itself. Otherwise the macro is
   undefined in that translation unit and the test is silently false.
2. A file must not test an ENABLE_* macro that nothing defines any more.
"""

import re
import subprocess
import sys

EXCLUDED_DIRS = (
    "src/crc32c/", "src/crypto/ctaes/", "src/crypto/randomx_vendor/", "src/crypto/slhdsa/",
    "src/crypto/oqs_vendor/", "src/leveldb/", "src/minisketch/", "src/secp256k1/", "src/univalue/",
)
CONFIG_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]config/bitcoin-config\.h[>"]', re.M)
CONDITIONAL = re.compile(r'^\s*#\s*(?:if|ifdef|ifndef|elif)\b(.*)$', re.M)
IDENTIFIER = re.compile(r'\b([A-Z][A-Z0-9_]{2,})\b')


def git_ls(*patterns):
    return subprocess.check_output(["git", "ls-files", "--", *patterns], text=True).split()


def configure_macros():
    text = open("configure.ac", encoding="utf8").read()
    return set(re.findall(r'AC_DEFINE(?:_UNQUOTED)?\(\s*\[?([A-Z][A-Z0-9_]+)', text))


def compiler_flag_macros():
    """Macros passed as -D compiler flags by the Makefiles (set per library, not via the config header)."""
    macros = set()
    for path in git_ls("src/Makefile*.am", "src/Makefile*.include"):
        macros.update(re.findall(r'-D([A-Z][A-Z0-9_]+)', open(path, encoding="utf8").read()))
    return macros


def defined_in_sources(files):
    defined = set()
    for path in files:
        try:
            text = open(path, encoding="utf8", errors="replace").read()
        except OSError:
            continue
        defined.update(re.findall(r'^\s*#\s*define\s+([A-Z][A-Z0-9_]+)', text, re.M))
    return defined


def main():
    files = [f for f in git_ls("src/*.cpp", "src/*.h", "src/*.c") if not f.startswith(EXCLUDED_DIRS)]
    config = configure_macros() - compiler_flag_macros()
    source_defines = defined_in_sources(git_ls("src/*.cpp", "src/*.h", "src/*.c"))
    errors = []
    for path in files:
        text = open(path, encoding="utf8", errors="replace").read()
        used = set()
        for cond in CONDITIONAL.findall(text):
            used.update(IDENTIFIER.findall(cond))
        config_used = sorted(used & config)
        if config_used and not CONFIG_INCLUDE.search(text) and not path.endswith("config/bitcoin-config.h"):
            errors.append(f"{path}: tests {', '.join(config_used)} but does not include <config/bitcoin-config.h>")
        for macro in sorted(m for m in used if m.startswith("ENABLE_")):
            if macro not in config and macro not in source_defines and macro not in compiler_flag_macros():
                errors.append(f"{path}: tests {macro}, which nothing defines")
    for line in errors:
        print(line)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
