# Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None
#
# PlatformIO post script: links against a patched copy of the framework's sections.ld
# that moves some newlib code from IRAM to flash.
#
# Why: with the Bluetooth controller (libbtdm_app, ~33 KB IRAM) linked next to Wi-Fi and
# PSRAM, .iram0.text overflows by ~4 KB. The prebuilt Arduino libs set every
# CONFIG_SPIRAM_CACHE_LIB*_IN_IRAM option; with the PSRAM workaround the ROM newlib can't
# be used, and these options put the libc copies in IRAM instead. For the time functions
# (mktime, localtime, tzset, ...) and the stdio buffer helpers this is only a speed choice:
# nothing calls them with the flash cache disabled (ISRs print with ets_printf from ROM).
# Memory and string functions (LIBMEM, LIBSTR) stay in IRAM: ISRs do call memcpy.
#
# The framework file is not changed; the copy lives in the build directory and is
# regenerated on every build, so a framework update is picked up automatically. If the
# expected lines are missing the script fails the build instead of linking blind.

import os
import re

Import("env")  # noqa: F821  (PlatformIO SCons global)

# newlib objects (libc_a-<name>.o) moved to flash.
MOVE = [
    # CONFIG_SPIRAM_CACHE_LIBTIME_IN_IRAM
    "asctime", "asctime_r", "ctime", "ctime_r", "gettzinfo", "gmtime", "gmtime_r",
    "lcltime", "lcltime_r", "mktime", "strftime", "timelocal", "tzcalc_limits",
    "tzset", "tzset_r",
    # CONFIG_SPIRAM_CACHE_LIBIO_IN_IRAM
    "fflush", "fputwc", "fvwrite", "makebuf", "refill", "sccl", "ungetc", "wbuf",
    "wcrtomb", "wctomb_r", "wsetup",
]


def find_sections_ld():
    for p in env.get("LIBPATH", []):  # noqa: F821
        cand = os.path.join(env.subst(str(p)), "sections.ld")  # noqa: F821
        if os.path.isfile(cand):
            return cand
    return None


def patch(text):
    tokens = ["*libc.a:libc_a-%s.*" % n for n in MOVE]
    out = []
    removed_iram = 0
    for line in text.splitlines(keepends=True):
        s = line.strip()
        # IRAM placement of the object's code: drop the line.
        m = re.match(r"^\*libc\.a:libc_a-([a-z_0-9]+)\.\*\(\.literal .*\.text", s)
        if m and m.group(1) in MOVE:
            removed_iram += 1
            continue
        # Flash .text: stop excluding these objects, so they land there.
        if "EXCLUDE_FILE" in line and s.endswith(".text.*)"):
            for t in tokens:
                line = line.replace(" " + t, "").replace("(" + t + " ", "(")
        out.append(line)
    return "".join(out), removed_iram


src = find_sections_ld()
if not src:
    raise SystemExit("iram_trim.py: sections.ld not found in LIBPATH")
with open(src, "r", encoding="utf-8") as f:
    patched, n = patch(f.read())
if n < len(MOVE) // 2:
    raise SystemExit("iram_trim.py: only %d of %d IRAM lines found in %s; check the framework version"
                     % (n, len(MOVE), src))

out_dir = os.path.join(env.subst("$BUILD_DIR"), "ld")  # noqa: F821
os.makedirs(out_dir, exist_ok=True)
dst = os.path.join(out_dir, "sections.ld")
with open(dst, "w", encoding="utf-8") as f:
    f.write(patched)

def flatten(items):
    for it in items:
        if isinstance(it, (list, tuple)):
            yield from flatten(it)
        else:
            yield it


# The Arduino builder nests the flag files as lists inside LINKFLAGS.
flags = list(flatten(env["LINKFLAGS"]))  # noqa: F821
replaced = False
for i, fl in enumerate(flags):
    if fl == "sections.ld" and i > 0 and flags[i - 1] == "-T":
        flags[i] = dst
        replaced = True
    elif fl == "-Tsections.ld":
        flags[i] = "-T" + dst
        replaced = True
if not replaced:
    raise SystemExit("iram_trim.py: -T sections.ld not found in LINKFLAGS")
env.Replace(LINKFLAGS=flags)  # noqa: F821
print("iram_trim.py: %d newlib objects moved from IRAM to flash (%s)" % (n, dst))
