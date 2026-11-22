"""Makes reportlab importable for the report build.

The system Python does not ship reportlab, and installing into it would need
root. The first run creates a private virtualenv next to this file and installs
reportlab into it, so the build stays reproducible without touching the system.
"""

import os
import subprocess
import sys

VENV_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".venv")


def ensure_reportlab():
    try:
        import reportlab  # noqa: F401

        return
    except ImportError:
        pass

    venv_python = os.path.join(VENV_DIR, "bin", "python")
    if not os.path.exists(venv_python):
        print("creating %s (reportlab is not installed)" % VENV_DIR, file=sys.stderr)
        subprocess.run([sys.executable, "-m", "venv", VENV_DIR], check=True)
        subprocess.run(
            [venv_python, "-m", "pip", "install", "--quiet", "reportlab"],
            check=True,
        )

    # Re-exec inside the venv so the rest of the build sees reportlab normally.
    os.execv(
        venv_python,
        [venv_python, os.path.abspath(sys.argv[0])] + sys.argv[1:],
    )
