module probdata_module

  use amrex_fort_module, only: rt => amrex_real

  implicit none

  real(rt), allocatable, public :: p_ambient, dens_ambient, exp_energy, e_ambient
  real(rt), allocatable, public :: r_init
  integer,  allocatable, public :: nsub

#ifdef AMREX_USE_CUDA
  attributes(managed) :: p_ambient, dens_ambient, exp_energy, &
                         r_init, nsub, e_ambient
#endif
  
end module probdata_module
