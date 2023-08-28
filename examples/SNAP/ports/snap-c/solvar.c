/***********************************************************************
 * Module: solvar.c
 *
 * This module contains several variables that are used in the solution
 * process, including their allocation and deallocation. Also includes
 * initialization of sweep parameters.
 ***********************************************************************/
#include "snap.h"

void solvar_data_init ( solvar_data *solvar_vars )
{
    FLUX    = NULL;
    FLUXPO  = NULL;
    FLUXPI  = NULL;
    T_XS    = NULL;
    A_XS    = NULL;
    PSII    = NULL;
    PSIJ    = NULL;
    PSIK    = NULL;
    JB_IN   = NULL;
    JB_OUT  = NULL;
    KB_IN   = NULL;
    KB_OUT  = NULL;
    FLKX    = NULL;
    FLKY    = NULL;
    FLKZ    = NULL;
    QTOT    = NULL;
    Q2GRP   = NULL;
    FLUXM   = NULL;
    S_XS    = NULL;
    PTR_IN  = NULL;
    PTR_OUT = NULL;
}

/***********************************************************************
 * Allocate solution arrays.
 ***********************************************************************/
void solvar_alloc ( input_data *input_vars, sn_data* sn_vars,
                    solvar_data *solvar_vars, int *ierr, sicm_device_list *devs )
{
/***********************************************************************
 * Allocate ptr_in/out if needed. Provide an initial condition of zero
 * This may be changed in the future if necessary.
 ***********************************************************************/
sicm_device *src = devs->devices[0];
    if ( TIMEDEP == 1 )
    {
        ALLOC_SICM_6D(src, PTR_IN,  NANG, NX, NY, NZ, NOCT, NG, double, ierr);
        ALLOC_SICM_6D(src, PTR_OUT, NANG, NX, NY, NZ, NOCT, NG, double, ierr);
    }

/***********************************************************************
 * Allocate the flux moments arrays. Keep an old copy.
 ***********************************************************************/
    ALLOC_SICM_4D(src, FLUX,   NX,      NY, NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, FLUXPO, NX,      NY, NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, FLUXPI, NX,      NY, NZ, NG, double, ierr);

    if ( (CMOM-1) != 0 )
    {
        ALLOC_SICM_5D(src, FLUXM, (CMOM-1), NX, NY, NZ, NG, double, ierr);
    }

/***********************************************************************
 * Allocate the source arrays.
 ***********************************************************************/
    ALLOC_SICM_5D(src, Q2GRP, CMOM, NX, NY, NZ, NG, double, ierr);
    ALLOC_SICM_5D(src, QTOT,  CMOM, NX, NY, NZ, NG, double, ierr);

/***********************************************************************
 * Allocate the cross section expanded to spatial mesh arrays
 ***********************************************************************/
    ALLOC_SICM_4D(src, T_XS, NX,   NY, NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, A_XS, NX,   NY, NZ, NG, double, ierr);
    ALLOC_SICM_5D(src, S_XS, NMOM, NX, NY, NZ, NG, double, ierr);

/***********************************************************************
 * Working arrays
 ***********************************************************************/
    ALLOC_SICM_4D(src, PSII, NANG, NY,     NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, PSIJ, NANG, ICHUNK, NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, PSIK, NANG, ICHUNK, NY, NG, double, ierr);

/***********************************************************************
 * PE boundary flux arrays
 ***********************************************************************/
    ALLOC_SICM_4D(src, JB_IN,  NANG, ICHUNK, NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, JB_OUT, NANG, ICHUNK, NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, KB_IN,  NANG, ICHUNK, NY, NG, double, ierr);
    ALLOC_SICM_4D(src, KB_OUT, NANG, ICHUNK, NY, NG, double, ierr);

/***********************************************************************
 * Leakage arrays
 ***********************************************************************/
    ALLOC_SICM_4D(src, FLKX, (NX+1),     NY,     NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, FLKY,     NX, (NY+1),     NZ, NG, double, ierr);
    ALLOC_SICM_4D(src, FLKZ,     NX,     NY, (NZ+1), NG, double, ierr);
}

void solvar_dealloc ( solvar_data *solvar_vars, input_data *input_vars, sn_data* sn_vars, sicm_device_list *devs )
{
    sicm_device *location = devs->devices[0];
    DEALLOC_SICM(location, PTR_IN, NANG*NX*NY*NZ*NOCT*NG, double);
    DEALLOC_SICM(location, PTR_OUT, NANG*NX*NY*NZ*NOCT*NG, double);
    DEALLOC_SICM(location, FLUX, NX*NY*NZ*NG, double);
    DEALLOC_SICM(location, FLUXPO, NX*NY*NZ*NG, double);
    DEALLOC_SICM(location, FLUXPI, NX*NY*NZ*NG, double);
    DEALLOC_SICM(location, T_XS, NX*NY*NZ*NG, double);
    DEALLOC_SICM(location, A_XS, NX*NY*NZ*NG, double);
    DEALLOC_SICM(location, PSII, NANG*NY*NZ*NG, double);
    DEALLOC_SICM(location, PSIJ, NANG*ICHUNK*NZ*NG, double);
    DEALLOC_SICM(location, PSIK, NANG*ICHUNK*NY*NG, double);
    DEALLOC_SICM(location, JB_IN, NANG*ICHUNK*NZ*NG, double);
    DEALLOC_SICM(location, JB_OUT, NANG*ICHUNK*NZ*NG, double);
    DEALLOC_SICM(location, KB_IN, NANG*ICHUNK*NY*NG, double);
    DEALLOC_SICM(location, KB_OUT, NANG*ICHUNK*NY*NG, double);
    DEALLOC_SICM(location, FLKX, (NX+1)*NY*NZ*NG, double);
    DEALLOC_SICM(location, FLKY, NX*(NY+1)*NZ*NG, double);
    DEALLOC_SICM(location, FLKZ, NX*NY*(NZ+1)*NG, double);   
    DEALLOC_SICM(location, S_XS, NMOM*NX*NY*NZ* NG, double);
    DEALLOC_SICM(location, Q2GRP, CMOM*NX*NY*NZ* NG, double);
    DEALLOC_SICM(location, QTOT, CMOM*NX*NY*NZ* NG, double);
    DEALLOC_SICM(location, FLUXM, (CMOM-1)*NX*NY*NZ* NG, double);
}
