#include "rbtree.h"
#include <stdio.h>

struct sicm_node_t sicm_leaf = {
  .parent = &sicm_leaf,
  .children = {&sicm_leaf, &sicm_leaf},
  .start = NULL,
  .end = NULL
};

// Helper functions to find relatives of a node.
struct sicm_node_t* grandparent(struct sicm_node_t* node) {
  if(node == &sicm_leaf || node->parent == &sicm_leaf) return &sicm_leaf;
  return node->parent->parent;
}

struct sicm_node_t* sibling(struct sicm_node_t* node) {
  if(node == &sicm_leaf || node->parent == &sicm_leaf) return &sicm_leaf;
  if(node->parent->children[0] == node) return node->parent->children[1];
  return node->parent->children[0];
}

struct sicm_node_t* uncle(struct sicm_node_t* node) {
  if(grandparent(node) == &sicm_leaf) return &sicm_leaf;
  return sibling(node->parent);
}

/*
 * Combined rotation function, slightly abstract because of the direction
 * argument. Remember that !nonzero is zero, and !zero is one.
 */
enum direction_t {LEFT, RIGHT};
void rotate(struct sicm_tree_t* tree, struct sicm_node_t* x, enum direction_t direction) {
  struct sicm_node_t* y = x->children[!direction];
  x->children[!direction] = y->children[direction];
  if(y->children[direction] != &sicm_leaf)
    y->children[direction]->parent = x;
  y->parent = x->parent;
  if(x->parent == &sicm_leaf)
    tree->root = y;
  else if(x == x->parent->children[direction])
    x->parent->children[direction] = y;
  else
    x->parent->children[!direction] = y;
  y->children[direction] = x;
  x->parent = y;
}

void sicm_insert_one(struct sicm_tree_t* tree, void* start, void* end) {
  struct sicm_node_t* z = malloc(sizeof(struct sicm_node_t));
  z->start = start;
  z->end = end;
  struct sicm_node_t* y = &sicm_leaf;
  struct sicm_node_t* x = tree->root;
  while(x != &sicm_leaf) {
    y = x;
    if(z->start < x->start) x = x->children[0];
    else x = x->children[1];
  }
  z->parent = y;
  if(y == &sicm_leaf) tree->root = z;
  else if(z->start < y->start) y->children[0] = z;
  else y->children[1] = z;
  z->children[0] = &sicm_leaf;
  z->children[1] = &sicm_leaf;
  SICM_SET_RED(z);

  // rb-insert-fixup
  while(SICM_IS_RED(z->parent)) {
    int d = z->parent == z->parent->parent->children[1];
    y = z->parent->parent->children[!d];
    if(SICM_IS_RED(y)) {
      SICM_SET_BLACK(z->parent);
      SICM_SET_BLACK(y);
      SICM_SET_RED(z->parent->parent);
      z = z->parent->parent;
    }
    else {
      if(z == z->parent->children[!d]) {
        z = z->parent;
        rotate(tree, z, d);
      }
      SICM_SET_BLACK(z->parent);
      SICM_SET_RED(z->parent->parent);
      rotate(tree, z->parent->parent, !d);
    }
  }
  SICM_SET_BLACK(tree->root);
}

void sicm_delete_one(struct sicm_tree_t* tree, struct sicm_node_t* z) {
  struct sicm_node_t* w = &sicm_leaf;
  struct sicm_node_t* x = &sicm_leaf;
  struct sicm_node_t* y = &sicm_leaf;
  if(z->children[0] == &sicm_leaf || z->children[1] == &sicm_leaf)
    y = z;
  else {
    // This is the successor algorithm; it finds the next node to the right,
    // i.e., the next address.
    if(z->children[1] != &sicm_leaf) {
      y = z->children[1];
      while(y->children[0] != &sicm_leaf) y = y->children[0];
    }
    else {
      x = z;
      y = z->parent;
      while(y != &sicm_leaf && x == y->children[1]) {
        x = y;
        y = y->parent;
      }
    }
  }
  x = y->children[0] != &sicm_leaf ? y->children[0] : y->children[1];
  x->parent = y->parent;
  if(y->parent == &sicm_leaf)
    tree->root = x;
  else if(y == y->parent->children[0])
    y->parent->children[0] = x;
  else
    y->parent->children[1] = x;
  if(y != z) {
    // We need to preserve the color of z. There's a variety of ways of skinning
    // that cat---we could store it in another variable, for example---but this
    // works too. This is a departure from the normal red-black algorithm,
    // because the key (start) is encoding the color.
    if(SICM_IS_BLACK(z)) {
      z->start = y->start;
      SICM_SET_BLACK(z);
    }
    else {
      z->start = y->start;
      SICM_SET_RED(z);
    }
    z->end = y->end;
  }
  if(SICM_IS_BLACK(y)) {
    while(x != tree->root && SICM_IS_BLACK(x)) {
      // If x is the left child of its parent, then a is left, b is right, and
      // vice versa. The "delete fixup" algorithm has symmetric cases. Remember
      // that rotate takes an argument specifying the direction of the rotation,
      // 0 for left and 1 for right. For the symmetric cases for delete fixup,
      // this is the same as a.
      int a = (x == x->parent->children[1]);
      int b = !a;
      w = x->parent->children[b];
      if(SICM_IS_RED(w)) {
        SICM_SET_BLACK(w);
        SICM_SET_RED(x->parent);
        rotate(tree, x->parent, a);
        w = x->parent->children[b];
      }
      if(SICM_IS_BLACK(w->children[0]) && SICM_IS_BLACK(w->children[1])) {
        SICM_SET_RED(w);
        x = x->parent;
      }
      else {
        if(SICM_IS_BLACK(w->children[b])) {
          SICM_SET_BLACK(w->children[a]);
          SICM_SET_RED(w);
          rotate(tree, w, b);
          w = x->parent->children[b];
        }
        SICM_TRANSFER_COLOR(w, x->parent);
        SICM_SET_BLACK(x->parent);
        SICM_SET_BLACK(w->children[b]);
        rotate(tree, x->parent, a);
        x = tree->root;
      }
    }
    SICM_SET_BLACK(x);
  }
  free(y);
}

void sicm_insert(struct sicm_tree_t* tree, void* start, void* end) {
  struct sicm_node_t* node = tree->root;
  void* nodestart;
  while(1) {
    nodestart = SICM_UNPACK_START(node);
    if(node == &sicm_leaf) {
      sicm_insert_one(tree, start, end);
      break;
    }
    else if((start >= nodestart && start <= node->end) || (end >= nodestart && end <= node->end)) {
      start = start < nodestart ? start : node->start;
      end = end > node->end ? end : node->end;
      sicm_delete_one(tree, node);
      node = tree->root;
    }
    else if(end < nodestart)
      node = node->children[0];
    else if(start > node->end)
      node = node->children[1];
    else
      printf("invalid case in sicm_insert\n");
  }
}

void sicm_delete(struct sicm_tree_t* tree, void* start, void* end) {
  struct sicm_node_t* node = tree->root;
  void *a, *b;
  void* nodestart;
  while(node != &sicm_leaf) {
    nodestart = SICM_UNPACK_START(node);
    if(start == nodestart && end == node->end) {
      sicm_delete_one(tree, node);
      break;
    }
    else if(end < nodestart)
      node = node->children[0];
    else if(start > node->end)
      node = node->children[1];
    // From this point on there must be an overlap, so something needs to happen
    // Conditions are written out fully for the sake of clarity
    else if(start < nodestart && end < node->end) {
      a = nodestart;
      b = node->end;
      sicm_delete_one(tree, node);
      sicm_insert(tree, end, b);
      end = a;
      node = tree->root;
    }
    else if(start < nodestart && end == node->end) {
      end = nodestart;
      sicm_delete_one(tree, node);
      node = tree->root;
    }
    else if(start < nodestart && end > node->end) {
      a = nodestart;
      b = node->end;
      sicm_delete_one(tree, node);
      sicm_delete(tree, b, end);
      end = a;
      node = tree->root;
    }
    else if(start == nodestart && end < node->end) {
      b = node->end;
      sicm_delete_one(tree, node);
      sicm_insert(tree, end, b);
      break;
    }
    else if(start == nodestart && end > node->end) {
      start = node->end;
      sicm_delete_one(tree, node);
      node = tree->root;
    }
    else if(start > nodestart && end < node->end) {
      b = node->end;
      node->end = start;
      sicm_insert(tree, end, b);
      break;
    }
    else if(start > nodestart && end == node->end) {
      node->end = start;
      break;
    }
    else if(start > nodestart && end > node->end) {
      b = node->end;
      node->end = start;
      start = b;
      node = tree->root;
    }
    else {
      printf("invalid case in sicm_delete: %lu %lu %lu %lu\n", (size_t)start, (size_t)end, (size_t)nodestart, (size_t)node->end);
    }
  }
}

struct sicm_tree_t* sicm_create_tree() {
  struct sicm_tree_t* tree = malloc(sizeof(struct sicm_tree_t));
  tree->root = &sicm_leaf;
  return tree;
}

void sicm_destroy_one(struct sicm_node_t* node) {
  if(node->children[0] != &sicm_leaf) sicm_destroy_one(node->children[0]);
  if(node->children[1] != &sicm_leaf) sicm_destroy_one(node->children[1]);
  free(node);
}

void sicm_destroy_tree(struct sicm_tree_t* tree) {
  if(tree->root != &sicm_leaf) sicm_destroy_one(tree->root);
  free(tree);
}

void sicm_traverse_one(struct sicm_node_t* node, void* aux, void (*f)(void* aux, void* start, void* end)) {
  if(node->children[0] != &sicm_leaf) sicm_traverse_one(node->children[0], aux, f);
  f(aux, SICM_UNPACK_START(node), node->end);
  if(node->children[1] != &sicm_leaf) sicm_traverse_one(node->children[1], aux, f);
}

void sicm_map_tree(struct sicm_tree_t* tree, void* aux, void (*f)(void* aux, void* start, void* end)) {
  if(tree->root != &sicm_leaf) sicm_traverse_one(tree->root, aux, f);
}
