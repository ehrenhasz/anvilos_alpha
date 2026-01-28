import os
import os.path
import sys
import subprocess
repourl = "https://gitlab.freedesktop.org/%s.git" % os.environ["CI_MERGE_REQUEST_PROJECT_PATH"]
os.environ["GIT_DEPTH"] = "1000"
subprocess.call(["git", "remote", "remove", "check-patch"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
subprocess.check_call(["git", "remote", "add", "check-patch", repourl])
subprocess.check_call(["git", "fetch", "check-patch", os.environ["CI_MERGE_REQUEST_TARGET_BRANCH_NAME"]],
                      stdout=subprocess.DEVNULL,
                      stderr=subprocess.DEVNULL)
ancestor = subprocess.check_output(["git", "merge-base",
                                    "check-patch/%s" % os.environ["CI_MERGE_REQUEST_TARGET_BRANCH_NAME"], "HEAD"],
                                   universal_newlines=True)
ancestor = ancestor.strip()
log = subprocess.check_output(["git", "log", "--format=%H %s",
                               ancestor + "..."],
                              universal_newlines=True)
subprocess.check_call(["git", "remote", "rm", "check-patch"])
if log == "":
    print("\nNo commits since %s, skipping checks\n" % ancestor)
    sys.exit(0)
errors = False
print("\nChecking all commits since %s...\n" % ancestor, flush=True)
ret = subprocess.run(["scripts/checkpatch.pl",
                      "--terse",
                      "--types", os.environ["CHECKPATCH_TYPES"],
                      "--git", ancestor + "..."])
if ret.returncode != 0:
    print("    ❌ FAIL one or more commits failed scripts/checkpatch.pl")
    sys.exit(1)
sys.exit(0)
