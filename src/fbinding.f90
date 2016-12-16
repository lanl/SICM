module sicm_f90
  use iso_c_binding
  implicit none

  type, bind(C) :: sf_device
    type(c_ptr) :: device
  end type sf_device

  type, bind(C) :: sf_device_list
    integer(c_int) :: device_count
    type(c_ptr) :: devices
  end type sf_device_list
  
  type, bind(C) :: sf_timing
    integer(c_int) :: alloc_time, read_time, write_time, free_time
  end type sf_timing
  
  contains
  
  subroutine sf_init(devices) bind(C)
    type(sf_device_list), intent(inout) :: devices
    call sicm_init_wrap(devices)
  end subroutine sf_init

  function sf_get_device(devices, i) bind(C)
    type(sf_device) :: sf_get_device
    type(sf_device_list), intent(in) :: devices
    integer(c_int), intent(in) :: i
    type(c_ptr) :: ptr
    call sicm_get_device_wrap(devices, i, ptr)
    sf_get_device%device = ptr
  end function sf_get_device

  function sf_alloc(device, sz) bind(C)
    type(c_ptr) :: sf_alloc
    type(sf_device), intent(in) :: device
    integer(c_size_t), intent(in) :: sz
    call sicm_alloc_wrap(device%device, sz, sf_alloc)
  end function sf_alloc
  
  function sf_move(src, dst, ptr, sz) bind(C)
    integer(c_int) :: sf_move
    type(sf_device), intent(in) :: src, dst
    type(c_ptr), intent(in) :: ptr
    integer(c_size_t), intent(in) :: sz
    call sicm_move_wrap(src%device, dst%device, ptr, sz, sf_move)
  end function sf_move
  
  function sf_capacity(device) bind(C)
    integer(c_size_t) :: sf_capacity
    type(sf_device), intent(in) :: device
    call sicm_capacity_wrap(device%device, sf_capacity)
  end function sf_capacity
  
  function sf_used(device) bind(C)
    integer(c_size_t) :: sf_used
    type(sf_device), intent(in) :: device
    call sicm_used_wrap(device%device, sf_used)
  end function sf_used
  
  function sf_model_distance(device) bind(C)
    integer(c_int) :: sf_model_distance
    type(sf_device), intent(in) :: device
    call sicm_model_distance_wrap(device%device, sf_model_distance)
  end function sf_model_distance
  
  function sf_latency(device, sz, iter) bind(C)
    type(sf_timing) :: sf_latency
    type(sf_device), intent(in) :: device
    integer(c_size_t), intent(in) :: sz
    integer(c_int), intent(in) :: iter
    call sicm_latency_wrap(device%device, sz, iter, sf_latency)
  end function sf_latency
end module sicm_f90
