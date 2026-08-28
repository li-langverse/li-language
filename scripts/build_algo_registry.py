#!/usr/bin/env python3
"""Regenerate benchmarks/competitive/algo_registry.json (126 algo_ids from sim plan)."""

from __future__ import annotations

import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "benchmarks" / "competitive" / "algo_registry.json"


def add(rows: list, start: int, items: list[str], family: str, impl: bool = False) -> None:
    for i, name in enumerate(items):
        rows.append(
            {
                "id": start + i,
                "name": name,
                "family": family,
                "implemented_smoke": impl,
            }
        )


def main() -> None:
    rows: list[dict] = []
    add(
        rows,
        1,
        [
            "num_dot_axpy",
            "num_matmul_naive",
            "num_matmul_blocked",
            "num_sparse_mv",
            "num_cg",
            "num_gmres",
            "num_cholesky",
            "num_eig_symmetric",
            "num_fft_r2c",
            "num_rng_pcg",
            "num_integ_euler",
            "num_integ_rk4",
            "num_integ_verlet",
            "num_integ_symplectic",
            "num_integ_semi_implicit",
            "num_root_newton",
            "num_opt_bfgs",
            "num_opt_line_search",
            "num_quadrature_gauss",
        ],
        "num",
    )
    add(
        rows,
        101,
        [
            "md_lj_cutoff_mic",
            "md_integrator_verlet",
            "md_energy_drift",
            "md_oracle_external",
            "md_neighbor_cell_list",
            "md_neighbor_verlet_skin",
            "md_integrator_leapfrog",
            "md_thermostat_nose_hoover",
            "md_thermostat_berendsen",
            "md_barostat_parrinello_rahman",
            "md_constraints_shake",
            "md_constraints_rattle",
            "md_longrange_ewald",
            "md_longrange_pme",
            "md_init_fcc_mb",
            "nbody_pairwise_gravity",
            "nbody_barnes_hut",
        ],
        "md",
        impl=True,
    )
    add(
        rows,
        201,
        [
            "pde_heat_explicit_2d",
            "pde_cfl_timestep",
            "pde_wave_1d_cfl",
            "pde_heat_implicit_jacobi",
            "cfd_fvm_face_flux",
            "cfd_simple",
            "cfd_piso",
            "cfd_pimple",
            "cfd_turbulence_k_epsilon",
            "cfd_turbulence_k_omega_sst",
            "fea_mesh_tri_tet",
            "fea_stiffness_assembly",
            "fea_gauss_quadrature",
            "fea_linear_elasticity",
            "fea_solver_direct",
            "fea_solver_iterative",
        ],
        "pde",
        impl=True,
    )
    add(
        rows,
        301,
        [
            "rigid_semi_implicit",
            "rigid_broadphase_sap",
            "rigid_broadphase_bvh",
            "rigid_contact_solver",
            "rigid_constraints",
        ],
        "rigid",
        impl=True,
    )
    add(
        rows,
        401,
        [
            "qm_gto_eval",
            "qm_overlap_integrals",
            "qm_kinetic_integrals",
            "qm_nuclear_attraction",
            "qm_eri_os",
            "qm_eri_screening",
            "qm_eri_density_fitting",
            "qm_hf_fock_build",
            "qm_hf_diis",
            "qm_hf_canonical_ortho",
            "qm_scf_solver",
            "qm_dft_xc_lda",
            "qm_dft_xc_gga",
            "qm_dft_xc_mgga",
            "qm_dft_grid_becke",
            "qm_dft_grid_lebedev",
            "qm_dft_hybrid_exchange",
            "qm_dft_scf_energy",
            "qm_grad_analytical",
            "qm_geom_opt_internal",
            "qm_geom_opt_bfgs",
            "qm_mp2",
            "qm_ccsd",
            "qm_tddft_casida",
            "qm_tddft_rpa",
            "qm_xtb_gfn",
            "qm_dispersion_d3",
            "qm_ecp",
            "qm_ase_calculator",
            "qm_property_dipole",
            "qm_property_freq",
            "qm_job_queue_io",
        ],
        "qm",
    )
    add(
        rows,
        501,
        [
            "drug_litl_stages",
            "drug_docking_score_vina",
            "drug_docking_diffusion",
            "drug_ml_retrain_loop",
            "drug_fep_alchemical",
        ],
        "drug",
    )
    add(
        rows,
        511,
        [
            "bio_rosetta_energy",
            "bio_rotamer_packing",
            "bio_proteinmpnn",
            "bio_rfdiffusion",
        ],
        "bio",
    )
    add(
        rows,
        521,
        [
            "ml_mlp_forward",
            "ml_mlp_train_step",
            "ml_conv2d_forward",
        ],
        "ml",
    )
    add(
        rows,
        601,
        [
            "am_plane_mesh_intersect",
            "am_polygon_clip",
            "am_slice_layers",
            "am_offset_perimeters",
            "am_infill_grid_lines",
            "am_infill_gyroid",
            "am_support_tree",
            "am_toolpath_arcs",
            "am_thermal_warp",
            "am_export_gcode_3mf",
        ],
        "am",
    )
    add(
        rows,
        701,
        [
            "viz_colormap",
            "viz_marching_cubes",
            "viz_resample",
            "viz_decimate",
            "viz_pipeline_graph",
            "viz_inspector_panels",
            "viz_linked_views",
        ],
        "viz",
    )
    add(
        rows,
        801,
        [
            "robo_multibody_step",
            "robo_ik_jacobian",
            "robo_plan_rrt",
            "robo_plan_prm",
            "robo_traj_opt",
        ],
        "robo",
        impl=True,
    )
    add(
        rows,
        901,
        [
            "auto_bicycle_model",
            "auto_dyn_rk4",
            "auto_sensor_raycast",
        ],
        "auto",
    )

    doc = {"schema": "li_algo_registry_v1", "count": len(rows), "algorithms": rows}
    OUT.write_text(json.dumps(doc, indent=2) + "\n")
    print(f"wrote {OUT.relative_to(REPO)} count={len(rows)}")


if __name__ == "__main__":
    main()
