
! This file is automatically created by parse_castro_params.py.  To update
! or add runtime parameters, please edit _cpp_parameters and then run
! mk_params.sh

! This module stores the runtime parameters and integer names for 
! indexing arrays.
!
! The Fortran-specific parameters are initialized in set_method_params(),
! and the ones that we are mirroring from C++ and obtaining through the
! ParmParse module are initialized in ca_set_castro_method_params().

module meth_params_module

  use amrex_error_module, only: amrex_error
  use amrex_fort_module, only: rt => amrex_real
  use actual_network, only: nspec, naux

  implicit none

  ! number of ghost cells for the hyperbolic solver
  integer, parameter :: NHYP = 4

  !---------------------------------------------------------------------
  ! conserved state components
  !---------------------------------------------------------------------

  ! NTHERM: number of thermodynamic variables (rho, 3 momenta, rho*e, rho*E, T)
  integer, parameter :: NTHERM = 7

  ! NVAR  : number of total variables in initial system  
  integer, parameter :: NVAR = NTHERM + nspec + naux

  ! nadv: number of passively advected variables
  integer, parameter :: nadv = 0

  ! We use these to index into the state "U"
  integer, parameter :: URHO = 1
  integer, parameter :: UMX = 2
  integer, parameter :: UMY = 3
  integer, parameter :: UMZ = 4
  integer, parameter :: UEDEN = 5
  integer, parameter :: UEINT = 6
  integer, parameter :: UTEMP = 7 ! == NTHERM
  integer, parameter :: UFA = 1
  integer, parameter :: UFS = NTHERM + 1
  integer, parameter :: UFX = 1

  !---------------------------------------------------------------------
  ! primitive state components
  !---------------------------------------------------------------------

  ! QTHERM: number of primitive variables: rho, p, (rho e), T + 3 velocity components 
  integer, parameter :: QTHERM = NTHERM + 1 ! the + 1 is for QGAME which is always defined in primitive mode

  ! QVAR  : number of total variables in primitive form
  integer, parameter :: QVAR = QTHERM + nspec + naux

  ! We use these to index into the state "Q"
  integer, parameter :: QRHO = 1
  integer, parameter :: QU = 2
  integer, parameter :: QV = 3
  integer, parameter :: QW = 4
  integer, parameter :: QGAME = 5
  integer, parameter :: QPRES = 6
  integer, parameter :: QREINT = 7
  integer, parameter :: QTEMP = 8 ! == QTHERM
  integer, parameter :: QFA = 1
  integer, parameter :: QFS = QTHERM + 1
  integer, parameter :: QFX = 1

  ! The NQAUX here are auxiliary quantities (game, gamc, c, csml, dpdr, dpde)
  ! that we create in the primitive variable call but that do not need to
  ! participate in tracing.
  integer, parameter :: NQAUX = 5
  integer, parameter :: QGAMC = 1
  integer, parameter :: QC    = 2
  integer, parameter :: QDPDR = 3
  integer, parameter :: QDPDE = 4

  ! NQ will be the total number of primitive variables, hydro + radiation
  integer, parameter :: NQ = QVAR

  integer, allocatable :: npassive
  integer, allocatable :: qpass_map(:), upass_map(:)

  ! These are used for the Godunov state
  ! Note that the velocity indices here are picked to be the same value
  ! as in the primitive variable array
  integer, parameter :: NGDNV = 6
  integer, parameter :: GDRHO = 1
  integer, parameter :: GDU = 2
  integer, parameter :: GDV = 3
  integer, parameter :: GDW = 4
  integer, parameter :: GDPRES = 5
  integer, parameter :: GDGAME = 6

  integer, save :: numpts_1d

  real(rt), save, allocatable :: outflow_data_old(:,:)
  real(rt), save, allocatable :: outflow_data_new(:,:)
  real(rt), save :: outflow_data_old_time
  real(rt), save :: outflow_data_new_time
  logical,  save :: outflow_data_allocated
  real(rt), save :: max_dist

#ifdef AMREX_USE_CUDA
  attributes(managed) :: upass_map, qpass_map, npassive
#endif

  ! Begin the declarations of the ParmParse parameters

  real(rt), allocatable :: small_dens
  real(rt), allocatable :: small_temp
  real(rt), allocatable :: cfl

#ifdef AMREX_USE_CUDA
  attributes(managed) :: small_dens
  attributes(managed) :: small_temp
  attributes(managed) :: cfl
#endif

  !$acc declare &
  !$acc create(small_dens, small_temp, cfl)

  ! End the declarations of the ParmParse parameters

contains

  subroutine ca_set_castro_method_params() bind(C, name="ca_set_castro_method_params")

    use amrex_parmparse_module, only: amrex_parmparse_build, amrex_parmparse_destroy, amrex_parmparse
    use amrex_fort_module, only: rt => amrex_real

    implicit none

    type (amrex_parmparse) :: pp

    call amrex_parmparse_build(pp, "castro")

    allocate(small_dens)
    small_dens = -1.d200
    allocate(small_temp)
    small_temp = -1.d200
    allocate(cfl)
    cfl = 0.8d0

    call pp%query("small_dens", small_dens)
    call pp%query("small_temp", small_temp)
    call pp%query("cfl", cfl)

    !$acc update &
    !$acc device(small_dens, small_temp, cfl)

    call amrex_parmparse_destroy(pp)

  end subroutine ca_set_castro_method_params



  subroutine ca_destroy_castro_method_params() bind(C, name="ca_destroy_castro_method_params")

    implicit none

    deallocate(small_dens)
    deallocate(small_temp)
    deallocate(cfl)

  end subroutine ca_destroy_castro_method_params

end module meth_params_module
