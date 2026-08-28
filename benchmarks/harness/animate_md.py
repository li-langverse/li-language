#!/usr/bin/env python3
"""Export md_lennard_jones trajectories and render particle animations (all languages).

Trajectories are read frame-by-frame from disk (no ``read_text`` / full-file RAM).
Grid views keep one open handle per panel. ``temp-x`` / ``temp-sequence`` show a 2×2
language grid per temperature (4 langs: cpp, rust, julia, li — native traj shared for the
first three). Use ``--max-frames`` for quick previews. 3D GIFs are memory-heavy at the
matplotlib layer; prefer smaller ``--steps`` / larger ``--stride`` for grid exports.

``temp-x`` timing (defaults tuned for ~80s full video = 4 × 20s segments):

- ``--temp-hold`` (default 20s): wall time per temperature segment, including ``--hold-init``.
- ``--hold-init`` (default 1.25s): FCC init freeze at the start of each segment.
- Exported frames ≈ ``1 + max_steps // stride`` (see ``md_core.c``); dynamics wall time
  ≈ ``(frames - 1) / fps`` after the init hold.
- Target: enough exported frames to cover ``int(fps * temp_hold) - hold_n`` dynamics indices
  (e.g. 30 fps × 20s − 37 hold frames → ~563 data frames → ``--steps 9000 --stride 16``).
- If the trajectory is shorter, the last frame is held for the remainder of the segment.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HARNESS = Path(__file__).resolve().parent
MD_DIR = REPO / "benchmarks" / "tier2_physics" / "md_lennard_jones"
BUILD_DIR = REPO / "build" / "bench" / "md_lennard_jones"
TRAJ_DIR = REPO / "benchmarks" / "results" / "md_lennard_jones"
DEFAULT_OUT = REPO / "benchmarks" / "results" / "share"
LANGS = ("cpp", "rust", "julia", "li")
LANG_LABELS = {"cpp": "C++", "rust": "Rust", "julia": "Julia", "li": "Li"}
# 2x2 panel order: cpp | rust / julia | li
GRID_LAYOUT: tuple[tuple[str, tuple[int, int]], ...] = (
    ("cpp", (0, 0)),
    ("rust", (0, 1)),
    ("julia", (1, 0)),
    ("li", (1, 1)),
)
# 2x2 temperature sweep (reduced LJ T numerically = Kelvin-style labels)
TEMP_PANELS: tuple[tuple[str, float, str, tuple[int, int]], ...] = (
    ("T0", 0.0, "0 K", (0, 0)),
    ("T10", 10.0, "10 K", (0, 1)),
    ("T100", 100.0, "100 K", (1, 0)),
    ("T10000", 10000.0, "10000 K", (1, 1)),
)

sys.path.insert(0, str(HARNESS))
from plot_theme import BG, LANG_COLORS, MUTED, PANEL, PRIMARY, TEXT, apply_theme  # noqa: E402

LI_MD_BOX_DEFAULT = 10.0


def build_native(bin_path: Path) -> None:
    cc = os.environ.get("CC", "clang")
    subprocess.check_call(
        [
            cc,
            "-O3",
            "-march=native",
            "-ffast-math",
            str(MD_DIR / "cpp" / "md_main.c"),
            str(MD_DIR / "common" / "md_core.c"),
            "-lm",
            "-o",
            str(bin_path),
        ],
        cwd=REPO,
    )


def build_li_traj(bin_path: Path) -> None:
    lic = REPO / "build" / "compiler" / "lic" / "lic"
    if not lic.is_file():
        raise RuntimeError(f"lic missing at {lic} — run ./scripts/build.sh")
    extra = f"{MD_DIR / 'common' / 'md_core.c'} {MD_DIR / 'common' / 'md_traj_env.c'}"
    env = {**os.environ, "LI_EXTRA_C": extra}
    subprocess.check_call(
        [
            str(lic),
            "build",
            str(MD_DIR / "li" / "traj.li"),
            "-o",
            str(bin_path),
            "--release",
            "-O2",
            "-march=native",
        ],
        cwd=REPO,
        env=env,
    )


def _hold_frame_count(fps: int, hold_init_sec: float) -> int:
    if hold_init_sec <= 0:
        return 0
    return max(1, int(fps * hold_init_sec))


def _frame_copy(fr: dict) -> dict:
    return {
        "step": fr["step"],
        "px": list(fr["px"]),
        "py": list(fr["py"]),
        "pz": list(fr["pz"]),
        "speed": list(fr["speed"]),
        "hold": fr.get("hold", False),
    }


def _hold_frame_from(first: dict) -> dict:
    fr = _frame_copy(first)
    fr["hold"] = True
    return fr


def _frame_at_anim_index(
    idx: int,
    hold_n: int,
    hold_fr: dict,
    f0: dict,
    reader: TrajectoryReader,
    *,
    data_i: list[int],
    cached: list[dict],
) -> dict:
    """Map animation index → data frame without pre-expanding hold duplicates."""
    if hold_n > 0 and idx < hold_n:
        return hold_fr
    target = idx - hold_n
    while data_i[0] < target:
        nfr = reader.read_frame()
        if nfr is None:
            break
        data_i[0] += 1
        cached[0] = nfr
    return cached[0]


def _temp_k_label(temp: float) -> str:
    if temp <= 0.0:
        return "0 K"
    if temp >= 1000 and abs(temp - round(temp)) < 1e-9:
        return f"{int(round(temp))} K"
    if abs(temp - round(temp)) < 1e-9:
        return f"{int(round(temp))} K"
    return f"{temp:g} K"


def _step_subtitle(fr: dict, meta: dict, dt: float, *, panel_label: str | None = None) -> str:
    if fr.get("hold"):
        tlabel = panel_label if panel_label is not None else _temp_k_label(float(meta.get("temp", 1.0)))
        return f"FCC initialization · {tlabel}"
    t = fr["step"] * dt
    return f"step {fr['step']} · t = {t:.2f}"


def export_trajectory(
    lang: str,
    traj_path: Path,
    *,
    stride: int,
    max_steps: int,
    temperature: float | None = None,
) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    native_bin = BUILD_DIR / "md_lj_native"
    li_bin = BUILD_DIR / "md_lj_li_traj"
    if lang == "li":
        build_li_traj(li_bin)
        cmd = [str(li_bin)]
        note = "lic + md_core.c"
    else:
        build_native(native_bin)
        cmd = [str(native_bin)]
        note = "shared md_core.c (native)"
    env = {
        **os.environ,
        "LI_MD_TRAJ": str(traj_path.resolve()),
        "LI_MD_TRAJ_STRIDE": str(stride),
        "LI_MD_TRAJ_STEPS": str(max_steps),
    }
    if temperature is not None:
        env["LI_MD_TEMP"] = str(temperature)
    subprocess.check_call(cmd, cwd=REPO, env=env)
    temp_note = f", T={temperature}" if temperature is not None else ""
    print(f"wrote {traj_path} [{lang}, {note}{temp_note}]")


def _parse_header_line(meta: dict, line: str) -> None:
    parts = line.split()
    if len(parts) != 2:
        return
    key, val = parts[0], parts[1]
    if key in ("n", "stride", "max_steps", "ncell"):
        meta[key] = int(val)
    elif key == "init":
        meta[key] = val
    else:
        meta[key] = float(val)


class TrajectoryReader:
    """Line-at-a-time trajectory reader; one frame in memory at a time."""

    def __init__(self, path: Path) -> None:
        self.path = path
        self._fh = None
        self.meta: dict = {
            "n": 256,
            "box": LI_MD_BOX_DEFAULT,
            "dt": 0.004,
            "temp": 1.0,
            "init": "fcc",
        }
        self._n = 256

    def open(self) -> TrajectoryReader:
        self._fh = self.path.open(encoding="utf-8")
        self._read_header()
        return self

    def close(self) -> None:
        if self._fh is not None:
            self._fh.close()
            self._fh = None

    def __enter__(self) -> TrajectoryReader:
        return self.open()

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def _read_header(self) -> None:
        assert self._fh is not None
        while True:
            line = self._fh.readline()
            if not line:
                break
            stripped = line.strip()
            if stripped == "---":
                break
            if stripped and not stripped.startswith("li_md"):
                _parse_header_line(self.meta, stripped)
        self._n = int(self.meta["n"])

    def read_frame(self) -> dict | None:
        assert self._fh is not None
        while True:
            line = self._fh.readline()
            if not line:
                return None
            if line.startswith("F "):
                step = int(line.split()[1])
                break
        px, py, pz, vx, vy, vz = [], [], [], [], [], []
        for _ in range(self._n):
            row = [float(x) for x in self._fh.readline().split()]
            px.append(row[0])
            py.append(row[1])
            pz.append(row[2])
            vx.append(row[3])
            vy.append(row[4])
            vz.append(row[5])
        speed = [(vx[j] ** 2 + vy[j] ** 2 + vz[j] ** 2) ** 0.5 for j in range(self._n)]
        return {"step": step, "px": px, "py": py, "pz": pz, "speed": speed}

    def iter_frames(self):
        while True:
            fr = self.read_frame()
            if fr is None:
                break
            yield fr


def scan_trajectory_stats(
    path: Path,
    *,
    max_frames: int | None = None,
) -> tuple[dict, int, float]:
    """Streaming pass: metadata, data-frame count, global speed vmax."""
    with TrajectoryReader(path) as reader:
        vmax = 0.0
        count = 0
        for fr in reader.iter_frames():
            vmax = max(vmax, max(fr["speed"]))
            count += 1
            if max_frames is not None and count >= max_frames:
                break
        if count == 0:
            raise ValueError(f"no frames in {path}")
        return dict(reader.meta), count, vmax


def scan_multi_trajectory_stats(
    paths: dict[str, Path],
    *,
    max_frames: int | None = None,
) -> tuple[dict, int, float]:
    """Per-path streaming scan; synced frame count = min across paths."""
    ref_meta: dict | None = None
    counts: list[int] = []
    vmax = 0.0
    for _key, path in paths.items():
        meta, count, v = scan_trajectory_stats(path, max_frames=max_frames)
        if ref_meta is None:
            ref_meta = meta
        counts.append(count)
        vmax = max(vmax, v)
    assert ref_meta is not None
    return ref_meta, min(counts), vmax


def read_first_frame(path: Path) -> tuple[dict, dict]:
    with TrajectoryReader(path) as reader:
        fr = reader.read_frame()
        if fr is None:
            raise ValueError(f"no frames in {path}")
        return dict(reader.meta), fr


def parse_trajectory(path: Path) -> tuple[dict, list[dict]]:
    """Load all frames (avoid for large files; prefer TrajectoryReader)."""
    with TrajectoryReader(path) as reader:
        return dict(reader.meta), list(reader.iter_frames())


def _plot_half(box: float) -> float:
    return box / 2.0


def _centered_plane_coords(fr: dict, half: float, plane: str) -> tuple[list[float], list[float]]:
    if plane == "xz":
        return [x - half for x in fr["px"]], [z - half for z in fr["pz"]]
    return [x - half for x in fr["px"]], [y - half for y in fr["py"]]


def _centered_plane_speed(
    fr: dict, half: float, plane: str
) -> tuple[list[float], list[float], list[float]]:
    x, y = _centered_plane_coords(fr, half, plane)
    return x, y, fr["speed"]


GRID_STYLES: dict[str, dict] = {
    # Default: plasma on dark panel, faint FCC lattice, soft box frame.
    "pleasing": {
        "cmap": "plasma",
        "marker_size": 15,
        "marker_alpha": 0.9,
        "edge_lw": 0.3,
        "box_color": "#484f58",
        "box_lw": 1.05,
        "box_ls": (0, (5, 4)),
        "box_alpha": 0.8,
        "fcc_guide": True,
        "fcc_alpha": 0.24,
        "fcc_color": "#3d444d",
        "axis_pad_frac": 0.06,
        "show_axis_grid": False,
        "title_fontsize": 14,
        "marker_size_3d": 11,
    },
    "classic": {
        "cmap": "cool",
        "marker_size": 16,
        "marker_alpha": 0.92,
        "edge_lw": 0.22,
        "box_color": MUTED,
        "box_lw": 1.0,
        "box_ls": "--",
        "box_alpha": 0.65,
        "fcc_guide": True,
        "fcc_alpha": 0.3,
        "fcc_color": "#30363d",
        "axis_pad_frac": 0.04,
        "show_axis_grid": True,
        "title_fontsize": 13,
        "marker_size_3d": 12,
    },
    "vivid": {
        "cmap": "viridis",
        "marker_size": 17,
        "marker_alpha": 0.95,
        "edge_lw": 0.35,
        "box_color": "#6e7681",
        "box_lw": 1.15,
        "box_ls": (0, (3, 2)),
        "box_alpha": 0.85,
        "fcc_guide": True,
        "fcc_alpha": 0.28,
        "fcc_color": "#484f58",
        "axis_pad_frac": 0.07,
        "show_axis_grid": False,
        "title_fontsize": 14,
        "marker_size_3d": 12,
    },
}


def _grid_style(name: str) -> dict:
    if name not in GRID_STYLES:
        raise ValueError(f"unknown grid style {name!r}; choose from {', '.join(GRID_STYLES)}")
    return GRID_STYLES[name]


def _axis_pad(half: float, style: dict) -> float:
    return half * float(style["axis_pad_frac"]) * 2.0


def _draw_fcc_guide_2d(ax, box: float, ncell: int, half: float, style: dict) -> None:
    if not style.get("fcc_guide") or ncell <= 0:
        return
    a = box / ncell
    for i in range(ncell + 1):
        t = i * a - half
        ax.axvline(t, color=style["fcc_color"], alpha=style["fcc_alpha"], linewidth=0.55, zorder=0)
        ax.axhline(t, color=style["fcc_color"], alpha=style["fcc_alpha"], linewidth=0.55, zorder=0)


def _style_grid_panel_2d(
    ax,
    *,
    box: float,
    half: float,
    plane: str,
    row: int,
    col: int,
    title: str,
    title_color: str,
    ncell: int,
    style: dict,
) -> None:
    pad = _axis_pad(half, style)
    ax.set_xlim(-half - pad, half + pad)
    ax.set_ylim(-half - pad, half + pad)
    ax.set_aspect("equal")
    ax.set_title(
        title,
        fontsize=style["title_fontsize"],
        fontweight="bold",
        color=title_color,
        pad=8,
    )
    _draw_fcc_guide_2d(ax, box, ncell, half, style)
    _draw_centered_box_2d(ax, box, style)
    if style.get("show_axis_grid"):
        ax.grid(True, color="#30363d", alpha=0.32, linewidth=0.4, linestyle="-", zorder=0)
    else:
        ax.grid(False)
    if row == 1:
        ax.set_xlabel("x (LJ)", color=MUTED, fontsize=11)
    if col == 0:
        ax.set_ylabel("y (LJ)" if plane == "xy" else "z (LJ)", color=MUTED, fontsize=11)


def _grid_scatter_2d(
    ax,
    fr: dict,
    half: float,
    plane: str,
    vmax: float,
    lang: str,
    style: dict,
):
    x, y, speed = _centered_plane_speed(fr, half, plane)
    edge = LANG_COLORS.get(lang, PRIMARY)
    return ax.scatter(
        x,
        y,
        c=speed,
        s=style["marker_size"],
        cmap=style["cmap"],
        vmin=0,
        vmax=vmax,
        alpha=style["marker_alpha"],
        edgecolors=edge,
        linewidths=style["edge_lw"],
        zorder=2,
    )


def _grid_scatter_3d(
    ax,
    fr: dict,
    half: float,
    vmax: float,
    lang: str,
    style: dict,
):
    x, y, z = _centered_xyz(fr, half)
    return ax.scatter(
        x,
        y,
        z,
        c=fr["speed"],
        s=style["marker_size_3d"],
        cmap=style["cmap"],
        vmin=0,
        vmax=vmax,
        alpha=style["marker_alpha"],
        depthshade=True,
        edgecolors=LANG_COLORS.get(lang, PRIMARY),
        linewidths=style["edge_lw"] * 0.6,
        zorder=2,
    )


def _scatter_offsets_2d(sc, fr: dict, half: float, plane: str) -> None:
    import numpy as np

    x, y, speed = _centered_plane_speed(fr, half, plane)
    sc.set_offsets(np.column_stack([x, y]))
    sc.set_array(np.array(speed))


def _draw_centered_box_2d(ax, box: float, style: dict | None = None) -> None:
    import matplotlib.pyplot as plt

    st = style or _grid_style("pleasing")
    half = _plot_half(box)
    rect = plt.Rectangle(
        (-half, -half),
        box,
        box,
        fill=False,
        edgecolor=st["box_color"],
        linewidth=st["box_lw"],
        linestyle=st["box_ls"],
        alpha=st["box_alpha"],
        zorder=1,
    )
    ax.add_patch(rect)


def _draw_periodic_box_3d(ax, box: float) -> None:
    """Wireframe cube for the simulation cell."""
    import numpy as np

    corners = np.array(
        [
            [0, 0, 0],
            [box, 0, 0],
            [box, box, 0],
            [0, box, 0],
            [0, 0, box],
            [box, 0, box],
            [box, box, box],
            [0, box, box],
        ]
    )
    edges = (
        (0, 1),
        (1, 2),
        (2, 3),
        (3, 0),
        (4, 5),
        (5, 6),
        (6, 7),
        (7, 4),
        (0, 4),
        (1, 5),
        (2, 6),
        (3, 7),
    )
    for a, b in edges:
        xs = [corners[a, 0], corners[b, 0]]
        ys = [corners[a, 1], corners[b, 1]]
        zs = [corners[a, 2], corners[b, 2]]
        ax.plot(xs, ys, zs, color=MUTED, linewidth=0.9, alpha=0.55)


def _draw_centered_box_3d(ax, box: float, style: dict | None = None) -> None:
    """Wireframe cube centered at origin ([-box/2, box/2] per axis)."""
    import numpy as np

    st = style or _grid_style("pleasing")
    half = _plot_half(box)
    pad = _axis_pad(half, st)
    corners = np.array(
        [
            [0, 0, 0],
            [box, 0, 0],
            [box, box, 0],
            [0, box, 0],
            [0, 0, box],
            [box, 0, box],
            [box, box, box],
            [0, box, box],
        ]
    ) - half
    edges = (
        (0, 1),
        (1, 2),
        (2, 3),
        (3, 0),
        (4, 5),
        (5, 6),
        (6, 7),
        (7, 4),
        (0, 4),
        (1, 5),
        (2, 6),
        (3, 7),
    )
    for a, b in edges:
        xs = [corners[a, 0], corners[b, 0]]
        ys = [corners[a, 1], corners[b, 1]]
        zs = [corners[a, 2], corners[b, 2]]
        ax.plot(
            xs,
            ys,
            zs,
            color=st["box_color"],
            linewidth=st["box_lw"],
            alpha=st["box_alpha"],
            linestyle=st["box_ls"] if isinstance(st["box_ls"], str) else "-",
        )
    lim = half + pad
    ax.set_xlim(-lim, lim)
    ax.set_ylim(-lim, lim)
    ax.set_zlim(-lim, lim)


def _centered_xyz(fr: dict, half: float) -> tuple[list[float], list[float], list[float]]:
    return (
        [x - half for x in fr["px"]],
        [y - half for y in fr["py"]],
        [z - half for z in fr["pz"]],
    )


def render_animation_3d(
    traj_path: Path,
    out_gif: Path,
    out_mp4: Path | None,
    *,
    fps: int,
    rotate: bool,
    lang: str,
    hold_init_sec: float = 0.0,
    max_frames: int | None = None,
) -> None:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import numpy as np
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    apply_theme()
    meta, n_data, vmax = scan_trajectory_stats(traj_path, max_frames=max_frames)
    hold_n = _hold_frame_count(fps, hold_init_sec)
    n_anim = hold_n + n_data
    box = float(meta["box"])
    half = _plot_half(box)
    dt = float(meta["dt"])

    fig = plt.figure(figsize=(10, 10))
    fig.patch.set_facecolor(BG)
    ax = fig.add_subplot(111, projection="3d")
    ax.set_facecolor("#161b22")
    ax.set_xlim(-half, half)
    ax.set_ylim(-half, half)
    ax.set_zlim(-half, half)
    ax.set_xlabel("x (LJ)")
    ax.set_ylabel("y (LJ)")
    ax.set_zlabel("z (LJ)")
    ax.set_box_aspect((1, 1, 1))
    _draw_centered_box_3d(ax, box)

    ncell = int(meta.get("ncell", 4))
    rho = meta.get("rho", meta.get("n", 256) / (box**3))
    driver = "lic + md_core.c" if lang == "li" else "shared md_core.c"
    tlabel = _temp_k_label(float(meta.get("temp", 1.0)))

    fig.suptitle(
        f"md_lennard_jones · {lang} · 3D",
        fontsize=16,
        fontweight="bold",
        color=TEXT,
        x=0.08,
        ha="left",
        y=0.98,
    )
    fig.text(
        0.08,
        0.94,
        f"N={int(meta['n'])} · FCC {ncell}³×4 · ρ={rho:.3f} · T={tlabel} · {driver} · rotating",
        fontsize=11,
        color=MUTED,
    )
    step_text = fig.text(0.96, 0.94, "", ha="right", fontsize=11, color=MUTED)
    fig.text(0.96, 0.03, f"{lang} · li-language", ha="right", fontsize=10, color=MUTED)

    with TrajectoryReader(traj_path) as reader:
        f0 = reader.read_frame()
        assert f0 is not None
        hold_fr = _hold_frame_from(f0) if hold_n > 0 else f0
        data_i = [0]
        cached = [f0]
        x0, y0, z0 = _centered_xyz(f0, half)
        sc = ax.scatter(
            x0,
            y0,
            z0,
            c=f0["speed"],
            s=12,
            cmap="cool",
            vmin=0,
            vmax=vmax,
            alpha=0.9,
            depthshade=True,
            edgecolors=LANG_COLORS.get(lang, PRIMARY),
            linewidths=0.12,
        )
        cbar = fig.colorbar(sc, ax=ax, fraction=0.03, pad=0.08, shrink=0.72)
        cbar.set_label("|v| (LJ)", color=TEXT)

        def update(idx: int):
            fr = _frame_at_anim_index(idx, hold_n, hold_fr, f0, reader, data_i=data_i, cached=cached)
            sc._offsets3d = _centered_xyz(fr, half)
            sc.set_array(np.array(fr["speed"]))
            step_text.set_text(_step_subtitle(fr, meta, dt))
            if rotate:
                ax.view_init(elev=22, azim=35 + idx * 0.55)
            return sc, step_text

        anim = animation.FuncAnimation(
            fig,
            update,
            frames=n_anim,
            interval=1000 // fps,
            blit=False,
        )

        out_gif.parent.mkdir(parents=True, exist_ok=True)
        anim.save(out_gif, writer=animation.PillowWriter(fps=fps))
        print(f"wrote {out_gif}")

        if out_mp4 is not None:
            try:
                anim.save(out_mp4, writer=animation.FFMpegWriter(fps=fps))
                print(f"wrote {out_mp4}")
            except Exception as exc:
                print(f"skip mp4 ({exc})", file=sys.stderr)

    plt.close(fig)


def render_animation(
    traj_path: Path,
    out_gif: Path,
    out_mp4: Path | None,
    *,
    fps: int,
    plane: str,
    lang: str,
    hold_init_sec: float = 0.0,
    max_frames: int | None = None,
) -> None:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import numpy as np

    apply_theme()
    meta, n_data, vmax = scan_trajectory_stats(traj_path, max_frames=max_frames)
    hold_n = _hold_frame_count(fps, hold_init_sec)
    n_anim = hold_n + n_data
    box = float(meta["box"])
    half = _plot_half(box)
    dt = float(meta["dt"])

    fig, ax = plt.subplots(figsize=(10, 10))
    fig.patch.set_facecolor(BG)
    ax.set_facecolor("#161b22")
    ax.set_xlim(-half, half)
    ax.set_ylim(-half, half)
    ax.set_aspect("equal")
    ax.set_xlabel("x (LJ)")
    ax.set_ylabel("y (LJ)" if plane == "xy" else "z (LJ)")
    _draw_centered_box_2d(ax, box)

    ncell = int(meta.get("ncell", 4))
    rho = meta.get("rho", meta.get("n", 256) / (box**3))
    tlabel = _temp_k_label(float(meta.get("temp", 1.0)))

    title = fig.suptitle(
        f"md_lennard_jones · {lang}",
        fontsize=16,
        fontweight="bold",
        color=TEXT,
        x=0.08,
        ha="left",
        y=0.98,
    )
    driver = "lic + md_core.c" if lang == "li" else "shared md_core.c"
    fig.text(
        0.08,
        0.94,
        f"N={int(meta['n'])} · FCC {ncell}³×4 · ρ={rho:.3f} · T={tlabel} · {driver}",
        fontsize=11,
        color=MUTED,
    )
    step_text = fig.text(0.96, 0.94, "", ha="right", fontsize=11, color=MUTED)
    fig.text(0.96, 0.03, f"{lang} · li-language", ha="right", fontsize=10, color=MUTED)

    with TrajectoryReader(traj_path) as reader:
        f0 = reader.read_frame()
        assert f0 is not None
        hold_fr = _hold_frame_from(f0) if hold_n > 0 else f0
        data_i = [0]
        cached = [f0]
        x0, y0 = _centered_plane_coords(f0, half, plane)
        sc = ax.scatter(
            x0,
            y0,
            c=f0["speed"],
            s=14,
            cmap="cool",
            vmin=0,
            vmax=vmax,
            alpha=0.85,
            edgecolors=LANG_COLORS.get(lang, PRIMARY),
            linewidths=0.15,
        )
        cbar = fig.colorbar(sc, ax=ax, fraction=0.046, pad=0.04)
        cbar.set_label("|v| (LJ)", color=TEXT)

        def update(idx: int):
            fr = _frame_at_anim_index(idx, hold_n, hold_fr, f0, reader, data_i=data_i, cached=cached)
            x, y = _centered_plane_coords(fr, half, plane)
            sc.set_offsets(np.column_stack([x, y]))
            sc.set_array(np.array(fr["speed"]))
            step_text.set_text(_step_subtitle(fr, meta, dt))
            return sc, step_text

        anim = animation.FuncAnimation(
            fig,
            update,
            frames=n_anim,
            interval=1000 // fps,
            blit=False,
        )

        out_gif.parent.mkdir(parents=True, exist_ok=True)
        anim.save(out_gif, writer=animation.PillowWriter(fps=fps))
        print(f"wrote {out_gif}")

        if out_mp4 is not None:
            try:
                anim.save(out_mp4, writer=animation.FFMpegWriter(fps=fps))
                print(f"wrote {out_mp4}")
            except Exception as exc:
                print(f"skip mp4 ({exc})", file=sys.stderr)

    plt.close(fig)
    _ = title


def _open_grid_readers(traj_paths: dict[str, Path]) -> dict[str, TrajectoryReader]:
    readers: dict[str, TrajectoryReader] = {}
    for lang, path in traj_paths.items():
        readers[lang] = TrajectoryReader(path).open()
    return readers


def _close_readers(readers: dict[str, TrajectoryReader]) -> None:
    for reader in readers.values():
        reader.close()


def render_grid_animation(
    traj_paths: dict[str, Path],
    out_gif: Path,
    out_mp4: Path | None,
    *,
    fps: int,
    plane: str = "xy",
    hold_init_sec: float = 0.0,
    max_frames: int | None = None,
    grid_style: str = "pleasing",
) -> None:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import numpy as np

    apply_theme()
    style = _grid_style(grid_style)
    meta, n_data, vmax = scan_multi_trajectory_stats(traj_paths, max_frames=max_frames)
    hold_n = _hold_frame_count(fps, hold_init_sec)
    n_anim = hold_n + n_data
    box = float(meta["box"])
    half = _plot_half(box)
    dt = float(meta["dt"])

    fig, axes = plt.subplots(2, 2, figsize=(14, 14))
    fig.patch.set_facecolor(BG)
    for ax in axes.flat:
        ax.set_facecolor(PANEL)

    ncell = int(meta.get("ncell", 4))
    rho = meta.get("rho", meta.get("n", 256) / (box**3))
    fig.suptitle(
        "md_lennard_jones · all languages",
        fontsize=16,
        fontweight="bold",
        color=TEXT,
        x=0.06,
        ha="left",
        y=0.98,
    )
    fig.text(
        0.06,
        0.955,
        f"N={int(meta['n'])} · FCC {ncell}³×4 · ρ={rho:.3f} · shared md_core.c (native + lic)",
        fontsize=11,
        color=MUTED,
    )
    step_text = fig.text(0.94, 0.955, "", ha="right", fontsize=11, color=MUTED)

    readers = _open_grid_readers(traj_paths)
    f0s: dict[str, dict] = {}
    hold_frs: dict[str, dict] = {}
    data_is: dict[str, list[int]] = {}
    cached: dict[str, list[dict]] = {}
    scatters: dict[str, object] = {}
    try:
        for lang, (row, col) in GRID_LAYOUT:
            ax = axes[row, col]
            _style_grid_panel_2d(
                ax,
                box=box,
                half=half,
                plane=plane,
                row=row,
                col=col,
                title=LANG_LABELS.get(lang, lang),
                title_color=LANG_COLORS.get(lang, TEXT),
                ncell=ncell,
                style=style,
            )

            f0 = readers[lang].read_frame()
            assert f0 is not None
            f0s[lang] = f0
            hold_frs[lang] = _hold_frame_from(f0) if hold_n > 0 else f0
            data_is[lang] = [0]
            cached[lang] = [f0]
            scatters[lang] = _grid_scatter_2d(ax, f0, half, plane, vmax, lang, style)

        cbar = fig.colorbar(
            scatters["cpp"], ax=axes.ravel().tolist(), fraction=0.02, pad=0.02, shrink=0.6
        )
        cbar.set_label("|v| (LJ)", color=TEXT)

        def update(idx: int):
            fr_step = None
            for lang, _ in GRID_LAYOUT:
                fr = _frame_at_anim_index(
                    idx,
                    hold_n,
                    hold_frs[lang],
                    f0s[lang],
                    readers[lang],
                    data_i=data_is[lang],
                    cached=cached[lang],
                )
                _scatter_offsets_2d(scatters[lang], fr, half, plane)
                fr_step = fr
            assert fr_step is not None
            step_text.set_text(_step_subtitle(fr_step, meta, dt))
            return (*scatters.values(), step_text)

        anim = animation.FuncAnimation(
            fig,
            update,
            frames=n_anim,
            interval=1000 // fps,
            blit=False,
        )

        out_gif.parent.mkdir(parents=True, exist_ok=True)
        anim.save(out_gif, writer=animation.PillowWriter(fps=fps))
        print(f"wrote {out_gif}")

        if out_mp4 is not None:
            try:
                anim.save(out_mp4, writer=animation.FFMpegWriter(fps=fps))
                print(f"wrote {out_mp4}")
            except Exception as exc:
                print(f"skip mp4 ({exc})", file=sys.stderr)
    finally:
        _close_readers(readers)

    plt.close(fig)


def render_temp_grid_animation(
    traj_paths: dict[str, Path],
    out_gif: Path,
    out_mp4: Path | None,
    *,
    fps: int,
    plane: str = "xy",
    hold_init_sec: float = 0.0,
    max_frames: int | None = None,
    grid_style: str = "pleasing",
) -> None:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import numpy as np

    apply_theme()
    style = _grid_style(grid_style)
    meta, n_data, vmax = scan_multi_trajectory_stats(traj_paths, max_frames=max_frames)
    hold_n = _hold_frame_count(fps, hold_init_sec)
    n_anim = hold_n + n_data
    first_key = TEMP_PANELS[0][0]
    box = float(meta["box"])
    half = _plot_half(box)
    dt = float(meta["dt"])

    fig, axes = plt.subplots(2, 2, figsize=(14, 14))
    fig.patch.set_facecolor(BG)
    for ax in axes.flat:
        ax.set_facecolor(PANEL)

    ncell = int(meta.get("ncell", 4))
    rho = meta.get("rho", meta.get("n", 256) / (box**3))
    fig.suptitle(
        "md_lennard_jones · temperature sweep",
        fontsize=16,
        fontweight="bold",
        color=TEXT,
        x=0.06,
        ha="left",
        y=0.98,
    )
    fig.text(
        0.06,
        0.955,
        f"N={int(meta['n'])} · FCC {ncell}³×4 · ρ={rho:.3f} · reduced LJ T (Kelvin-style labels)",
        fontsize=11,
        color=MUTED,
    )
    step_text = fig.text(0.94, 0.955, "", ha="right", fontsize=11, color=MUTED)

    readers = _open_grid_readers(traj_paths)
    f0s: dict[str, dict] = {}
    hold_frs: dict[str, dict] = {}
    data_is: dict[str, list[int]] = {}
    cached: dict[str, list[dict]] = {}
    scatters: dict[str, object] = {}
    try:
        for key, _temp, label, (row, col) in TEMP_PANELS:
            ax = axes[row, col]
            _style_grid_panel_2d(
                ax,
                box=box,
                half=half,
                plane=plane,
                row=row,
                col=col,
                title=label,
                title_color=TEXT,
                ncell=ncell,
                style=style,
            )

            f0 = readers[key].read_frame()
            assert f0 is not None
            f0s[key] = f0
            hold_frs[key] = _hold_frame_from(f0) if hold_n > 0 else f0
            data_is[key] = [0]
            cached[key] = [f0]
            scatters[key] = _grid_scatter_2d(ax, f0, half, plane, vmax, "cpp", style)

        cbar = fig.colorbar(
            scatters[first_key], ax=axes.ravel().tolist(), fraction=0.02, pad=0.02, shrink=0.6
        )
        cbar.set_label("|v| (LJ)", color=TEXT)

        def update(idx: int):
            fr_step = None
            panel = None
            for key, _temp, label, _pos in TEMP_PANELS:
                fr = _frame_at_anim_index(
                    idx,
                    hold_n,
                    hold_frs[key],
                    f0s[key],
                    readers[key],
                    data_i=data_is[key],
                    cached=cached[key],
                )
                _scatter_offsets_2d(scatters[key], fr, half, plane)
                fr_step = fr
                panel = label
            assert fr_step is not None and panel is not None
            step_text.set_text(_step_subtitle(fr_step, meta, dt, panel_label=panel))
            return (*scatters.values(), step_text)

        anim = animation.FuncAnimation(
            fig,
            update,
            frames=n_anim,
            interval=1000 // fps,
            blit=False,
        )

        out_gif.parent.mkdir(parents=True, exist_ok=True)
        anim.save(out_gif, writer=animation.PillowWriter(fps=fps))
        print(f"wrote {out_gif}")

        if out_mp4 is not None:
            try:
                anim.save(out_mp4, writer=animation.FFMpegWriter(fps=fps))
                print(f"wrote {out_mp4}")
            except Exception as exc:
                print(f"skip mp4 ({exc})", file=sys.stderr)
    finally:
        _close_readers(readers)

    plt.close(fig)


def _temp_lang_traj_paths(traj_dir: Path, key: str) -> dict[str, Path]:
    """Per-temperature paths: native traj shared by cpp/rust/julia; separate Li export."""
    native = traj_dir / f"traj_{key}.txt"
    li_path = traj_dir / f"traj_{key}_li.txt"
    if li_path.is_file():
        li = li_path
    else:
        print(
            f"note: {li_path.name} missing — Li panel uses native traj for {key}",
            file=sys.stderr,
        )
        li = native
    return {"cpp": native, "rust": native, "julia": native, "li": li}


def render_temp_lang_sequence_animation(
    traj_dir: Path,
    out_gif: Path,
    out_mp4: Path | None,
    *,
    fps: int,
    temp_hold: float = 20.0,
    plane: str = "xy",
    hold_init_sec: float = 1.25,
    max_frames: int | None = None,
    grid_style: str = "pleasing",
) -> None:
    """2×2 language grid per temperature (0 → 10 → 100 → 10000 K), ~temp_hold each.

    Four streaming readers per segment (native traj ×3 labels + Li). Trajectories for
    other temperatures are not opened until their segment starts. If exported frames are
    fewer than ``int(fps * temp_hold) - hold_n``, the last frame is held (no crash).
    """
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation

    apply_theme()
    style = _grid_style(grid_style)
    frames_per_temp = max(1, int(fps * temp_hold))
    hold_n = _hold_frame_count(fps, hold_init_sec)
    n_segments = len(TEMP_PANELS)
    n_anim = frames_per_temp * n_segments

    fig, axes = plt.subplots(2, 2, figsize=(14, 14))
    fig.patch.set_facecolor(BG)
    for ax in axes.flat:
        ax.set_facecolor(PANEL)

    subtitle = fig.text(0.5, 0.955, "", fontsize=11, color=MUTED, ha="center")
    step_text = fig.text(0.94, 0.955, "", ha="right", fontsize=11, color=MUTED)
    title = fig.suptitle(
        "md_lennard_jones",
        fontsize=22,
        fontweight="bold",
        color=TEXT,
        x=0.5,
        ha="center",
        y=0.98,
    )

    seg_state: dict = {
        "idx": -1,
        "readers": None,
        "f0s": None,
        "hold_frs": None,
        "data_is": None,
        "cached": None,
        "scatters": None,
        "meta": None,
        "vmax": 1.0,
        "n_data": 0,
        "label": "",
        "cbar": None,
    }

    def _close_segment() -> None:
        readers = seg_state["readers"]
        if readers:
            _close_readers(readers)
        seg_state["readers"] = None

    def _open_segment(seg_i: int) -> None:
        key, _temp, label, _pos = TEMP_PANELS[seg_i]
        _close_segment()
        paths = _temp_lang_traj_paths(traj_dir, key)
        meta, n_data, vmax = scan_multi_trajectory_stats(paths, max_frames=max_frames)
        box = float(meta["box"])
        half = _plot_half(box)
        ncell = int(meta.get("ncell", 4))
        rho = meta.get("rho", meta.get("n", 256) / (box**3))

        readers = _open_grid_readers(paths)
        f0s: dict[str, dict] = {}
        hold_frs: dict[str, dict] = {}
        data_is: dict[str, list[int]] = {}
        cached: dict[str, list[dict]] = {}
        existing = seg_state["scatters"]

        if existing is None:
            scatters: dict[str, object] = {}
            for lang, (row, col) in GRID_LAYOUT:
                ax = axes[row, col]
                _style_grid_panel_2d(
                    ax,
                    box=box,
                    half=half,
                    plane=plane,
                    row=row,
                    col=col,
                    title=LANG_LABELS.get(lang, lang),
                    title_color=LANG_COLORS.get(lang, TEXT),
                    ncell=ncell,
                    style=style,
                )
                f0 = readers[lang].read_frame()
                assert f0 is not None
                f0s[lang] = f0
                hold_frs[lang] = _hold_frame_from(f0) if hold_n > 0 else f0
                data_is[lang] = [0]
                cached[lang] = [f0]
                scatters[lang] = _grid_scatter_2d(ax, f0, half, plane, vmax, lang, style)
            cbar = fig.colorbar(
                scatters["cpp"],
                ax=axes.ravel().tolist(),
                fraction=0.02,
                pad=0.02,
                shrink=0.6,
            )
            cbar.set_label("|v| (LJ)", color=TEXT)
            seg_state["cbar"] = cbar
        else:
            scatters = existing
            for lang, _ in GRID_LAYOUT:
                f0 = readers[lang].read_frame()
                assert f0 is not None
                f0s[lang] = f0
                hold_frs[lang] = _hold_frame_from(f0) if hold_n > 0 else f0
                data_is[lang] = [0]
                cached[lang] = [f0]
                scatters[lang].set_clim(0, vmax)
                _scatter_offsets_2d(scatters[lang], f0, half, plane)

        title.set_text(f"md_lennard_jones · {label}")
        subtitle.set_text(
            f"N={int(meta['n'])} · FCC {ncell}³×4 · ρ={rho:.3f} · "
            f"2×2 languages (cpp/rust/julia = native md_core.c)"
        )

        seg_state.update(
            idx=seg_i,
            readers=readers,
            f0s=f0s,
            hold_frs=hold_frs,
            data_is=data_is,
            cached=cached,
            scatters=scatters,
            meta=meta,
            vmax=vmax,
            n_data=n_data,
            label=label,
        )

    _open_segment(0)

    def _lang_frame(lang: str, local: int) -> dict:
        if local < hold_n:
            return seg_state["hold_frs"][lang]
        k = local - hold_n
        n_data = seg_state["n_data"]
        data_idx = min(max(k, 0), n_data - 1)
        return _frame_at_anim_index(
            data_idx,
            0,
            seg_state["hold_frs"][lang],
            seg_state["f0s"][lang],
            seg_state["readers"][lang],
            data_i=seg_state["data_is"][lang],
            cached=seg_state["cached"][lang],
        )

    def update(idx: int):
        seg_i = idx // frames_per_temp
        local = idx % frames_per_temp
        if seg_state["idx"] != seg_i:
            _open_segment(seg_i)
        meta = seg_state["meta"]
        box = float(meta["box"])
        half = _plot_half(box)
        ncell = int(meta.get("ncell", 4))
        dt = float(meta["dt"])
        label = seg_state["label"]
        scatters = seg_state["scatters"]
        assert scatters is not None
        fr_step = None
        for lang, _ in GRID_LAYOUT:
            fr_lang = _lang_frame(lang, local)
            _scatter_offsets_2d(scatters[lang], fr_lang, half, plane)
            fr_step = fr_lang
        assert fr_step is not None
        step_text.set_text(_step_subtitle(fr_step, meta, dt, panel_label=label))
        return (*scatters.values(), step_text, title, subtitle)

    anim = animation.FuncAnimation(
        fig,
        update,
        frames=n_anim,
        interval=1000 // fps,
        blit=False,
    )

    try:
        out_gif.parent.mkdir(parents=True, exist_ok=True)
        anim.save(out_gif, writer=animation.PillowWriter(fps=fps))
        print(f"wrote {out_gif}")

        if out_mp4 is not None:
            try:
                anim.save(out_mp4, writer=animation.FFMpegWriter(fps=fps))
                print(f"wrote {out_mp4}")
            except Exception as exc:
                print(f"skip mp4 ({exc})", file=sys.stderr)
    finally:
        _close_segment()

    plt.close(fig)


def render_grid_animation_3d(
    traj_paths: dict[str, Path],
    out_gif: Path,
    out_mp4: Path | None,
    *,
    fps: int,
    rotate: bool,
    hold_init_sec: float = 0.0,
    max_frames: int | None = None,
    grid_style: str = "pleasing",
) -> None:
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
    import numpy as np
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    apply_theme()
    style = _grid_style(grid_style)
    meta, n_data, vmax = scan_multi_trajectory_stats(traj_paths, max_frames=max_frames)
    hold_n = _hold_frame_count(fps, hold_init_sec)
    n_anim = hold_n + n_data
    box = float(meta["box"])
    half = _plot_half(box)
    dt = float(meta["dt"])

    fig = plt.figure(figsize=(14, 14))
    fig.patch.set_facecolor(BG)
    axes = np.empty((2, 2), dtype=object)
    for lang, (row, col) in GRID_LAYOUT:
        ax = fig.add_subplot(2, 2, row * 2 + col + 1, projection="3d")
        axes[row, col] = ax
        ax.set_facecolor(PANEL)
        ax.set_box_aspect((1, 1, 1))
        ax.set_title(
            LANG_LABELS.get(lang, lang),
            fontsize=style["title_fontsize"],
            fontweight="bold",
            color=LANG_COLORS.get(lang, TEXT),
        )
        _draw_centered_box_3d(ax, box, style)

    ncell = int(meta.get("ncell", 4))
    rho = meta.get("rho", meta.get("n", 256) / (box**3))
    fig.suptitle(
        "md_lennard_jones · all languages · 3D",
        fontsize=16,
        fontweight="bold",
        color=TEXT,
        x=0.06,
        ha="left",
        y=0.98,
    )
    fig.text(
        0.06,
        0.955,
        f"N={int(meta['n'])} · FCC {ncell}³×4 · ρ={rho:.3f} · shared md_core.c · rotating",
        fontsize=11,
        color=MUTED,
    )
    step_text = fig.text(0.94, 0.955, "", ha="right", fontsize=11, color=MUTED)

    readers = _open_grid_readers(traj_paths)
    f0s: dict[str, dict] = {}
    hold_frs: dict[str, dict] = {}
    data_is: dict[str, list[int]] = {}
    cached: dict[str, list[dict]] = {}
    scatters: dict[str, object] = {}
    try:
        for lang, (row, col) in GRID_LAYOUT:
            ax = axes[row, col]
            f0 = readers[lang].read_frame()
            assert f0 is not None
            f0s[lang] = f0
            hold_frs[lang] = _hold_frame_from(f0) if hold_n > 0 else f0
            data_is[lang] = [0]
            cached[lang] = [f0]
            scatters[lang] = _grid_scatter_3d(ax, f0, half, vmax, lang, style)

        cbar = fig.colorbar(
            scatters["cpp"],
            ax=[axes[r, c] for r in range(2) for c in range(2)],
            fraction=0.02,
            pad=0.02,
            shrink=0.55,
        )
        cbar.set_label("|v| (LJ)", color=TEXT)

        def update(idx: int):
            fr_step = None
            for lang, (row, col) in GRID_LAYOUT:
                fr = _frame_at_anim_index(
                    idx,
                    hold_n,
                    hold_frs[lang],
                    f0s[lang],
                    readers[lang],
                    data_i=data_is[lang],
                    cached=cached[lang],
                )
                sc = scatters[lang]
                x, y, z = _centered_xyz(fr, half)
                sc._offsets3d = (x, y, z)
                sc.set_array(np.array(fr["speed"]))
                if rotate:
                    axes[row, col].view_init(elev=22, azim=35 + idx * 0.55)
                fr_step = fr
            assert fr_step is not None
            step_text.set_text(_step_subtitle(fr_step, meta, dt))
            return (*scatters.values(), step_text)

        anim = animation.FuncAnimation(
            fig,
            update,
            frames=n_anim,
            interval=1000 // fps,
            blit=False,
        )

        out_gif.parent.mkdir(parents=True, exist_ok=True)
        anim.save(out_gif, writer=animation.PillowWriter(fps=fps))
        print(f"wrote {out_gif}")

        if out_mp4 is not None:
            try:
                anim.save(out_mp4, writer=animation.FFMpegWriter(fps=fps))
                print(f"wrote {out_mp4}")
            except Exception as exc:
                print(f"skip mp4 ({exc})", file=sys.stderr)
    finally:
        _close_readers(readers)

    plt.close(fig)


def _export_temperature_trajectories(
    traj_dir: Path,
    *,
    skip_export: bool,
    stride: int,
    max_steps: int,
) -> dict[str, Path]:
    paths: dict[str, Path] = {}
    for key, temp, _label, _pos in TEMP_PANELS:
        traj_path = traj_dir / f"traj_{key}.txt"
        paths[key] = traj_path
        if not skip_export:
            export_trajectory("cpp", traj_path, stride=stride, max_steps=max_steps, temperature=temp)
    return paths


def _export_temp_lang_trajectories(
    traj_dir: Path,
    *,
    skip_export: bool,
    stride: int,
    max_steps: int,
) -> None:
    """Export ``traj_T*.txt`` (native) + ``traj_T*_li.txt`` per temperature panel."""
    for key, temp, _label, _pos in TEMP_PANELS:
        native = traj_dir / f"traj_{key}.txt"
        li_path = traj_dir / f"traj_{key}_li.txt"
        if not skip_export:
            export_trajectory("cpp", native, stride=stride, max_steps=max_steps, temperature=temp)
            export_trajectory("li", li_path, stride=stride, max_steps=max_steps, temperature=temp)


def _positive_fps(value: str) -> int:
    fps = int(value)
    if fps < 1:
        raise argparse.ArgumentTypeError("must be ≥ 1")
    return fps


def _export_trajectories(
    langs: tuple[str, ...],
    traj_dir: Path,
    *,
    skip_export: bool,
    stride: int,
    max_steps: int,
) -> dict[str, Path]:
    paths: dict[str, Path] = {}
    for lang in langs:
        traj_path = traj_dir / f"traj_{lang}.txt"
        paths[lang] = traj_path
        if not skip_export:
            export_trajectory(lang, traj_path, stride=stride, max_steps=max_steps)
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description="MD Lennard-Jones particle animations")
    parser.add_argument("--traj-dir", type=Path, default=TRAJ_DIR)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--lang", choices=(*LANGS, "all"), default="all")
    parser.add_argument(
        "--stride",
        type=int,
        default=16,
        help="trajectory sample every N MD steps (default: 16; ~563 frames with --steps 9000)",
    )
    parser.add_argument(
        "--steps",
        type=int,
        default=9000,
        help="MD steps per trajectory export (default: 9000; pairs with --stride for temp-x)",
    )
    parser.add_argument(
        "--fps",
        type=_positive_fps,
        default=30,
        help="playback frames per second (default: 30; ≥30 recommended for shareables)",
    )
    parser.add_argument(
        "--view",
        choices=(
            "xy",
            "xz",
            "3d",
            "grid",
            "temp-grid",
            "temp-sequence",
            "temps",
            "temp-x",
            "all",
        ),
        default="all",
        help=(
            "2D projection, 3D rotating, 2×2 language grid, 2×2 temperature grid, "
            "temp-x / temp-sequence (2×2 langs per T, sequential), or all views"
        ),
    )
    parser.add_argument(
        "--hold-init",
        type=float,
        default=1.25,
        metavar="SEC",
        help="seconds to hold frame 0 (FCC init) per segment before dynamics (default: 1.25)",
    )
    parser.add_argument(
        "--temp-hold",
        type=float,
        default=20.0,
        metavar="SEC",
        help=(
            "wall seconds per temperature in temp-x view (default: 20; includes hold-init; "
            "4 segments ≈ 80s total)"
        ),
    )
    parser.add_argument("--no-rotate", action="store_true", help="3D: fixed camera")
    parser.add_argument("--skip-export", action="store_true")
    parser.add_argument(
        "--max-frames",
        type=int,
        default=None,
        help="Cap data frames per trajectory (streaming scan + render; dev previews)",
    )
    parser.add_argument(
        "--grid-style",
        choices=tuple(GRID_STYLES),
        default="pleasing",
        help="2×2 grid aesthetics preset (default: pleasing — plasma, FCC guide, padded axes)",
    )
    args = parser.parse_args()

    langs = list(LANGS) if args.lang == "all" else [args.lang]
    view_arg = args.view
    if view_arg == "temps":
        view_arg = "temp-grid"
    want_grid = view_arg in ("grid", "all")
    want_temp_grid = view_arg in ("temp-grid", "all")
    want_temp_lang_sequence = view_arg in ("temp-sequence", "temp-x")
    if view_arg == "grid":
        per_lang_views: list[str] = []
    elif view_arg in ("temp-grid", "temp-sequence", "temp-x"):
        per_lang_views = []
    elif view_arg == "all":
        per_lang_views = ["xy", "3d"]
    else:
        per_lang_views = [view_arg]

    if want_grid and args.lang != "all":
        print("grid view requires --lang all", file=sys.stderr)
        return 1

    args.traj_dir.mkdir(parents=True, exist_ok=True)
    args.out.mkdir(parents=True, exist_ok=True)

    if want_temp_grid:
        temp_paths = _export_temperature_trajectories(
            args.traj_dir,
            skip_export=args.skip_export,
            stride=args.stride,
            max_steps=args.steps,
        )
        try:
            render_temp_grid_animation(
                temp_paths,
                args.out / "md_lennard_jones_temperature_grid.gif",
                args.out / "md_lennard_jones_temperature_grid.mp4",
                fps=args.fps,
                hold_init_sec=args.hold_init,
                max_frames=args.max_frames,
                grid_style=args.grid_style,
            )
        except ValueError as exc:
            print(exc, file=sys.stderr)
            return 1
        if view_arg == "temp-grid":
            print(f"wrote animations to {args.out}")
            return 0

    if want_temp_lang_sequence:
        _export_temp_lang_trajectories(
            args.traj_dir,
            skip_export=args.skip_export,
            stride=args.stride,
            max_steps=args.steps,
        )
        try:
            render_temp_lang_sequence_animation(
                args.traj_dir,
                args.out / "md_lennard_jones_temperature_x.gif",
                args.out / "md_lennard_jones_temperature_x.mp4",
                fps=args.fps,
                temp_hold=args.temp_hold,
                hold_init_sec=args.hold_init,
                max_frames=args.max_frames,
                grid_style=args.grid_style,
            )
        except ValueError as exc:
            print(exc, file=sys.stderr)
            return 1
        if view_arg in ("temp-sequence", "temp-x"):
            print(f"wrote animations to {args.out}")
            return 0

    export_langs = list(dict.fromkeys([*langs, *(LANGS if want_grid else ())]))
    traj_paths = _export_trajectories(
        tuple(export_langs),
        args.traj_dir,
        skip_export=args.skip_export,
        stride=args.stride,
        max_steps=args.steps,
    )

    if want_grid:
        grid_paths = {lang: traj_paths[lang] for lang in LANGS}
        try:
            render_grid_animation(
                grid_paths,
                args.out / "md_lennard_jones_grid_particles.gif",
                args.out / "md_lennard_jones_grid_particles.mp4",
                fps=args.fps,
                hold_init_sec=args.hold_init,
                max_frames=args.max_frames,
                grid_style=args.grid_style,
            )
            if view_arg == "all":
                render_grid_animation_3d(
                    grid_paths,
                    args.out / "md_lennard_jones_grid_particles_3d.gif",
                    args.out / "md_lennard_jones_grid_particles_3d.mp4",
                    fps=args.fps,
                    rotate=not args.no_rotate,
                    hold_init_sec=args.hold_init,
                    max_frames=args.max_frames,
                    grid_style=args.grid_style,
                )
        except ValueError as exc:
            print(exc, file=sys.stderr)
            return 1
        if view_arg == "grid":
            print(f"wrote animations to {args.out}")
            return 0

    for lang in langs:
        traj_path = traj_paths[lang]
        for view in per_lang_views:
            try:
                if view == "3d":
                    render_animation_3d(
                        traj_path,
                        args.out / f"md_lennard_jones_{lang}_particles_3d.gif",
                        args.out / f"md_lennard_jones_{lang}_particles_3d.mp4",
                        fps=args.fps,
                        rotate=not args.no_rotate,
                        lang=lang,
                        hold_init_sec=args.hold_init,
                        max_frames=args.max_frames,
                    )
                else:
                    suffix = f"_{view}" if view != "xy" else ""
                    render_animation(
                        traj_path,
                        args.out / f"md_lennard_jones_{lang}_particles{suffix}.gif",
                        args.out / f"md_lennard_jones_{lang}_particles{suffix}.mp4",
                        fps=args.fps,
                        plane=view,
                        lang=lang,
                        hold_init_sec=args.hold_init,
                        max_frames=args.max_frames,
                    )
            except ValueError as exc:
                print(exc, file=sys.stderr)
                return 1

    native = [args.traj_dir / f"traj_{l}.txt" for l in ("cpp", "rust", "julia")]
    if all(p.exists() for p in native):
        b0 = native[0].read_bytes()
        if all(p.read_bytes() == b0 for p in native[1:]):
            print("verify: cpp/rust/julia trajectories are byte-identical (native md_core.c)")
    cpp_traj = args.traj_dir / "traj_cpp.txt"
    li_traj = args.traj_dir / "traj_li.txt"
    if cpp_traj.exists() and li_traj.exists():
        try:
            _, f_cpp = read_first_frame(cpp_traj)
            _, f_li = read_first_frame(li_traj)
        except ValueError:
            f_cpp = f_li = None
        if f_cpp and f_li and f_cpp["step"] == f_li["step"]:
            max_d = 0.0
            for k in ("px", "py", "pz"):
                for a, b in zip(f_cpp[k], f_li[k]):
                    max_d = max(max_d, abs(a - b))
            if max_d < 1e-9:
                print("verify: Li trajectory matches native at t=0 (shared kernel)")
            else:
                print(f"warn: Li vs native position drift at t=0: {max_d}", file=sys.stderr)

    print(f"wrote animations to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
