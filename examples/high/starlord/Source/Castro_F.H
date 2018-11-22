#ifndef _Castro_F_H_
#define _Castro_F_H_
#include <AMReX_BLFort.H>

#ifdef __cplusplus
#include <AMReX.H>
extern "C"
{
#endif

  void ca_network_init();

  void eos_finalize();

  void network_finalize();

  void ca_extern_init(const int* name, const int* namlen);

  void ca_extern_finalize();

  void probinit_finalize();

  void ca_get_num_spec(int* nspec);

  void ca_get_num_aux(int* naux);

  void ca_get_spec_names(int* spec_names, int* ispec, int* len);

  void ca_get_spec_az(int* ispec, amrex::Real* A, amrex::Real* Z);

  void ca_get_aux_names(int* aux_names, int* iaux, int* len);

  void ca_get_qvar(int* qvar);

  void ca_get_nqaux(int* nqaux);

  void ca_get_ngdnv(int* ngdnv);

  void ca_set_special_tagging_flag(const amrex::Real& max_den, const int* flag);

  void ca_get_method_params(int* HYP_GROW);

  void ca_set_method_params
    (const int& dm,
     const int& Density, const int& Xmom, 
     const int& Eden,    const int& Eint, 
     const int& Temp     , const int& FirstAdv, 
     const int& FirstSpec, const int& FirstAux, 
     const int& NumAdv);

  void ca_destroy_method_params();

  void ca_set_castro_method_params();

  void ca_destroy_castro_method_params();

  void ca_init_godunov_indices();

  void ca_set_problem_params
    (const int& dm,
     const int* physbc_lo, const int* physbc_hi,
     const int& Interior_value, const int& Inflow_value,
     const int& Outflow_value, const int& Symmetry_value,
     const int& SlipWall_value, const int& NoSlipWall_value,
     const int& coord_type,
     const amrex::Real* problo, const amrex::Real* probhi,
     const amrex::Real* center);

  void ca_destroy_problem_params();

  void ca_set_grid_info
    (const int& max_level, const amrex::Real* dx_level,
     const int* domlo_level, const int* domhi_level,
     const int* ref_ratio, const int* n_error_buf,
     const int* blocking_factor);

  void ca_destroy_grid_info();

  void ca_get_tagging_params(const int* name, const int* namlen);

  void ca_ctoprim(const int* lo, const int* hi,
                  const amrex::Real* u, const int* u_lo, const int* u_hi,
                  const amrex::Real* q, const int* q_lo, const int* q_hi,
                  const amrex::Real* qaux, const int* qa_lo, const int* qa_hi);

  void ca_check_initial_species
    (const int* lo, const int* hi, BL_FORT_FAB_ARG_3D(state));

  void ca_enforce_minimum_density
    (const amrex::Real* S_old, const int* s_old_lo, const int* s_old_hi,
     const amrex::Real* S_new, const int* s_new_lo, const int* s_new_hi,
     const amrex::Real* vol, const int* vol_lo, const int* vol_hi,
     const int* lo, const int* hi,
     const amrex::Real* frac_change, const int verbose);

  void ca_normalize_species
    (BL_FORT_FAB_ARG_3D(S_new), const int* lo, const int* hi);

  void ca_compute_temp
    (const int* lo, const int* hi, const BL_FORT_FAB_ARG_3D(state));

  void ca_enforce_consistent_e
    (const int* lo, const int* hi, BL_FORT_FAB_ARG_3D(state));

  void ca_reset_internal_e
    (const int* lo, const int* hi, BL_FORT_FAB_ARG_3D(S_new),
     const int verbose);

  void ca_hypfill
    (BL_FORT_FAB_ARG(state),
     const int dlo[], const int dhi[],
     const amrex::Real dx[], const amrex::Real glo[],
     const amrex::Real* time, const int bc[]);

  void ca_denfill
    (BL_FORT_FAB_ARG(state),
     const int dlo[], const int dhi[],
     const amrex::Real dx[], const amrex::Real glo[],
     const amrex::Real* time, const int bc[]);

  void ca_xmomfill
    (BL_FORT_FAB_ARG(state),
     const int dlo[], const int dhi[],
     const amrex::Real dx[], const amrex::Real glo[],
     const amrex::Real* time, const int bc[]);

  void ca_ymomfill
    (BL_FORT_FAB_ARG(state),
     const int dlo[], const int dhi[],
     const amrex::Real dx[], const amrex::Real glo[],
     const amrex::Real* time, const int bc[]);

  void ca_zmomfill
    (BL_FORT_FAB_ARG(state),
     const int dlo[], const int dhi[],
     const amrex::Real dx[], const amrex::Real glo[],
     const amrex::Real* time, const int bc[]);

  void ca_estdt
    (const int* lo, const int* hi,
     const BL_FORT_FAB_ARG_3D(state),
     const amrex::Real* dx, amrex::Real* dt);

  void ca_divu
    (const int* lo, const int* hi,
     const amrex::Real* dx,
     BL_FORT_FAB_ARG_3D(q),
     BL_FORT_FAB_ARG_3D(div));

  void ca_uflaten
    (const int* lo, const int* hi,
     BL_FORT_FAB_ARG_3D(q),
     BL_FORT_FAB_ARG_3D(flatn));

  void ca_ppm_reconstruct
    (const int* lo, const int* hi,
     BL_FORT_FAB_ARG_3D(q),
     BL_FORT_FAB_ARG_3D(flatn),
     BL_FORT_FAB_ARG_3D(qm),
     BL_FORT_FAB_ARG_3D(qp));

  void ca_construct_flux
    (const int* lo, const int* hi,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real dt,
     const int idir,
     const BL_FORT_FAB_ARG_3D(statein),
     BL_FORT_FAB_ARG_3D(div),
     BL_FORT_FAB_ARG_3D(qaux),
     BL_FORT_FAB_ARG_3D(qm),
     BL_FORT_FAB_ARG_3D(qp),
     BL_FORT_FAB_ARG_3D(qe),
     BL_FORT_FAB_ARG_3D(flux),
     BL_FORT_FAB_ARG_3D(area));

  void ca_construct_hydro_update
    (const int* lo, const int* hi,
     const amrex::Real* dx, const amrex::Real dt,
     const amrex::Real stage_weight,
     BL_FORT_FAB_ARG_3D(qx),
     BL_FORT_FAB_ARG_3D(qy),
     BL_FORT_FAB_ARG_3D(qz),
     BL_FORT_FAB_ARG_3D(xflux),
     BL_FORT_FAB_ARG_3D(yflux),
     BL_FORT_FAB_ARG_3D(zflux),
     BL_FORT_FAB_ARG_3D(xarea),
     BL_FORT_FAB_ARG_3D(yarea),
     BL_FORT_FAB_ARG_3D(zarea),
     const BL_FORT_FAB_ARG_3D(volume),
     BL_FORT_FAB_ARG_3D(update));

  void flush_output();

  void ca_summass
    (const int* lo, const int* hi, const BL_FORT_FAB_ARG_3D(rho),
     const amrex::Real* dx, BL_FORT_FAB_ARG_3D(vol), amrex::Real* mass);

  void ca_initdata
    (const int level, const int* lo, const int* hi,
     BL_FORT_FAB_ARG_3D(state),
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* xhi);

#ifdef __cplusplus
}
#endif



#endif
