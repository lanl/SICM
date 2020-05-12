#include "sicm_cpp.hpp"
#include <iostream>
#include <vector>

int main() {
  struct sicm_device_list devices = sicm_init_cpp();
  struct sicm_device* device = &devices.devices[1];
  std::cout << sicm_avail(device) << std::endl;
  sicm_stl(device, std::vector, int, v)
  for(int i = 0; i < 100; i++) v.push_back(i);
  std::cout << sicm_avail(device) << std::endl;
}
