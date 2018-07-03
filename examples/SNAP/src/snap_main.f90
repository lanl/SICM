PROGRAM snap_main

!-----------------------------------------------------------------------
!
!    SNAP - SN Application Proxy
!
!    Parallel programming model based on PARTISN
!
!
!    SNAP: SN (Discrete Ordinates) Application Proxy
!    Version 1.x (C13087)
!    LA-CC-13-016
!
!    This code is Unclassified, and contains no Unclassified Controlled
!    Nuclear Information
!
!    Copyright (c) 2013, Los Alamos National Security, LLC
!    All rights reserved.
!
!    Copyright 2013. Los Alamos National Security, LLC. This software
!    was produced under U.S. Government contract DE-AC52-06NA25396 for
!    Los Alamos National Laboratory (LANL), which is operated by Los
!    Alamos National Security, LLC for the U.S. Department of Energy.
!    The U.S. Government has rights to use, reproduce, and distribute
!    this software. NEITHER THE GOVERNMENT NOR LOS ALAMOS NATIONAL
!    SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES
!    ANY LIABILITY FOR THE USE OF THIS SOFTWARE. If software is
!    modified to produce derivative works, such modified software should
!    be clearly marked, so as not to confuse it with the version
!    available from LANL.
!
!    Additionally, redistribution and use in source and binary forms,
!    with or without modification, are permitted provided that the
!    following conditions are met:
!    --Redistributions of source code must retain the above copyright
!      notice, this list of conditions and the following disclaimer.
!    --Redistributions in binary form must reproduce the above copyright
!      notice, this list of conditions and the following disclaimer in
!      the documentation and/or other materials provided with the
!      distribution.
!    --Neither the name of Los Alamos National Security, LLC, Los Alamos
!      National Laboratory, LANL, the U.S. Government, nor the names of
!      its contributors may be used to endorse or promote products
!      derived from this software without specific prior written
!      permission.
!
!    THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND
!    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
!    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
!    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
!    DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL SECURITY, LLC OR
!    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
!    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
!    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
!    USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
!    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
!    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
!    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
!    SUCH DAMAGE.
!
!-----------------------------------------------------------------------

  USE global_module, ONLY: i_knd, r_knd, ifile, ofile, iunit, ounit, allocator

  USE utils_module

  USE version_module, ONLY: version_print

  USE input_module, ONLY: input_read

  USE setup_module, ONLY: setup

  USE output_module, ONLY: output

  USE time_module, ONLY: tsnap, wtime, tparset, time_summ, tgrind

  USE dealloc_module

  USE plib_module, ONLY: pinit, iproc, root, comm_snap, bcast,         &
    pcomm_set, pinit_omp

  USE control_module, ONLY: otrdone, swp_typ

  use geom_module, only: nx, ny, nz
  use data_module, only: ng
  use sn_module, only: nang

  use iso_c_binding
  use sicm_f90

  IMPLICIT NONE
!_______________________________________________________________________
!
! Local variables
!_______________________________________________________________________

  CHARACTER(LEN=1) :: star='*'

  CHARACTER(LEN=64) :: error

  INTEGER(i_knd) :: ierr, i

  REAL(r_knd) :: t1, t2, t3, t4, t5
  integer(8) :: start, finish, clock_rate
  integer(c_int) :: pin_res

  type(sf_device_list) :: devices

  call system_clock(start, clock_rate)

  ! Our SICM initialization is done in three steps:
  ! sf_init looks for the memory devices that are available on the
  ! machine, and populates the devices list
  ! sf_get_device let us actually fetch an allocator, which is stored
  ! globally
  ! since this version was set up primarily for sanity-checking and
  ! timings, the device is selected literally
  ! device 2 will typically be a huge page allocator on numa node 0,
  ! but see examples/basic.c for an example of figuring out what all the
  ! memory devices actually are
  ! (note, some of the functions have not yet been wrapped for FORTRAN,
  ! so replicating that here may require writing some more wrappers)
  ! finally, sf_pin ensure the process only runs local to the memory
  call sf_init(devices)
  allocator = sf_get_device(devices, 1)
  pin_res = sf_pin(allocator)

!_______________________________________________________________________
!
! Perform calls that set up the parallel environment in MPI and
! OpenMP. Also starts the timer. Update parallel setup time.
!_______________________________________________________________________

  ierr = 0
  error = ' '

  CALL pinit ( t1 )

  CALL wtime ( t2 )
  tparset = tparset + t2 - t1
!_______________________________________________________________________
!
! Read the command line arguments to get i/o file names. Open the two
! files.
!_______________________________________________________________________

  CALL cmdarg ( ierr, error )
  CALL bcast ( ierr, comm_snap, root )
  IF ( ierr /= 0 ) THEN
    CALL print_error ( 0, error )
    CALL stop_run ( 0, 0, 0, 0 )
  END IF

  CALL open_file ( iunit, ifile, 'OLD', 'READ', ierr, error )
  CALL bcast ( ierr, comm_snap, root )
  IF ( ierr /= 0 ) THEN
    CALL print_error ( 0, error )
    CALL stop_run ( 0, 0, 0, 0 )
  END IF

  CALL open_file ( ounit, ofile, 'REPLACE', 'WRITE', ierr, error )
  CALL bcast ( ierr, comm_snap, root )
  IF ( ierr /= 0 ) THEN
    CALL print_error ( 0, error )
    CALL stop_run ( 0, 0, 0, 0 )
  END IF
!_______________________________________________________________________
!
! Write code version and execution time to output.
!_______________________________________________________________________

  IF ( iproc == root ) CALL version_print
!_______________________________________________________________________
!
! Read input
!_______________________________________________________________________

  CALL input_read

  CALL close_file ( iunit, ierr, error )
  CALL bcast ( ierr, comm_snap, root )
  IF ( ierr /= 0 ) THEN
    CALL print_error ( ounit, error )
    CALL stop_run ( 0, 0, 0, 0 )
  END IF
!_______________________________________________________________________
!
! Get nthreads for each proc. Print the warning about resetting nthreads
! if necessary. Don't stop run. Set up the SDD MPI topology.
!_______________________________________________________________________

  CALL wtime ( t3 )

  CALL pinit_omp ( ierr, error )
  IF ( ierr /= 0 ) CALL print_error ( 0, error )

  CALL pcomm_set

  CALL wtime ( t4 )
  tparset = tparset + t4 - t3
!_______________________________________________________________________
!
! Setup problem
!_______________________________________________________________________

  CALL setup
!_______________________________________________________________________
!
! Call for the problem solution
!_______________________________________________________________________

  CALL translv
!_______________________________________________________________________
!
! Output the results. Print the timing summary.
!_______________________________________________________________________

  CALL output
  IF ( iproc == root ) CALL time_summ
!_______________________________________________________________________
!
! Final cleanup: deallocate, close output file, end the program
!_______________________________________________________________________

  CALL dealloc_input ( 4 )
  CALL dealloc_solve ( swp_typ, 2 )

  CALL wtime ( t5 )
  tsnap = t5 - t1

  !IF ( iproc == root ) THEN
    !WRITE( ounit, 501 ) tsnap
    !WRITE( ounit, 502 ) tgrind, ( star, i = 1, 80 )
  !END IF

  CALL close_file ( ounit, ierr, error )
  CALL bcast ( ierr, comm_snap, root )
  call system_clock(finish)
  write (6,*) "time:", 1000 * (finish - start) / clock_rate, "ms"
  write (6,*) "memory:", 16_8 * int(nx, c_size_t) * int(ny, c_size_t) * &
    int(nz, c_size_t) * int(nang, c_size_t) * int(ng, c_size_t) * int(r_knd, c_size_t), "B"
  IF ( ierr /= 0 ) THEN
    CALL print_error ( 0, error )
    CALL stop_run ( 1, 0, 0, 0 )
  END IF

  IF ( otrdone ) THEN
    CALL stop_run ( 1, 0, 0, 1 )
  ELSE
    CALL stop_run ( 1, 0, 0, 2 )
  END IF
!_______________________________________________________________________

  501 FORMAT( 2X, 'Total Execution time', T41, ES11.4, / )
  502 FORMAT( 2X, 'Grind Time (nanoseconds)', 8X, ES11.4, /, /, 80A )
!_______________________________________________________________________
!_______________________________________________________________________

END PROGRAM snap_main
