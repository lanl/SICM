#include <stdlib.h>

/*
 * Note--in the interests of space efficiency, the color of a tree node will be
 * packed as the least-significant bit of the start field. Since this tree only
 * tracks pages, the eleven least-significant bits of start are unused (assuming
 * 4K pages--so all values of start are divisible by 4096). This also means that
 * queries may have to use the SICM_UNPACK_START macro, which simply masks out
 * the color bit.
 *
 * If color were an explicit value, it would get padded out to the size of a
 * pointer (8 bytes, with current architectures). Since there could potentially
 * be a lot of nodes in this tree, this saving might not be trivial.
 *
 * The children are encoded as an array so that symmetric cases can be done with
 * index assignment.
 */
struct sicm_node_t {
  struct sicm_node_t* parent;
  struct sicm_node_t* children[2];
  void* start;
  void* end;
};

/*
 * Simple data structure to make it easier to pass trees to functions.
 */
struct sicm_tree_t {
  struct sicm_node_t* root;
};

/*
 * Based on the algorithm shown in Cormen et al., an explicit singleton leaf is
 * used to aid with bookkeeping in the delete function. While the leaf is
 * initialized with self-references for its parent and children, these should
 * not be assumed stable; this node is, in effect, a scratchpad.
 * (But this is typical stuff for red-black trees.)
 */
struct sicm_node_t sicm_leaf;

/*
 * We define black as 0 and red as 1 (in the least-significant position).
 * Therefore, we mask out color with a bitwise-and with the complement of 1,
 * i.e., 111...110. For a packed value, a bitwise-and with 1 gets the color,
 * either 0 (black) or 1 (red). Setting a node to black is therefore the same as
 * removing the color bit, while coloring a node red is a bitwise-or with 1.
 *
 * The SET macros do an in-place assignment, so these are statements. The
 * semicolon is excluded in the macros, because they should appear at the
 * invocation of the macro.
 *
 * SICM_TRANSFER_COLOR assigns the color of y to x.
 */
#define SICM_UNPACK_START(x) ((void*)((size_t)(x->start) & ~1))
#define SICM_IS_BLACK(x) (!((size_t)(x->start) & 1))
#define SICM_IS_RED(x) ((size_t)(x->start) & 1)
#define SICM_SET_BLACK(x) (x->start = (void*)((size_t)(x->start) & ~1))
#define SICM_SET_RED(x) (x->start = (void*)((size_t)(x->start) | 1))
#define SICM_TRANSFER_COLOR(x, y) \
  (x->start = (void*)(((size_t)(x->start) & ~1) ^ ((size_t)(y->start) & 1)))

void sicm_insert(struct sicm_tree_t* tree, void* start, void* end);
void sicm_delete(struct sicm_tree_t* tree, void* start, void* end);

struct sicm_tree_t* sicm_create_tree();
void sicm_destroy_tree(struct sicm_tree_t* tree);

void sicm_map_tree(struct sicm_tree_t* tree, void* aux, void (*f)(void *aux, void* start, void* end));
