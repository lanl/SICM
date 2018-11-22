#ifndef _Castro_error_F_H_
#define _Castro_error_F_H_
#include <AMReX_BLFort.H>

#ifdef __cplusplus
extern "C"
{
#endif

  void ca_laplac_error
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(var),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_denerror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(den),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_velerror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(vel),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_vorterror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(vel),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_temperror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(temp),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_presserror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(press),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_nucerror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(state),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_mfracerror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(press),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_enterror
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(den),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

  void ca_state_error
    (int* tag, const int* tag_lo, const int* tag_hi,
     const int* tagval, const int* clearval,
     BL_FORT_FAB_ARG_3D(state),
     const int* lo, const int* hi,
     const int* ncomp,
     const int* domlo, const int* domhi,
     const amrex::Real* dx, const amrex::Real* xlo, const amrex::Real* problo,
     const amrex::Real* time, const int* level);

#ifdef __cplusplus
}
#endif

#endif
