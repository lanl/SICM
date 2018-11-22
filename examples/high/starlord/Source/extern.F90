module extern_probin_module

  use amrex_fort_module, only: rt => amrex_real

  implicit none

  private

  logical, allocatable, public :: use_eos_coulomb
  logical, allocatable, public :: eos_input_is_constant
  real(rt), allocatable, public :: eos_ttol, eos_dtol
  real(rt), allocatable, public :: small_x
#ifdef AMREX_USE_CUDA
  attributes(managed) :: small_x
  attributes(managed) :: eos_input_is_constant
  attributes(managed) :: use_eos_coulomb
  attributes(managed) :: eos_ttol, eos_dtol
#endif

  !$acc declare create(use_eos_coulomb, eos_input_is_constant, small_x)

end module extern_probin_module

subroutine runtime_init(name,namlen)

  use extern_probin_module

  implicit none

  integer :: namlen
  integer :: name(namlen)

  integer :: un, i, status

  integer, parameter :: maxlen = 256
  character (len=maxlen) :: probin

  namelist /extern/ use_eos_coulomb
  namelist /extern/ eos_input_is_constant
  namelist /extern/ small_x
  namelist /extern/ eos_ttol
  namelist /extern/ eos_dtol

  allocate(use_eos_coulomb)
  allocate(eos_input_is_constant)
  allocate(small_x)
  allocate(eos_ttol)
  allocate(eos_dtol)

  use_eos_coulomb = .true.
  eos_input_is_constant = .false.
  small_x = 1.d-30
  eos_ttol = 1.d-8
  eos_dtol = 1.d-8

  ! create the filename
  if (namlen > maxlen) then
     print *, 'probin file name too long'
     stop
  endif

  do i = 1, namlen
     probin(i:i) = char(name(i))
  end do


  ! read in the namelist
  un = 9
  open (unit=un, file=probin(1:namlen), form='formatted', status='old')
  read (unit=un, nml=extern, iostat=status)

  if (status < 0) then
     ! the namelist does not exist, so we just go with the defaults
     continue

  else if (status > 0) then
     ! some problem in the namelist
     print *, 'ERROR: problem in the extern namelist'
     stop
  endif

  close (unit=un)

  !$acc update &
  !$acc device(use_eos_coulomb, eos_input_is_constant, small_x)

end subroutine runtime_init


subroutine ca_extern_finalize() bind(c, name='ca_extern_finalize')

  use extern_probin_module

  implicit none

  deallocate(use_eos_coulomb)
  deallocate(eos_input_is_constant)
  deallocate(small_x)
  deallocate(eos_ttol)
  deallocate(eos_dtol)

end subroutine ca_extern_finalize
