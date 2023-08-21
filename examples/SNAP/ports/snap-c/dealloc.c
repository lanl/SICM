/***********************************************************************
 * Controls the deallocation process of run-time arrays.
 ***********************************************************************/
#include "snap.h"

/***********************************************************************
 * Call for the data deallocation from individual deallocation
 * subroutines. Covers the allocations from input.
 ***********************************************************************/
void dealloc_input ( int selectFlag, sn_data *sn_vars, data_data *data_vars,
                     mms_data *mms_vars, input_data *input_vars, sicm_device_list *devs )
{
    sn_deallocate ( sn_vars, input_vars, devs);
    if ( selectFlag > 1 )
         data_deallocate ( data_vars, input_vars, sn_vars, devs );
    if ( selectFlag > 2 )
        mms_deallocate ( mms_vars, input_vars, devs );
}

/***********************************************************************
 * Call for the data deallocation from individual deallocation
 * subroutines. Covers the allocations from input.
 ***********************************************************************/
void dealloc_solve ( int selectFlag, geom_data *geom_vars,
                     solvar_data *solvar_vars, control_data *control_vars, sicm_device_list *devs, input_data *input_vars, sn_data* sn_vars )
{
    geom_dealloc ( geom_vars, input_vars, devs );
    if ( selectFlag > 1 )
        solvar_dealloc ( solvar_vars,input_vars, sn_vars, devs );
    if ( selectFlag > 2 )
        control_dealloc ( control_vars, devs, input_vars );
}
