!-----------------------------------------------------------------------
!
! MODULE: data_module
!> @brief
!> This module contains the variables and setup subroutines for the mock
!> cross section data. It establishes the number of groups and
!> constructs the cross section arrays.
!
!-----------------------------------------------------------------------

MODULE data_module

  USE global_module, ONLY: i_knd, r_knd, zero

  use iso_c_binding
  use sgf, only: sgf_alloc_perf, sgf_free

  IMPLICIT NONE

  PUBLIC

  SAVE
!_______________________________________________________________________
!
! Module Input Variables
!
! ng       - number of groups
! mat_opt  - material layout, 0/1/2=homogeneous/center/corner, with
!            two materials, and material 2 nowhere/center/corner
! src_opt  - source layout, 0/1/2=homogenous/src-center/src-corner=
!            source everywhere/center of problem/corner, strength=10.0
! scatp    - 0/1=no/yes print the full scattering matrix to file 'slgg'
!_______________________________________________________________________

  INTEGER(i_knd) :: ng=1, mat_opt=0, src_opt=0, scatp=0
!_______________________________________________________________________
!
! Run-time variables
!
! v(ng)         - mock velocity array
! nmat          - number of materials
! mat(nx,ny,nz) - material identifier array
!
! qi(nx,ny,nz,ng)             - fixed source array for src_opt<3
! qim(nang,nx,ny,nz,noct,ng)  - fixed source array for src_opt>=3
!
! sigt(nmat,ng)          - total interaction
! siga(nmat,ng)          - absorption
! sigs(nmat,ng)          - scattering, total
! slgg(nmat,nmom,ng,ng)  - scattering matrix, all moments/groups
! vdelt(ng)              - time-absorption coefficient
!_______________________________________________________________________

  INTEGER(i_knd) :: nmat=1

  INTEGER(i_knd), ALLOCATABLE, DIMENSION(:,:,:) :: mat

  REAL(r_knd), ALLOCATABLE, DIMENSION(:) :: v, vdelt

  REAL(r_knd), ALLOCATABLE, DIMENSION(:,:) :: sigt, siga, sigs

  REAL(r_knd), pointer, DIMENSION(:,:,:,:) :: qi, slgg

  REAL(r_knd), pointer, DIMENSION(:,:,:,:,:,:) :: qim

  type(c_ptr) :: c_tmp
  integer(c_size_t) :: qidim, qimdim


  CONTAINS


  SUBROUTINE data_allocate ( nx, ny, nz, nmom, nang, noct, timedep,    &
    istat )

!-----------------------------------------------------------------------
!
! Allocate data_module arrays.
!
!-----------------------------------------------------------------------

    INTEGER(i_knd), INTENT(IN) :: nx, ny, nz, nmom, nang, noct, timedep

    INTEGER(i_knd), INTENT(INOUT) :: istat

!_______________________________________________________________________
!
!   Establish number of materials according to mat_opt
!_______________________________________________________________________

    IF ( mat_opt > 0 ) nmat = 2
!_______________________________________________________________________
!
!   Allocate velocities
!_______________________________________________________________________

    istat = 0

    IF ( timedep == 1 ) THEN
      ALLOCATE( v(ng), STAT=istat )
    ELSE
      ALLOCATE( v(0), STAT=istat )
    END IF
    IF ( istat /= 0 ) RETURN

    v = zero
!_______________________________________________________________________
!
!   Allocate the material identifier array. ny and nz are 1 if not
!   2-D/3-D.
!_______________________________________________________________________

    ALLOCATE( mat(nx,ny,nz), STAT=istat )
    IF ( istat /= 0 ) RETURN

    mat = 1
!_______________________________________________________________________
!
!   Allocate the fixed source array. If src_opt < 3, allocate the qi
!   array, not the qim. Do the opposite (store the full angular copy) of
!   the source, qim, if src_opt>=3 (MMS). Allocate array not used to 0.
!   ny and nz are 1 if not 2-D/3-D.
!_______________________________________________________________________

    IF ( src_opt < 3 ) THEN
      qidim = int(nx, c_size_t) * int(ny, c_size_t) * int(nz, c_size_t)&
        * int(ng, c_size_t) * int(r_knd, c_size_t)
      c_tmp = sgf_alloc_perf(qidim)
      call c_f_pointer(c_tmp, qi, shape=[nx,ny,nz,ng])
      qimdim = 0_8
      ALLOCATE( qim(0,0,0,0,0,0), STAT=istat )
      !ALLOCATE( qi(nx,ny,nz,ng), qim(0,0,0,0,0,0), STAT=istat )
      IF ( istat /= 0 ) RETURN
      qi = zero
    ELSE
      qidim = int(nx, c_size_t) * int(ny, c_size_t) * int(nz, c_size_t)&
        * int(ng, c_size_t) * int(r_knd, c_size_t)
      c_tmp = sgf_alloc_perf(qidim)
      call c_f_pointer(c_tmp, qi, shape=[nx,ny,nz,ng])
      qimdim = int(nang, c_size_t) * int(nx, c_size_t) * int(ny, c_size_t)&
        * int(nz, c_size_t) * int(noct, c_size_t) * int(ng, c_size_t)&
        * int(r_knd, c_size_t)
      c_tmp = sgf_alloc_perf(qimdim)
      call c_f_pointer(c_tmp, qim, shape=[nang,nx,ny,nz,noct,ng])
      !ALLOCATE( qi(nx,ny,nz,ng), qim(nang,nx,ny,nz,noct,ng),           &
        !STAT=istat )
      IF ( istat /= 0 ) RETURN
      qi = zero
      qim = zero
    END IF
!_______________________________________________________________________
!
!   Allocate mock cross sections
!_______________________________________________________________________

    ALLOCATE( sigt(nmat,ng), siga(nmat,ng), sigs(nmat,ng),             &
      slgg(nmat,nmom,ng,ng), STAT=istat )
    IF ( istat /= 0 ) RETURN

    sigt = zero
    siga = zero
    sigs = zero
    slgg = zero
!_______________________________________________________________________
!
!   Allocate the vdelt array
!_______________________________________________________________________

    ALLOCATE( vdelt(ng), STAT=istat )
    IF ( istat /= 0 ) RETURN

    vdelt = zero
!_______________________________________________________________________
!_______________________________________________________________________

  END SUBROUTINE data_allocate


  SUBROUTINE data_deallocate

!-----------------------------------------------------------------------
!
! Deallocate data_module arrays.
!
!-----------------------------------------------------------------------
!_______________________________________________________________________

    DEALLOCATE( v )
    DEALLOCATE( mat )
    !DEALLOCATE( qi, qim )
    call sgf_free(c_loc(qi))
    IF ( src_opt < 3 ) THEN
      deallocate(qim)
    ELSE
      call sgf_free(c_loc(qim))
    END IF
    DEALLOCATE( sigt, siga, sigs )
    DEALLOCATE( slgg )
    DEALLOCATE( vdelt )
!_______________________________________________________________________
!_______________________________________________________________________

  END SUBROUTINE data_deallocate


END MODULE data_module
