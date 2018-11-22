subroutine ca_network_init() bind(C, name="ca_network_init")

  use network, only: network_init
  call network_init()

end subroutine ca_network_init


! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_extern_init(name,namlen) bind(C, name="ca_extern_init")

  ! initialize the external runtime parameters in
  ! extern_probin_module

  use amrex_fort_module, only: rt => amrex_real

  integer, intent(in) :: namlen
  integer, intent(in) :: name(namlen)

  call runtime_init(name,namlen)

end subroutine ca_extern_init

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_num_spec(nspec_out) bind(C, name="ca_get_num_spec")

  use network, only: nspec
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(out) :: nspec_out

  nspec_out = nspec

end subroutine ca_get_num_spec

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_num_aux(naux_out) bind(C, name="ca_get_num_aux")

  use network, only: naux
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(out) :: naux_out

  naux_out = naux

end subroutine ca_get_num_aux

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_spec_names(spec_names,ispec,len) &
     bind(C, name="ca_get_spec_names")

  use network, only: nspec, short_spec_names
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(in   ) :: ispec
  integer, intent(inout) :: len
  integer, intent(inout) :: spec_names(len)

  ! Local variables
  integer   :: i

  len = len_trim(short_spec_names(ispec+1))

  do i = 1,len
     spec_names(i) = ichar(short_spec_names(ispec+1)(i:i))
  end do

end subroutine ca_get_spec_names

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_spec_az(ispec,A,Z) bind(C, name="ca_get_spec_az")

  use network, only: nspec, aion, zion
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer,  intent(in   ) :: ispec
  real(rt), intent(inout) :: A, Z

  ! C++ is 0-based indexing, so increment
  A = aion(ispec+1)
  Z = zion(ispec+1)

end subroutine ca_get_spec_az

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_aux_names(aux_names,iaux,len) &
     bind(C, name="ca_get_aux_names")

  use network, only: naux, short_aux_names
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(in   ) :: iaux
  integer, intent(inout) :: len
  integer, intent(inout) :: aux_names(len)

  ! Local variables
  integer   :: i

  len = len_trim(short_aux_names(iaux+1))

  do i = 1,len
     aux_names(i) = ichar(short_aux_names(iaux+1)(i:i))
  end do

end subroutine ca_get_aux_names

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_qvar(qvar_in) bind(C, name="ca_get_qvar")

  use meth_params_module, only: QVAR
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(inout) :: qvar_in

  qvar_in = QVAR

end subroutine ca_get_qvar

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_nqaux(nqaux_in) bind(C, name="ca_get_nqaux")

  use meth_params_module, only: NQAUX
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(inout) :: nqaux_in

  nqaux_in = NQAUX

end subroutine ca_get_nqaux

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_ngdnv(ngdnv_in) bind(C, name="ca_get_ngdnv")

  use meth_params_module, only: NGDNV

  implicit none

  integer, intent(inout) :: ngdnv_in

  ngdnv_in = NGDNV

end subroutine ca_get_ngdnv

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_method_params(nGrowHyp) bind(C, name="ca_get_method_params")

  ! Passing data from F90 back to C++

  use meth_params_module, only: NHYP
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(out) :: ngrowHyp

  nGrowHyp = NHYP

end subroutine ca_get_method_params

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine allocate_outflow_data(np,nc) &
     bind(C, name="allocate_outflow_data")

  use meth_params_module, only: outflow_data_old, outflow_data_new, outflow_data_allocated
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(in) :: np,nc

  if (.not. outflow_data_allocated) then
     allocate(outflow_data_old(nc,np))
     allocate(outflow_data_new(nc,np))
  end if

  outflow_data_allocated = .true.

end subroutine allocate_outflow_data

! :::
! ::: ----------------------------------------------------------------
! :::
subroutine set_old_outflow_data(radial,time,np,nc) &
     bind(C, name="set_old_outflow_data")

  ! Passing data from C++ to F90

  use meth_params_module, only: outflow_data_old, outflow_data_old_time
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  real(rt), intent(in) :: radial(nc,np)
  real(rt), intent(in) :: time
  integer,  intent(in) :: np,nc

  ! Do this so the routine has the right size
  deallocate(outflow_data_old)
  allocate(outflow_data_old(nc,np))

  outflow_data_old(1:nc,1:np) = radial(1:nc,1:np)

  outflow_data_old_time = time

end subroutine set_old_outflow_data

! :::
! ::: ----------------------------------------------------------------
! :::
subroutine set_new_outflow_data(radial,time,np,nc) &
     bind(C, name="set_new_outflow_data")

  ! Passing data from C++ to F90

  use meth_params_module, only: outflow_data_new, outflow_data_new_time
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  real(rt), intent(in) :: radial(nc,np)
  real(rt), intent(in) :: time
  integer,  intent(in) :: np,nc

  ! Do this so the routine has the right size
  deallocate(outflow_data_new)
  allocate(outflow_data_new(nc,np))

  outflow_data_new(1:nc,1:np) = radial(1:nc,1:np)

  outflow_data_new_time = time

end subroutine set_new_outflow_data

! :::
! ::: ----------------------------------------------------------------
! :::
subroutine swap_outflow_data() bind(C, name="swap_outflow_data")

  use meth_params_module, only: outflow_data_new, outflow_data_new_time, &
                                outflow_data_old, outflow_data_old_time
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer :: np, nc

  nc = size(outflow_data_new,dim=1)
  np = size(outflow_data_new,dim=2)

  if (size(outflow_data_old,dim=2) .ne. size(outflow_data_new,dim=2)) then
     ! Do this so the routine has the right size
     deallocate(outflow_data_old)
     allocate(outflow_data_old(nc,np))
  end if

  if (size(outflow_data_old,dim=2) .ne. size(outflow_data_new,dim=2)) then
     print *,'size of old and new dont match in swap_outflow_data '
     call bl_error("Error:: Castro_nd.F90 :: swap_outflow_data")
  end if

  outflow_data_old(1:nc,1:np) = outflow_data_new(1:nc,1:np)

  if (outflow_data_new_time .ge. 0.e0_rt) &
       outflow_data_old_time = outflow_data_new_time
  outflow_data_new_time = -1.e0_rt

end subroutine swap_outflow_data

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_set_method_params(dm,Density,Xmom,Eden,Eint,Temp, &
                                FirstAdv,FirstSpec,FirstAux,numadv) &
                                bind(C, name="ca_set_method_params")

  use meth_params_module
  use network, only: nspec, naux
  use eos_module, only: eos_init
  use eos_type_module, only: eos_get_small_dens, eos_get_small_temp
  use amrex_constants_module, only: ZERO, ONE
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(in) :: dm
  integer, intent(in) :: Density, Xmom, Eden, Eint, Temp, &
       FirstAdv, FirstSpec, FirstAux
  integer, intent(in) :: numadv
  integer :: iadv, ispec

  integer :: i
  integer :: ioproc

  ! easy indexing for the passively advected quantities.  This
  ! lets us loop over all groups (advected, species, aux)
  ! in a single loop.
  allocate(qpass_map(QVAR))
  allocate(upass_map(NVAR))
  allocate(npassive)

  ! Transverse velocities

  if (dm == 1) then
     upass_map(1) = UMY
     qpass_map(1) = QV

     upass_map(2) = UMZ
     qpass_map(2) = QW

     npassive = 2

  else if (dm == 2) then
     upass_map(1) = UMZ
     qpass_map(1) = QW

     npassive = 1
  else
     npassive = 0
  endif

  do iadv = 1, nadv
     upass_map(npassive + iadv) = UFA + iadv - 1
     qpass_map(npassive + iadv) = QFA + iadv - 1
  enddo
  npassive = npassive + nadv

  if (QFS > -1) then
     do ispec = 1, nspec+naux
        upass_map(npassive + ispec) = UFS + ispec - 1
        qpass_map(npassive + ispec) = QFS + ispec - 1
     enddo
     npassive = npassive + nspec + naux
  endif


  !---------------------------------------------------------------------
  ! other initializations
  !---------------------------------------------------------------------

  ! This is a routine which links to the C++ ParallelDescriptor class

  call bl_pd_is_ioproc(ioproc)

  !---------------------------------------------------------------------
  ! safety checks
  !---------------------------------------------------------------------

  if (small_dens <= 0.e0_rt) then
     if (ioproc == 1) then
        call bl_warning("Warning:: small_dens has not been set, defaulting to 1.e-200_rt.")
     endif
     small_dens = 1.e-200_rt
  endif

  if (small_temp <= 0.e0_rt) then
     if (ioproc == 1) then
        call bl_warning("Warning:: small_temp has not been set, defaulting to 1.e-200_rt.")
     endif
     small_temp = 1.e-200_rt
  endif

  ! Note that the EOS may modify our choices because of its
  ! internal limitations, so the small_dens and small_temp
  ! may be modified coming back out of this routine.

  call eos_init(small_dens=small_dens, small_temp=small_temp)

  ! Update device variables

  !$acc update &
  !$acc device(small_dens, small_temp)

end subroutine ca_set_method_params



subroutine ca_destroy_method_params() bind(C, name="ca_destroy_method_params")

  use meth_params_module, only: qpass_map, upass_map, npassive

  implicit none

  deallocate(qpass_map)
  deallocate(upass_map)
  deallocate(npassive)

end subroutine ca_destroy_method_params



subroutine ca_init_godunov_indices() bind(C, name="ca_init_godunov_indices")

  use meth_params_module, only: GDRHO, GDU, GDV, GDW, GDPRES, GDGAME, NGDNV, &
                                QU, QV, QW
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  ! sanity check
  if ((QU /= GDU) .or. (QV /= GDV) .or. (QW /= GDW)) then
     call bl_error("ERROR: velocity components for godunov and primitive state are not aligned")
  endif

end subroutine ca_init_godunov_indices

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_set_problem_params(dm,physbc_lo_in,physbc_hi_in,&
                                 Interior_in, Inflow_in, Outflow_in, &
                                 Symmetry_in, SlipWall_in, NoSlipWall_in, &
                                 coord_type_in, &
                                 problo_in, probhi_in, center_in) &
                                 bind(C, name="ca_set_problem_params")

  ! Passing data from C++ into F90

  use amrex_constants_module, only: ZERO
  use prob_params_module
  use meth_params_module, only: UMX, UMY, UMZ
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer,  intent(in) :: dm
  integer,  intent(in) :: physbc_lo_in(dm),physbc_hi_in(dm)
  integer,  intent(in) :: Interior_in, Inflow_in, Outflow_in, Symmetry_in, SlipWall_in, NoSlipWall_in
  integer,  intent(in) :: coord_type_in
  real(rt), intent(in) :: problo_in(dm), probhi_in(dm), center_in(dm)

  allocate(dim)

  dim = dm

  allocate(physbc_lo(3))
  allocate(physbc_hi(3))

  physbc_lo(1:dm) = physbc_lo_in(1:dm)
  physbc_hi(1:dm) = physbc_hi_in(1:dm)

  allocate(Interior)
  allocate(Inflow)
  allocate(Outflow)
  allocate(Symmetry)
  allocate(SlipWall)
  allocate(NoSlipWall)

  allocate(center(3))
  allocate(problo(3))
  allocate(probhi(3))
  
  Interior   = Interior_in
  Inflow     = Inflow_in
  Outflow    = Outflow_in
  Symmetry   = Symmetry_in
  SlipWall   = SlipWall_in
  NoSlipWall = NoSlipWall_in

  coord_type = coord_type_in

  problo = ZERO
  probhi = ZERO
  center = ZERO

  problo(1:dm) = problo_in(1:dm)
  probhi(1:dm) = probhi_in(1:dm)
  center(1:dm) = center_in(1:dm)

  allocate(dg(3))

  dg(:) = 1

  if (dim .lt. 2) then
     dg(2) = 0
  endif

  if (dim .lt. 3) then
     dg(3) = 0
  endif

  ! sanity check on our allocations
  if (UMZ > MAX_MOM_INDEX) then
     call bl_error("ERROR: not enough space in comp in mom_flux_has_p")
  endif

  ! keep track of which components of the momentum flux have pressure
  if (dim == 1 .or. (dim == 2 .and. coord_type == 1)) then
     mom_flux_has_p(1)%comp(UMX) = .false.
  else
     mom_flux_has_p(1)%comp(UMX) = .true.
  endif
  mom_flux_has_p(1)%comp(UMY) = .false.
  mom_flux_has_p(1)%comp(UMZ) = .false.

  mom_flux_has_p(2)%comp(UMX) = .false.
  mom_flux_has_p(2)%comp(UMY) = .true.
  mom_flux_has_p(2)%comp(UMZ) = .false.

  mom_flux_has_p(3)%comp(UMX) = .false.
  mom_flux_has_p(3)%comp(UMY) = .false.
  mom_flux_has_p(3)%comp(UMZ) = .true.

end subroutine ca_set_problem_params



subroutine ca_destroy_problem_params() bind(C, name="ca_destroy_problem_params")

  use prob_params_module

  deallocate(dim)

  deallocate(physbc_lo)
  deallocate(physbc_hi)

  deallocate(Interior)
  deallocate(Inflow)
  deallocate(Outflow)
  deallocate(Symmetry)
  deallocate(SlipWall)
  deallocate(NoSlipWall)

  deallocate(center)
  deallocate(problo)
  deallocate(probhi)

  deallocate(dg)

end subroutine ca_destroy_problem_params

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_set_grid_info(max_level_in, dx_level_in, domlo_in, domhi_in, &
                            ref_ratio_in, n_error_buf_in, blocking_factor_in) &
                            bind(C, name="ca_set_grid_info")

  use prob_params_module, only: max_level, dx_level, domlo_level, domhi_level, n_error_buf, ref_ratio, blocking_factor
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer,  intent(in) :: max_level_in
  real(rt), intent(in) :: dx_level_in(3*(max_level_in+1))
  integer,  intent(in) :: domlo_in(3*(max_level_in+1)), domhi_in(3*(max_level_in+1))
  integer,  intent(in) :: ref_ratio_in(3*(max_level_in+1))
  integer,  intent(in) :: n_error_buf_in(0:max_level_in)
  integer,  intent(in) :: blocking_factor_in(0:max_level_in)

  integer :: lev, dir

  ! Sometimes this routine can get called multiple
  ! times upon initialization; in this case, just to
  ! be safe, we'll deallocate and start again.

  if (allocated(dx_level)) then
     deallocate(dx_level)
  endif
  if (allocated(domlo_level)) then
     deallocate(domlo_level)
  endif
  if (allocated(domhi_level)) then
     deallocate(domhi_level)
  endif

  if (allocated(ref_ratio)) then
     deallocate(ref_ratio)
  endif
  if (allocated(n_error_buf)) then
     deallocate(n_error_buf)
  endif
  if (allocated(blocking_factor)) then
     deallocate(blocking_factor)
  endif

  max_level = max_level_in

  allocate(dx_level(1:3, 0:max_level))
  allocate(domlo_level(1:3, 0:max_level))
  allocate(domhi_level(1:3, 0:max_level))
  allocate(ref_ratio(1:3, 0:max_level))
  allocate(n_error_buf(0:max_level))
  allocate(blocking_factor(0:max_level))

  do lev = 0, max_level
     do dir = 1, 3
        dx_level(dir,lev) = dx_level_in(3*lev + dir)
        domlo_level(dir,lev) = domlo_in(3*lev + dir)
        domhi_level(dir,lev) = domhi_in(3*lev + dir)
        ref_ratio(dir,lev) = ref_ratio_in(3*lev + dir)
     enddo
     n_error_buf(lev) = n_error_buf_in(lev)
     blocking_factor(lev) = blocking_factor_in(lev)
  enddo

end subroutine ca_set_grid_info



subroutine ca_destroy_grid_info() bind(c, name='ca_destroy_grid_info')

  use prob_params_module, only: max_level, dx_level, domlo_level, domhi_level, n_error_buf, ref_ratio, blocking_factor

  implicit none

  deallocate(dx_level)
  deallocate(domlo_level)
  deallocate(domhi_level)
  deallocate(ref_ratio)
  deallocate(n_error_buf)
  deallocate(blocking_factor)

end subroutine ca_destroy_grid_info

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_set_special_tagging_flag(dummy,flag) &
     bind(C, name="ca_set_special_tagging_flag")

  use amrex_fort_module, only: rt => amrex_real

  real(rt) :: dummy
  integer  :: flag

end subroutine ca_set_special_tagging_flag

! :::
! ::: ----------------------------------------------------------------
! :::

subroutine ca_get_tagging_params(name, namlen) &
     bind(C, name="ca_get_tagging_params")

  ! Initialize the tagging parameters

  use tagging_module
  use amrex_fort_module, only: rt => amrex_real

  integer :: namlen
  integer :: name(namlen)

  integer :: un, i, status

  integer, parameter :: maxlen = 256
  character (len=maxlen) :: probin

  namelist /tagging/ &
       denerr,   dengrad,  dengrad_rel,  max_denerr_lev,  max_dengrad_lev,  max_dengrad_rel_lev, &
       temperr, tempgrad, tempgrad_rel, max_temperr_lev, max_tempgrad_lev, max_tempgrad_rel_lev

  ! Set namelist defaults
  denerr = 1.e20_rt
  dengrad = 1.e20_rt
  dengrad_rel = 1.e20_rt
  max_denerr_lev = 10
  max_dengrad_lev = 10
  max_dengrad_rel_lev = -1

  temperr  = 1.e20_rt
  tempgrad = 1.e20_rt
  tempgrad_rel = 1.e20_rt
  max_temperr_lev = -1
  max_tempgrad_lev = -1
  max_tempgrad_rel_lev = -1

  ! create the filename
  if (namlen > maxlen) then
     call bl_error('probin file name too long')
  endif

  do i = 1, namlen
     probin(i:i) = char(name(i))
  end do

  ! read in the namelist
  un = 9
  open (unit=un, file=probin(1:namlen), form='formatted', status='old')
  read (unit=un, nml=tagging, iostat=status)

  if (status < 0) then
     ! the namelist does not exist, so we just go with the defaults
     continue

  else if (status > 0) then
     ! some problem in the namelist
     call bl_error('ERROR: problem in the tagging namelist')
  endif

  close (unit=un)

end subroutine ca_get_tagging_params





module castro_util_module

  implicit none

contains

  subroutine ca_enforce_consistent_e(lo,hi,state,s_lo,s_hi) bind(c,name='ca_enforce_consistent_e')

    use amrex_constants_module, only: HALF, ONE
    use amrex_fort_module, only: rt => amrex_real
    use meth_params_module, only: NVAR, URHO, UMX, UMY, UMZ, UEDEN, UEINT

    implicit none

    integer,  intent(in   ) :: lo(3), hi(3)
    integer,  intent(in   ) :: s_lo(3), s_hi(3)
    real(rt), intent(inout) :: state(s_lo(1):s_hi(1),s_lo(2):s_hi(2),s_lo(3):s_hi(3),NVAR)

    ! Local variables
    integer  :: i,j,k
    real(rt) :: u, v, w, rhoInv

    !$gpu

    !
    ! Enforces (rho E) = (rho e) + 1/2 rho (u^2 + v^2 + w^2)
    !
    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             rhoInv = ONE / state(i,j,k,URHO)
             u = state(i,j,k,UMX) * rhoInv
             v = state(i,j,k,UMY) * rhoInv
             w = state(i,j,k,UMZ) * rhoInv

             state(i,j,k,UEDEN) = state(i,j,k,UEINT) + &
                  HALF * state(i,j,k,URHO) * (u*u + v*v + w*w)

          end do
       end do
    end do

  end subroutine ca_enforce_consistent_e



  subroutine ca_reset_internal_e(lo,hi,u,u_lo,u_hi,verbose) bind(c,name='ca_reset_internal_e')

    use eos_module, only: eos
    use eos_type_module, only: eos_t, eos_input_re, eos_input_rt
    use network, only: nspec, naux
    use amrex_constants_module, only: ZERO, HALF, ONE
    use amrex_fort_module, only: rt => amrex_real
    use meth_params_module, only: NVAR, URHO, UMX, UMY, UMZ, UEDEN, UEINT, UFS, UFX, UTEMP, small_temp

    implicit none

    integer, intent(in) :: lo(3), hi(3)
    integer, intent(in), value :: verbose
    integer, intent(in) :: u_lo(3), u_hi(3)
    real(rt), intent(inout) :: u(u_lo(1):u_hi(1),u_lo(2):u_hi(2),u_lo(3):u_hi(3),NVAR)

    ! Local variables
    integer  :: i,j,k
    real(rt) :: Up, Vp, Wp, ke, rho_eint, eden, small_e, eint_new, rhoInv

    real(rt), parameter :: dual_energy_eta2 = 1.e-4_rt

    type (eos_t) :: eos_state

    !$gpu

    ! Reset internal energy

    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             rhoInv = ONE/u(i,j,k,URHO)
             Up = u(i,j,k,UMX) * rhoInv
             Vp = u(i,j,k,UMY) * rhoInv
             Wp = u(i,j,k,UMZ) * rhoInv
             ke = HALF * (Up**2 + Vp**2 + Wp**2)

             if (u(i,j,k,UEDEN) < ZERO) then

                if (u(i,j,k,UEINT) < ZERO) then

                   eos_state % rho   = u(i,j,k,URHO)
                   eos_state % T     = small_temp
                   eos_state % xn(:) = u(i,j,k,UFS:UFS+nspec-1) * rhoInv
                   eos_state % aux(1:naux) = u(i,j,k,UFX:UFX+naux-1) * rhoInv

                   call eos(eos_input_rt, eos_state)

                   u(i,j,k,UEINT) = u(i,j,k,URHO) * eos_state % e

                endif

                u(i,j,k,UEDEN) = u(i,j,k,UEINT) + u(i,j,k,URHO) * ke

             else

                rho_eint = u(i,j,k,UEDEN) - u(i,j,k,URHO) * ke

                ! Reset (e from e) if it's greater than eta * E.
                if (rho_eint .gt. ZERO .and. rho_eint / u(i,j,k,UEDEN) .gt. dual_energy_eta2) then

                   u(i,j,k,UEINT) = rho_eint

                   ! If not resetting and little e is negative ...
                else if (u(i,j,k,UEINT) .le. ZERO) then

                   eos_state % rho   = u(i,j,k,URHO)
                   eos_state % T     = small_temp
                   eos_state % xn(:) = u(i,j,k,UFS:UFS+nspec-1) * rhoInv
                   eos_state % aux(1:naux) = u(i,j,k,UFX:UFX+naux-1) * rhoInv

                   call eos(eos_input_rt, eos_state)

                   eint_new = eos_state % e

                   if (verbose .gt. 0) then
                      print *,'   '
                      print *,'>>> Warning: Castro_util.F90::reset_internal_energy  ',i,j,k
                      print *,'>>> ... resetting neg. e from EOS using small_temp'
                      print *,'>>> ... from ',u(i,j,k,UEINT)/u(i,j,k,URHO),' to ', eint_new
                      print *,'    '
                   end if

                   u(i,j,k,UEINT) = u(i,j,k,URHO) * eint_new

                endif

             end if
          enddo
       enddo
    enddo

  end subroutine ca_reset_internal_e



  subroutine ca_compute_temp(lo,hi,state,s_lo,s_hi) bind(c,name='ca_compute_temp')

    use network, only: nspec, naux
    use eos_module, only: eos
    use eos_type_module, only: eos_input_re, eos_t
    use amrex_constants_module, only: ZERO, ONE
    use amrex_fort_module, only: rt => amrex_real
    use meth_params_module, only: NVAR, URHO, UEDEN, UEINT, UTEMP, UFS, UFX

    implicit none

    integer , intent(in   ) :: lo(3),hi(3)
    integer , intent(in   ) :: s_lo(3),s_hi(3)
    real(rt), intent(inout) :: state(s_lo(1):s_hi(1),s_lo(2):s_hi(2),s_lo(3):s_hi(3),NVAR)

    integer  :: i,j,k
    real(rt) :: rhoInv

    type (eos_t) :: eos_state

    !$gpu

    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             ! First check the inputs for validity.

#ifndef CUDA
             if (state(i,j,k,URHO) <= ZERO) then
                print *,'   '
                print *,'>>> Error: Castro_util.F90::ca_compute_temp ',i,j,k
                print *,'>>> ... negative density ',state(i,j,k,URHO)
                print *,'    '
                call bl_error("Error:: compute_temp_nd.F90")
             end if

             if (state(i,j,k,UEINT) <= ZERO) then
                print *,'   '
                print *,'>>> Warning: Castro_util.F90::ca_compute_temp ',i,j,k
                print *,'>>> ... negative (rho e) ',state(i,j,k,UEINT)
                print *,'   '
                call bl_error("Error:: compute_temp_nd.F90")
             end if
#endif

             rhoInv = ONE / state(i,j,k,URHO)

             eos_state % rho = state(i,j,k,URHO)
             eos_state % T   = state(i,j,k,UTEMP) ! Initial guess for the EOS
             eos_state % e   = state(i,j,k,UEINT) * rhoInv
             eos_state % xn  = state(i,j,k,UFS:UFS+nspec-1) * rhoInv
             eos_state % aux = state(i,j,k,UFX:UFX+naux-1) * rhoInv

             call eos(eos_input_re, eos_state)

             state(i,j,k,UTEMP) = eos_state % T

             ! In case we've floored, or otherwise allowed the energy to change, update the energy accordingly.

             state(i,j,k,UEINT) = state(i,j,k,URHO) * eos_state % e

          enddo
       enddo
    enddo

  end subroutine ca_compute_temp
  


  subroutine ca_check_initial_species(lo, hi, state, state_lo, state_hi) bind(c,name='ca_check_initial_species')

    use network           , only: nspec
    use meth_params_module, only: NVAR, URHO, UFS
    use amrex_fort_module, only: rt => amrex_real

    implicit none

    integer, intent(in) :: lo(3), hi(3)
    integer, intent(in) :: state_lo(3), state_hi(3)
    real(rt), intent(in) :: state(state_lo(1):state_hi(1),state_lo(2):state_hi(2),state_lo(3):state_hi(3),NVAR)

    ! Local variables
    integer  :: i, j, k
    real(rt) :: spec_sum

    !$gpu

    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             spec_sum = sum(state(i,j,k,UFS:UFS+nspec-1))

             if (abs(state(i,j,k,URHO)-spec_sum) .gt. 1.e-8_rt * state(i,j,k,URHO)) then

#ifndef CUDA
                print *,'Sum of (rho X)_i vs rho at (i,j,k): ',i,j,k,spec_sum,state(i,j,k,URHO)
                call bl_error("Error:: Failed check of initial species summing to 1")
#endif

             end if

          enddo
       enddo
    enddo

  end subroutine ca_check_initial_species



  subroutine ca_normalize_species(u, u_lo, u_hi, lo, hi) bind(c,name='ca_normalize_species')

    use network, only: nspec
    use amrex_constants_module, only: ONE
    use amrex_fort_module, only: rt => amrex_real
    use extern_probin_module, only: small_x
    use meth_params_module, only: NVAR, URHO, UFS

    implicit none

    integer,  intent(in   ) :: lo(3), hi(3)
    integer,  intent(in   ) :: u_lo(3), u_hi(3)
    real(rt), intent(inout) :: u(u_lo(1):u_hi(1),u_lo(2):u_hi(2),u_lo(3):u_hi(3),NVAR)

    ! Local variables
    integer  :: i, j, k
    real(rt) :: xn(nspec)

    !$gpu

    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             xn = u(i,j,k,UFS:UFS+nspec-1)

             xn = max(small_x * u(i,j,k,URHO), min(u(i,j,k,URHO), xn))

             xn = u(i,j,k,URHO) * (xn / sum(xn))

             u(i,j,k,UFS:UFS+nspec-1) = xn

          enddo
       enddo
    enddo

  end subroutine ca_normalize_species



  subroutine ca_summass(lo,hi,rho,r_lo,r_hi,dx, &
                        vol,v_lo,v_hi,mass) bind(c,name='ca_summass')

    use amrex_fort_module, only: rt => amrex_real, amrex_add

    implicit none

    integer,  intent(in   ) :: lo(3), hi(3)
    integer,  intent(in   ) :: r_lo(3), r_hi(3)
    integer,  intent(in   ) :: v_lo(3), v_hi(3)
    real(rt), intent(in   ) :: dx(3)
    real(rt), intent(in   ) :: rho(r_lo(1):r_hi(1),r_lo(2):r_hi(2),r_lo(3):r_hi(3))
    real(rt), intent(in   ) :: vol(v_lo(1):v_hi(1),v_lo(2):v_hi(2),v_lo(3):v_hi(3))
    real(rt), intent(inout) :: mass

    integer  :: i, j, k
    real(rt) :: dm

    !$gpu

    do k = lo(3), hi(3)
       do j = lo(2), hi(2)
          do i = lo(1), hi(1)

             dm = rho(i,j,k) * vol(i,j,k)

             call amrex_add(mass, dm)

          enddo
       enddo
    enddo

  end subroutine ca_summass



  subroutine ca_hypfill(adv,adv_l1,adv_l2,adv_l3,adv_h1,adv_h2, &
                        adv_h3,domlo,domhi,dx,xlo,time,bc) bind(C, name="ca_hypfill")

    use amrex_fort_module, only: rt => amrex_real
    use bc_fill_module, only: hypfill
    use meth_params_module, only: NVAR

    implicit none

    integer,  intent(in   ) :: adv_l1, adv_l2, adv_l3, adv_h1, adv_h2, adv_h3
    integer,  intent(in   ) :: bc(3,2,NVAR)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: dx(3), xlo(3), time
    real(rt), intent(inout) :: adv(adv_l1:adv_h1,adv_l2:adv_h2,adv_l3:adv_h3,NVAR)

    call hypfill(adv, adv_l1, adv_l2, adv_l3, adv_h1, adv_h2, adv_h3, domlo, domhi, dx, xlo, time, bc)

  end subroutine ca_hypfill



  subroutine ca_denfill(adv,adv_l1,adv_l2,adv_l3,adv_h1,adv_h2, &
                        adv_h3,domlo,domhi,dx,xlo,time,bc) bind(C, name="ca_denfill")

    use amrex_fort_module, only: rt => amrex_real
    use bc_fill_module, only: denfill
    use meth_params_module, only: NVAR

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: adv_l1, adv_l2, adv_l3, adv_h1, adv_h2, adv_h3
    integer,  intent(in   ) :: bc(3,2,1)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: dx(3), xlo(3), time
    real(rt), intent(inout) :: adv(adv_l1:adv_h1,adv_l2:adv_h2,adv_l3:adv_h3)

    call denfill(adv, adv_l1, adv_l2, adv_l3, adv_h1, adv_h2, adv_h3, domlo, domhi, dx, xlo, time, bc)

  end subroutine ca_denfill

end module castro_util_module
