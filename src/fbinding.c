#include "sicm_low.h"

struct sicm_fortran_device {
  void* device;
};

struct sicm_fortran_device_list {
  int count;
  struct sicm_device** devices;
};

void sicm_init_wrap_(struct sicm_fortran_device_list* devices_) {
  struct sicm_device_list devices = sicm_init();
  devices_->count = devices.count;
  devices_->devices = malloc(devices.count * sizeof(struct sicm_device*));
  int i;
  for(i = 0; i < devices.count; i++)
    devices_->devices[i] = &(devices.devices[i]);
}

void sicm_get_device_wrap_(struct sicm_fortran_device_list* devices_, int* i, struct sicm_fortran_device* device) {
  device->device = devices_->devices[*i];
}

void sicm_alloc_wrap_(struct sicm_fortran_device* device, size_t* size, void** ptr) {
  *ptr = sicm_alloc(device->device, *size);
}

void sicm_move_wrap_(struct sicm_fortran_device* src, struct sicm_fortran_device* dst, void** ptr, size_t* size, int* res) {
  *res = sicm_move(src->device, dst->device, *ptr, *size);
}

void sicm_capacity_wrap_(struct sicm_fortran_device* device, size_t* res) {
  *res = sicm_capacity(device->device);
}

void sicm_used_wrap_(struct sicm_fortran_device* device, size_t* res) {
  *res = sicm_used(device->device);
}

void sicm_model_distance_wrap_(struct sicm_fortran_device* device, int* res) {
  #ifdef _GNU_SOURCE
  *res = sicm_model_distance(device->device);
  #else
  *res = -1;
  #endif
}

void sicm_latency_wrap_(struct sicm_fortran_device* device, size_t* size, int* iter, struct sicm_timing* res) {
  sicm_latency(device->device, *size, *iter, res);
}
