
/*******************************************************************************
 * Module: data.c
 * This module contains the variables and setup subroutines for the mock
 * cross section data. It establishes the number of groups and constructs
 * the cross section arrays.
 *******************************************************************************/
#include "snap.h"

/*******************************************************************************
 * Constructor for data_data type
 *******************************************************************************/
void data_data_init (data_data *data_vars )
{
    V     = NULL;
    NMAT  = 1;
    MAT   = NULL;
    QI    = NULL;
    QIM   = NULL;
    SIGT  = NULL;
    SIGA  = NULL;
    SIGS  = NULL;
    SLGG  = NULL;
    VDELT = NULL;
}

/*******************************************************************************
 * Allocate data_module arrays.
 *******************************************************************************/
void data_allocate ( data_data *data_vars, input_data *input_vars,
                     sn_data *sn_vars, int *ierr, sicm_device_list *devs )
{
/*******************************************************************************
 * Local variables
 *******************************************************************************/
    int i,j,k;

/*******************************************************************************
 * Establish number of materials according to mat_opt
 *******************************************************************************/
    if ( MAT_OPT > 0 )
    {
        NMAT = 2;
    }
     sicm_device *src = devs->devices[0];
/*******************************************************************************
 * Allocate velocities
 *******************************************************************************/
    if ( TIMEDEP == 1 )
    {
        ALLOC_SICM_1D(src,V, NG, double, ierr);
    }

    if ( *ierr != 0 ) return;

/*******************************************************************************
 * Allocate the material identifier array. ny and nz are 1 if not
 * 2-D/3-D.
 *******************************************************************************/
    ALLOC_SICM_3D(src, MAT, NX, NY, NZ, int, ierr);

    if ( *ierr != 0 ) return;

    for ( k=0; k < NZ; k++ )
    {
        for ( j=0; j < NY; j++ )
        {
            for ( i=0; i < NX; i++ )
            {
                MAT_3D(i, j, k) = 1;
            }
        }
    }

/*******************************************************************************
 * Allocate the fixed source array. If src_opt < 3, allocate the qi
 * array, not the qim. Do the opposite (store the full angular copy) of
 * the source, qim, if src_opt>=3 (MMS). Allocate array not used to 0.
 * ny and nz are 1 if not 2-D/3-D.
 *******************************************************************************/
    if ( SRC_OPT < 3 )
    {
        ALLOC_4D(QI, NX, NY, NZ, NG, double, ierr);

        if ( *ierr != 0 ) return;
    }
    else
    {
        ALLOC_4D(QI, NX, NY, NZ, NG, double, ierr);
        ALLOC_SICM_6D(src, QIM, NANG, NX, NY, NZ, NOCT, NG, double, ierr);

        if ( *ierr != 0 ) return;
    }


/*******************************************************************************
 * Allocate mock cross sections
 *******************************************************************************/
    if (NMAT != 0 )
    {
        ALLOC_SICM_2D(src, SIGT, NMAT, NG, double, ierr);
        ALLOC_SICM_2D(src, SIGA, NMAT, NG, double, ierr);
        ALLOC_SICM_2D(src, SIGS, NMAT, NG, double, ierr);
        ALLOC_4D(SLGG, NMAT, NMOM, NG, NG, double, ierr);

        if ( *ierr != 0 ) return;
    }
    else
    {
        ALLOC_SICM_1D(src, SIGT, NG, double, ierr);
        ALLOC_SICM_1D(src, SIGA, NG, double, ierr);
        ALLOC_SICM_1D(src, SIGS, NG, double, ierr);
        ALLOC_SICM_3D(src, SLGG, NMOM, NG, NG, double, ierr);

        if ( *ierr != 0 ) return;
    }


/*******************************************************************************
 * Allocate the vdelt array
 *******************************************************************************/
    ALLOC_SICM_1D(src, VDELT, NG, double, ierr);

    if ( *ierr != 0 ) return;
}

/*******************************************************************************
 * Deallocate the data module arrays
 *******************************************************************************/
void data_deallocate ( data_data *data_vars, input_data *input_vars, sn_data *sn_vars, sicm_device_list *devs )
{
    sicm_device *location = devs->devices[0];
    DEALLOC_SICM(location, V,NG,double);
   if (NMAT != 0)
{
    DEALLOC_SICM(location, SIGT,NMAT*NG,double);
    DEALLOC_SICM(location, SIGA,NMAT*NG,double);
    DEALLOC_SICM(location, SIGS,NMAT*NG,double);
 }
 else {
    DEALLOC_SICM(location, SIGT,NG,double);
    DEALLOC_SICM(location, SIGA,NG,double);
    DEALLOC_SICM(location, SIGS,NG,double);
  } 
   DEALLOC_SICM(location,MAT,NX*NY*NZ,int);
   DEALLOC_SICM(location, SLGG,NMOM*NG*NG, double);
    FREE(QI);
    DEALLOC_SICM(location,QIM, NANG*NX*NY*NZ*NOCT*NG, double );
    DEALLOC_SICM(location, VDELT,NG,double);
}

