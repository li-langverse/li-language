"""Li dark theme for matplotlib — X-ready 16:9 figures."""

from __future__ import annotations

from pathlib import Path

# Canvas
FIG_W = 16
FIG_H = 9
FIG_DPI = 200
RETINA_SCALE = 2

# Palette
BG = "#0d1117"
PANEL = "#161b22"
TEXT = "#e6edf3"
MUTED = "#8b949e"
PRIMARY = "#58a6ff"
PASS = "#3fb950"
FAIL = "#f85149"
WARN = "#d29922"

LANG_COLORS = {
    "li": PRIMARY,
    "cpp": "#f0883e",
    "rust": "#ffa657",
    "julia": "#a371f7",
    "python": "#79c0ff",
    "python+numpy": "#56d364",
}


def apply_theme():
    import matplotlib.pyplot as plt

    plt.rcParams.update(
        {
            "figure.facecolor": BG,
            "axes.facecolor": PANEL,
            "axes.edgecolor": "#30363d",
            "axes.labelcolor": TEXT,
            "text.color": TEXT,
            "xtick.color": MUTED,
            "ytick.color": MUTED,
            "grid.color": "#21262d",
            "grid.alpha": 0.8,
            "font.size": 13,
            "axes.titlesize": 20,
            "axes.titleweight": "bold",
            "legend.facecolor": PANEL,
            "legend.edgecolor": "#30363d",
            "figure.dpi": FIG_DPI,
        }
    )


def brand_figure(title: str, subtitle: str = ""):
    import matplotlib.pyplot as plt

    apply_theme()
    fig, ax = plt.subplots(figsize=(FIG_W, FIG_H))
    fig.subplots_adjust(left=0.08, right=0.96, top=0.88, bottom=0.12)
    ax.set_title(title, loc="left", pad=16, color=TEXT)
    if subtitle:
        fig.text(0.08, 0.92, subtitle, fontsize=12, color=MUTED)
    fig.text(0.96, 0.03, "Li · li-language", ha="right", fontsize=10, color=MUTED)
    return fig, ax


def save_share(fig, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(
        path,
        facecolor=BG,
        edgecolor="none",
        bbox_inches="tight",
        dpi=FIG_DPI * RETINA_SCALE,
    )
    return path
