#include <hwloc.h>
#include <stdio.h>
#include <string.h>

int find_CUDA(hwloc_obj_t node,
    int *numa_node,
    char *pciBusId, size_t pciBusIdSize) {
    if (!numa_node || !pciBusId || !pciBusIdSize) {
        return -1;
    }

    hwloc_obj_t pci = NULL;
    while (node && node->type != HWLOC_OBJ_PCI_DEVICE) {
        node = node->parent;
    }
    pci = node;

    hwloc_obj_t numa = NULL;
    while (node && node->type != HWLOC_OBJ_PACKAGE) {
        node = node->parent;
    }
    numa = node;

    if (!pci || !numa) {
        return -1;
    }

    *numa_node = numa->logical_index;
    snprintf(pciBusId, pciBusIdSize, "%04x:%02x:%02x.%01x",
        pci->attr->pcidev.domain,
        pci->attr->pcidev.bus,
        pci->attr->pcidev.dev,
        pci->attr->pcidev.func);

    return 0;
}

struct pci_device {
    char *name;
    int numa;
    char pci[100];
};

int main(void) {
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);  // initialization
    hwloc_topology_set_io_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(topology);   // actual detection

    int obj_avail = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_OS_DEVICE);
    struct pci_device *gpus = malloc(sizeof(struct pci_device) * obj_avail);
    int gpu_count = 0;
    for(int i = 0; i < obj_avail; i++) {
        hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_OS_DEVICE, i);
        if (obj->subtype && (strncmp(obj->subtype, "CUDA", 4) == 0)) {
            struct pci_device *gpu = &gpus[gpu_count];
            if (find_CUDA(obj, &gpu->numa, gpu->pci, sizeof(gpu->pci)) == 0) {
                gpu->name = obj->name;
                gpu_count++;
            }
        }
    }

    for(int i = 0; i < gpu_count; i++) {
        printf("%d %s\n", gpus[i].numa, gpus[i].name);
    }

    free(gpus);

    hwloc_topology_destroy(topology);

    return 0;
}
