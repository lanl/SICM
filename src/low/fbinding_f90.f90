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
  
  type, bind(C) :: sf_time
    integer(c_long) :: nsec, sec
  end type sf_time
  
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
  
  subroutine sf_free(device, ptr, sz) bind(C)
    type(sf_device), intent(in) :: device
    type(c_ptr) :: ptr
    integer(c_size_t), intent(in) :: sz
    call sicm_free_wrap(device%device, ptr, sz)
  end subroutine sf_free
  
  function sf_move(src, dst, ptr, sz) bind(C)
    integer(c_int) :: sf_move
    type(sf_device), intent(in) :: src, dst
    type(c_ptr), intent(in) :: ptr
    integer(c_size_t), intent(in) :: sz
    call sicm_move_wrap(src%device, dst%device, ptr, sz, sf_move)
  end function sf_move
  
  function sf_pin(device) bind(C)
    integer(c_int) :: sf_pin
    type(sf_device), intent(in) :: device
    call sicm_pin_wrap(device%device, sf_pin)
  end function sf_pin
  
  function sf_capacity(device) bind(C)
    integer(c_size_t) :: sf_capacity
    type(sf_device), intent(in) :: device
    call sicm_capacity_wrap(device%device, sf_capacity)
  end function sf_capacity
  
  function sf_avail(device) bind(C)
    integer(c_size_t) :: sf_avail
    type(sf_device), intent(in) :: device
    call sicm_avail_wrap(device%device, sf_avail)
  end function sf_avail
  
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
  
  subroutine sf_system_debug(path) bind(C)
    character(kind=c_char) :: path(*)
    call sicm_system_debug(path)
  end subroutine sf_system_debug
  
  function sf_bandwidth_linear2(device, sz, kernel)
    integer(c_size_t) :: sf_bandwidth_linear2
    type(sf_device), intent(in) :: device
    integer(c_size_t), intent(in) :: sz
    integer(c_size_t) :: kernel
    integer(c_size_t) :: bytes, i
    type(sf_time) :: stime, etime
    type(c_ptr) :: a_, b_
    double precision, pointer, dimension(:) :: a, b
    
    a_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(a_, a, shape=[sz])
    b_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(b_, b, shape=[sz])
    
!$omp parallel do
    do i=1,sz
      a(i) = 1
      b(i) = 2
    end do
    
    call sicm_get_time(stime)
    bytes = kernel(a, b, sz)
    call sicm_get_time(etime)
    
    sf_bandwidth_linear2 = bytes / ((etime%sec - stime%sec) * 1000000 + (etime%nsec - stime%nsec) / 1000);
    
    call sf_free(device, a_, sz * 8_8)
    call sf_free(device, b_, sz * 8_8)
  end function sf_bandwidth_linear2
  
  function sf_bandwidth_random2(device, sz, kernel)
    integer(c_size_t) :: sf_bandwidth_random2
    type(sf_device), intent(in) :: device
    integer(c_size_t), intent(in) :: sz
    integer(c_size_t) :: kernel
    integer(c_size_t) :: bytes, i
    type(sf_time) :: stime, etime
    type(c_ptr) :: a_, b_, indexes_
    double precision, pointer, dimension(:) :: a, b
    integer(c_size_t), pointer, dimension(:) :: indexes
    
    a_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(a_, a, shape=[sz])
    b_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(b_, b, shape=[sz])
    indexes_ = sf_alloc(device, sz * c_size_t)
    call c_f_pointer(indexes_, indexes, shape=[sz])
    
!$omp parallel do
    do i=1,sz
      a(i) = 1
      b(i) = 2
      call sicm_index_hash(i, sz, indexes(i))
    end do
    
    call sicm_get_time(stime)
    bytes = kernel(a, b, indexes, sz)
    call sicm_get_time(etime)
    
    sf_bandwidth_random2 = bytes / ((etime%sec - stime%sec) * 1000000 + (etime%nsec - stime%nsec) / 1000);
    
    call sf_free(device, a_, sz * 8_8)
    call sf_free(device, b_, sz * 8_8)
    call sf_free(device, indexes_, sz * c_size_t)
  end function sf_bandwidth_random2
  
  function sf_bandwidth_linear3(device, sz, kernel)
    integer(c_size_t) :: sf_bandwidth_linear3
    type(sf_device), intent(in) :: device
    integer(c_size_t), intent(in) :: sz
    integer(c_size_t) :: kernel
    integer(c_size_t) :: bytes, i
    type(sf_time) :: stime, etime
    type(c_ptr) :: a_, b_, c_
    double precision, pointer, dimension(:) :: a, b, c
    
    a_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(a_, a, shape=[sz])
    b_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(b_, b, shape=[sz])
    c_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(c_, c, shape=[sz])
    
!$omp parallel do
    do i=1,sz
      a(i) = 1
      b(i) = 2
      c(i) = 3
    end do
    
    call sicm_get_time(stime)
    bytes = kernel(a, b, c, sz)
    call sicm_get_time(etime)
    
    sf_bandwidth_linear3 = bytes / ((etime%sec - stime%sec) * 1000000 + (etime%nsec - stime%nsec) / 1000);
    
    call sf_free(device, a_, sz * 8_8)
    call sf_free(device, b_, sz * 8_8)
    call sf_free(device, c_, sz * 8_8)
  end function sf_bandwidth_linear3
  
  function sf_bandwidth_random3(device, sz, kernel)
    integer(c_size_t) :: sf_bandwidth_random3
    type(sf_device), intent(in) :: device
    integer(c_size_t), intent(in) :: sz
    integer(c_size_t) :: kernel
    integer(c_size_t) :: bytes, i
    type(sf_time) :: stime, etime
    type(c_ptr) :: a_, b_, c_, indexes_
    double precision, pointer, dimension(:) :: a, b, c
    integer(c_size_t), pointer, dimension(:) :: indexes
    
    a_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(a_, a, shape=[sz])
    b_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(b_, b, shape=[sz])
    c_ = sf_alloc(device, sz * 8_8)
    call c_f_pointer(c_, c, shape=[sz])
    indexes_ = sf_alloc(device, sz * c_size_t)
    call c_f_pointer(indexes_, indexes, shape=[sz])
    
!$omp parallel do
    do i=1,sz
      a(i) = 1
      b(i) = 2
      c(i) = 3
      call sicm_index_hash(i, sz, indexes(i))
    end do
    
    call sicm_get_time(stime)
    bytes = kernel(a, b, c, indexes, sz)
    call sicm_get_time(etime)
    
    sf_bandwidth_random3 = bytes / ((etime%sec - stime%sec) * 1000000 + (etime%nsec - stime%nsec) / 1000);
    
    call sf_free(device, a_, sz * 8_8)
    call sf_free(device, b_, sz * 8_8)
    call sf_free(device, c_, sz * 8_8)
    call sf_free(device, indexes_, sz * c_size_t)
  end function sf_bandwidth_random3
end module sicm_f90
