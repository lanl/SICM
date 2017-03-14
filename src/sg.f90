module sgf
  use iso_c_binding
  implicit none

  contains

  subroutine sgf_init(id) bind(C)
    integer(c_int), intent(in) :: id
    call sg_init_wrap(id)
  end subroutine sgf_init

  function sgf_alloc_exact(sz) bind(C)
    type(c_ptr) :: sgf_alloc_exact
    integer(c_size_t), intent(in) :: sz
    call sg_alloc_exact_wrap(sz, sgf_alloc_exact)
  end function sgf_alloc_exact

  function sgf_alloc_perf(sz) bind(C)
    type(c_ptr) :: sgf_alloc_perf
    integer(c_size_t), intent(in) :: sz
    call sg_alloc_perf_wrap(sz, sgf_alloc_perf)
  end function sgf_alloc_perf

  function sgf_alloc_cap(sz) bind(C)
    type(c_ptr) :: sgf_alloc_cap
    integer(c_size_t), intent(in) :: sz
    call sg_alloc_cap_wrap(sz, sgf_alloc_cap)
  end function sgf_alloc_cap

  subroutine sgf_free(ptr) bind(C)
    type(c_ptr), intent(in) :: ptr
    call sg_free_wrap(ptr)
  end subroutine sgf_free
end module sgf
