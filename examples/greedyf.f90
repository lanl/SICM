program greedyf
  use sgf
  use iso_c_binding
  implicit none

  integer, parameter :: sz = 1048576
  integer(4), pointer :: blob(:)
  integer(4) :: i
  type(c_ptr) :: c_tmp

  call sgf_init(0)
  c_tmp = sgf_alloc_perf(int(sz * 4, c_size_t))
  call c_f_pointer(c_tmp, blob, shape=[sz])
  do i = 1, sz
    blob(i) = i
  end do
  print *, blob(1:10)
  print *, blob(sz-10:sz)
  call sgf_free(c_tmp)
end program greedyf
