#include <stdio.h>
#include <sicm_high.h>

sicm_device_list sicm_devices;
struct arena_list * sicm_arena_list;
struct arena_list * cur_arena = NULL;
struct arena_list * cur_dram_arena;
struct arena_list * cur_nvram_arena;
struct arena_list * cur_pmem_arena;

struct arena_list * find_arena(int arena_tag){
        struct arena_list * head = sicm_arena_list;
        while(head != NULL){
                if(head->sicm_tag == arena_tag){
                        return head;
                }
                head = head->next;
        }
        return NULL;
}

void * sicm_allocate(char * alloc_type, int dev, size_t s, int arena_tag){
	
	struct arena_list * alloc_arena = find_arena(arena_tag);
        if(alloc_arena == NULL){
                struct arena_list new_arena;
                new_arena.tag = arena_tag;
                new_arena.sa = sicm_arena_create(0, &sicm_devices.devices[dev]);
                if(cur_arena == NULL){
                        cur_arena = new_arena;
                }else{
                        cur_arena->next = new_arena;
                        cur_arena = cur_arena->next;
                }
                return sicm_arena_alloc(new_arena.sa, s);
        }
        else{
                return sicm_arena_alloc(alloc_arena.sa, s);
        }
}

int main() {
        int i;
        char *buf1, *buf2;

        sicm_devices = sicm_init();
        for(i = 0; i < sicm_devices.count; i++) {
                printf("%d %d\n", i, sicm_numa_id(&sicm_devices.devices[i]));
        }
	if (s == NULL) {
                fprintf(stderr, "sicm_arena_create failed\n");
                return -1;
        }

        buf1 = sicm_arena_alloc(s, 1024);
        buf2 = sicm_arena_alloc(s, 2048*1024);
        sicm_free(buf1);
        sicm_free(buf2);
        return 0;
}
