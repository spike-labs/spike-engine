import os
import sys
import codecs
from platform_methods import subprocess_main
from io import StringIO

def get_git_hash(gitfolder):
    githash = ""
    if os.path.isfile(".git"):
        module_folder = open(".git", "r").readline().strip()
        if module_folder.startswith("gitdir: "):
            gitfolder = module_folder[8:]

    if os.path.isfile(os.path.join(gitfolder, "HEAD")):
        head = open(os.path.join(gitfolder, "HEAD"), "r", encoding="utf8").readline().strip()
        if head.startswith("ref: "):
            ref = head[5:]
            # If this directory is a Git worktree instead of a root clone.
            parts = gitfolder.split("/")
            if len(parts) > 2 and parts[-2] == "worktrees":
                gitfolder = "/".join(parts[0:-2])
            head = os.path.join(gitfolder, ref)
            packedrefs = os.path.join(gitfolder, "packed-refs")
            if os.path.isfile(head):
                githash = open(head, "r").readline().strip()
            elif os.path.isfile(packedrefs):
                # Git may pack refs into a single file. This code searches .git/packed-refs file for the current ref's hash.
                # https://mirrors.edge.kernel.org/pub/software/scm/git/docs/git-pack-refs.html
                for line in open(packedrefs, "r").read().splitlines():
                    if line.startswith("#"):
                        continue
                    (line_hash, line_ref) = line.split(" ")
                    if ref == line_ref:
                        githash = line_hash
                        break
        else:
            githash = head
    return githash


def get_spike_version_info(module_version_string="", silent=False):
    import methods
    version_info = methods.get_version_info(module_version_string, silent)

    import spike_version
    version_info.update({
        "short_name": str(spike_version.short_name),
        "name": str(spike_version.name),
        "year": int(spike_version.year),
        "website": str(spike_version.website),

        # "build": str(build_name),
        # "module_config": str(version.module_config) + module_version_string,
        # "docs_branch": str(version.docs),

        "spike_major": int(spike_version.major),
        "spike_minor": int(spike_version.minor),
        "spike_patch": int(spike_version.patch),
        "spike_status": str(spike_version.status),
        "spike_module_config": str(spike_version.module_config)
    })

    version_info["git_hash"] = get_git_hash("../../.git/modules/godot")
    version_info["spike_git_hash"] = get_git_hash("../../.git")

    return version_info


def generate_spike_version_header(module_version_string=""):
    version_info = get_spike_version_info(module_version_string)

    # NOTE: It is safe to generate these files here, since this is still executed serially.

    f = open("../../engine/core/version_generated.gen.h", "w")
    f.write(
        """/* THIS FILE IS GENERATED DO NOT EDIT */
#ifndef VERSION_GENERATED_GEN_H
#define VERSION_GENERATED_GEN_H
#define VERSION_SHORT_NAME "{short_name}"
#define VERSION_NAME "{name}"
#define VERSION_MAJOR {major}
#define VERSION_MINOR {minor}
#define VERSION_PATCH {patch}
#define VERSION_STATUS "{status}"
#define VERSION_BUILD "{build}"
#define VERSION_MODULE_CONFIG "{module_config}"
#define VERSION_YEAR {year}
#define VERSION_WEBSITE "{website}"
#define VERSION_DOCS_BRANCH "{docs_branch}"
#define VERSION_DOCS_URL "https://docs.godotengine.org/en/" VERSION_DOCS_BRANCH

#define VERSION_SPIKE_MAJOR {spike_major}
#define VERSION_SPIKE_MINOR {spike_minor}
#define VERSION_SPIKE_PATCH {spike_patch}
#define VERSION_SPIKE_STATUS "{spike_status}"
// #define VERSION_SPIKE_MODULE_CONFIG "{spike_module_config}"

extern const char *const VERSION_SPIKE_HASH;

#endif // VERSION_GENERATED_GEN_H
""".format(
            **version_info
        )
    )
    f.close()

    fhash = open("../../engine/core/version_hash.gen.cpp", "w")
    fhash.write(
        """/* THIS FILE IS GENERATED DO NOT EDIT */
#include "core/version.h"
const char *const VERSION_HASH = "{git_hash}";
const char *const VERSION_SPIKE_HASH = "{spike_git_hash}";
""".format(
            **version_info
        )
    )
    fhash.close()

def make_spike_license_header(target, source, env):
    dst = target[0]
    code_str = StringIO()
    for f in source:
        fname = str(f)
        basename = os.path.splitext(os.path.basename(fname))[0]
        with open(fname, "r") as fr:
            bom = fr.read(3)
            if bom != codecs.BOM_UTF8:
                fr.seek(0)
            homepage = fr.readline().rstrip()
            license = ""
            for l in fr.readlines():
                line = l.rstrip()
                if len(line) > 0:
                    license = license + line.replace('"', '\\"') + " "
                else:
                    license = license + "\\n\\n"
            code_str.write("    { \"%s\", \"%s\", \"%s\", },\n" % (basename, homepage, license))

    s = StringIO()
    s.write(
        """/* THIS FILE IS GENERATED DO NOT EDIT */

#pragma once

struct SpikeLicenseInfo {
	const char* name;
	const char* homepage;
    const char* license;
};

static const struct SpikeLicenseInfo SPIKE_LICENSE[] = {
""")
    s.write(code_str.getvalue())
    s.write("    { nullptr, nullptr, nullptr },")
    s.write("};")

    with open(dst, "w") as f:
        f.write(s.getvalue())

    s.close()
    code_str.close()

if __name__ == "__main__":
    subprocess_main(globals())