#include "sg.hpp"
#include <iostream>
#include <vector>

class GreedyTest : public SGPerf {
public:
  GreedyTest();
  int data[1048576];
};

GreedyTest::GreedyTest() {
  for(int i = 0; i < 1048576; i++) {
    this->data[i] = i;
  }
}

void status() {
  std::cout << "Memory status:" << std::endl;
  for(size_t i = 0; i < sg_performance_list.count; i++) {
    struct sicm_device* device = &sg_performance_list.devices[i];
    std::cout << sicm_numa_id(device) << ": " << sicm_avail(device) << std::endl;
  }
  std::cout << std::endl;
}

int main() {
  sg_init(0);
  status();
  GreedyTest* test = new GreedyTest();
  status();
  delete test;
  status();

  std::vector<int, SGPerfAllocator<int> > data;
  for(int i = 0; i < 1048576; i++) {
    data.push_back(i);
  }
  status();
}
