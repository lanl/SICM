#include <sicm_low.h>

struct arena_list{
        int sicm_tag;
        sicm_arena sa;
        struct arena_list * next;
};

extern sicm_device_list sicm_devices;
extern struct arena_list * sicm_arena_list;
extern struct arena_list * sicm_arena;
extern struct arena_list * cur_dram_arena;
extern struct arena_list * cur_nvram_arena;
extern struct arena_list * cur_hbm_arena;

sicm_arena * find_arena();
