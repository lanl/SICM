#include "sicm_cpp.hpp"
#include <iostream>

class SicmTest: public SicmClass {
  public:
    SicmTest(int a, int b, int c);
    ~SicmTest();
    int a, b, c;
};

SicmTest::SicmTest(int a, int b, int c) {
  this->a = a;
  this->b = b;
  this->c = c;
}

SicmTest::~SicmTest() { }

int main() {
  struct sicm_device_list devices = sicm_init_cpp();
  struct sicm_device* device = &devices.devices[1];
  std::cout << sicm_avail(device) << std::endl;
  sicm_new(device, SicmTest, testObj, (1, 2, 3))
  std::cout << sicm_avail(device) << std::endl;
  std::cout << testObj->a << ", " << testObj->b << ", " << testObj->c << std::endl;
  SicmTest* testObj2 = new SicmTest(4, 5, 6);
  std::cout << testObj2->a << ", " << testObj2->b << ", " << testObj2->c << std::endl;
  std::cout << sicm_avail(device) << std::endl;
  sicm_delete(testObj)
  std::cout << sicm_avail(device) << std::endl;
  delete testObj2;
  std::cout << sicm_avail(device) << std::endl;
}
