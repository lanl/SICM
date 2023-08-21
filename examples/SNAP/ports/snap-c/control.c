/***********************************************************************
 * Module: control.c
 * This module contains the variables that control SNAP's solver
 * routines. This includes the time-dependent variables.
 ***********************************************************************/
#include "snap.h"

void control_data_init ( control_data *control_vars )
{
    DT      = 0;
    TOLR    = 1.02E-12;
    DFMXI   = NULL;
    DFMXO   = -1;
    INRDONE = NULL;
    OTRDONE = false;
}

/***********************************************************************
 * Allocate control module variables.
 ***********************************************************************/
void control_alloc ( input_data *input_vars, control_data *control_vars, int *ierr ,sicm_device_list *devs )
{
    int i;
    sicm_device *src = devs->devices[0];
    ALLOC_SICM_1D(src, DFMXI,   NG, double, ierr);
    ALLOC_SICM_1D(src, INRDONE, NG, bool,   ierr);

    if ( *ierr != 0 ) return;

    for ( i = 0; i < NG; i++ )
    {
        INRDONE_1D(i) = false;
        DFMXI_1D(i) = -1;
    }

    DFMXO = -1;
}

/***********************************************************************
 * Deallocate control module variables.
 ***********************************************************************/
void control_dealloc ( control_data *control_vars, sicm_device_list *devs, input_data *input_vars )
{
    sicm_device *location = devs->devices[0];
    DEALLOC_SICM(location, DFMXI,NG,double);
    DEALLOC_SICM(location, INRDONE,NG,bool);
}
