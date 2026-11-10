# Stamps FW_VERSION (CI env var, else git short SHA) into project sources for GET /version.
Import("projenv")
import os, subprocess

def fw_version():
    if os.environ.get("FW_VERSION"):
        return os.environ["FW_VERSION"]
    cwd = projenv["PROJECT_DIR"]
    try:
        sha = subprocess.check_output(["git", "rev-parse", "--short=12", "HEAD"], cwd=cwd, text=True).strip()
        dirty = subprocess.call(["git", "diff", "--quiet", "HEAD"], cwd=cwd) != 0
        return sha + ("-dirty" if dirty else "")
    except Exception:
        return "unknown"

projenv.Append(CPPDEFINES=[("FW_VERSION", projenv.StringifyMacro(fw_version()))])
