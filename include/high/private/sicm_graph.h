#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sicm_queue.h"

#define MAX_TOKEN_LEN 20

enum sicm_node_type {
  SICM_NODE_MEMORY,
  SICM_NODE_COMPUTE,
  SICM_NODE_OTHER,
  SICM_NODE_INVALID
};

typedef struct sicm_edge sicm_edge;
typedef struct sicm_node sicm_node;
struct sicm_edge {
  size_t counterpart; /* The index of the inverse of this edge */
  char derived;
  unsigned long bandwidth, latency;
  size_t node; /* Index into the node array of the node we're pointing to */
};

struct sicm_node {
  char *name;
  int numa;
  unsigned long capacity;
  enum sicm_node_type type;
  size_t num_edges;
  sicm_edge *edges;
};
/* Need a single identifier for tree.h */
typedef sicm_node * sicm_node_ptr;

typedef struct sicm_graph {
  size_t num_nodes;
  sicm_node *nodes;
} sicm_graph;

static inline sicm_graph *sicm_graph_init() {
  sicm_graph *graph;

  graph = malloc(sizeof(sicm_graph));
  graph->num_nodes = 0;
  graph->nodes = NULL;
  return graph;
}

/* Returns a node pointer, given a name */
static inline size_t get_node_from_name(char *node_name, sicm_graph *graph) {
  size_t i;
  sicm_node *node;
  
  for(i = 0; i < graph->num_nodes; i++) {
    node = &graph->nodes[i];
    if(!strcmp(node_name, node->name)) {
      return i;
    }
  }
  fprintf(stderr, "Node '%s' undefined. Aborting.\n", node_name);
  exit(1);
}

static inline void set_node_field(char *field_name, char *field_val, sicm_node *node) {
  unsigned long val;

  /* Make sure nothing's NULL */
  if(!node) {
    fprintf(stderr, "Got a NULL node. Aborting.\n");
    exit(1);
  }
  if(!field_name || !field_val) {
    fprintf(stderr, "Got a NULL field name or value. Aborting.\n");
    exit(1);
  }

  val = strtoul(field_val, NULL, 0);
  if(!strcmp(field_name, "capacity")) {
    node->capacity = val;
  } else if(!strcmp(field_name, "numa")) {
    node->numa = val;
  } else {
    fprintf(stderr, "Invalid node field name: '%s'. Aborting.\n", field_name);
    exit(1);
  }
}

static inline void set_edge_field(char *field_name, char *field_val, sicm_edge *edge) {
  unsigned long val;

  /* Make sure nothing's NULL */
  if(!edge) {
    fprintf(stderr, "Got a NULL edge. Aborting.\n");
    exit(1);
  }
  if(!field_name || !field_val) {
    fprintf(stderr, "Got a NULL field name or value. Aborting.\n");
    exit(1);
  }

  val = strtoul(field_val, NULL, 0);
  if(!strcmp(field_name, "bandwidth")) {
    edge->bandwidth = val;
  } else if(!strcmp(field_name, "latency")) {
    edge->latency = val;
  } else {
    fprintf(stderr, "Invalid edge field name: '%s'. Aborting.\n", field_name);
    exit(1);
  }
}

/* Allocates and returns a pointer to a new edge */
static inline sicm_edge *new_edge(sicm_node *node) {
  sicm_edge *edge;
  node->num_edges++;
  node->edges = realloc(node->edges, sizeof(sicm_edge) * node->num_edges);
  edge = &node->edges[node->num_edges - 1];
  /* Initialize everything */
  edge->counterpart = SIZE_MAX;
  edge->derived = 0;
  edge->bandwidth = 0;
  edge->latency = 0;
  edge->node = SIZE_MAX;
  return edge;
}


/* Finds the (perhaps multiple) shortest paths that go from start to end.
 * "Shortest path" is defined as a path with the fewest number of hops.
 * Uses a BFS implementation that uses a queue. Adds the derived edge to the graph.
 */
static inline void set_derived_edge(sicm_graph *graph, size_t start, size_t end) {
  queue *q;
  sicm_node *node, *counter_node;
  sicm_edge *edge, *counter_edge, *backedge;
  size_t *distances, *backedges, node_index, next_node_index, i,
         bandwidth, latency;

  q = queue_init();
  backedges = (size_t *) calloc(graph->num_nodes, sizeof(size_t));
  distances = (size_t *) malloc(graph->num_nodes * sizeof(size_t));
  /* Since size_t is larger than memset's int */
  for(i = 0; i < graph->num_nodes; i++) {
    distances[i] = SIZE_MAX;
  }

  queue_push(q, start);
  distances[start] = 0;
  while(!queue_empty(q)) {
    node_index = queue_pop(q);
    node = &graph->nodes[node_index];
    for(i = 0; i < node->num_edges; i++) { 
      next_node_index = node->edges[i].node;
      if(distances[next_node_index] != SIZE_MAX) continue;
      distances[next_node_index] = distances[node_index] + 1;
      backedges[next_node_index] = node->edges[i].counterpart;
      queue_push(q, next_node_index);
    }
  }

  /* Traverse the backedges to find the shortest path from end to start */
  /* Figure out the max bandwidth and total latency of that path */
  i = end;
  bandwidth = SIZE_MAX;
  latency = 0;
  while(i != start) {
    backedge = &graph->nodes[i].edges[backedges[i]];
    i = backedge->node;
    latency += backedge->latency;
    if(backedge->bandwidth < bandwidth) {
      bandwidth = backedge->bandwidth;
    }
  }

  /* Now add the edge to the graph with the calculated bandwith and latency */
  node = &graph->nodes[start];
  counter_node = &graph->nodes[end];
  edge = new_edge(node);
  counter_edge = new_edge(counter_node);
  edge->counterpart = counter_node->num_edges - 1;
  counter_edge->counterpart = node->num_edges - 1;
  edge->bandwidth = bandwidth;
  counter_edge->bandwidth = bandwidth;
  edge->latency = latency;
  counter_edge->latency = latency;
  edge->node = end;
  counter_edge->node = start;
  edge->derived = 1;
  counter_edge->derived = 1;
}

static inline void sicm_graph_print(sicm_graph *graph) {
  size_t i, n;
  sicm_node *node;
  sicm_edge *edge;

  if(!graph) {
    fprintf(stderr, "NULL graph given. Aborting.\n");
    exit(1);
  }

  printf("==========\n");
  for(i = 0; i < graph->num_nodes; i++) {
    node = &graph->nodes[i];
    printf("Node %s:\n", node->name);
    printf("  NUMA node: %d\n", node->numa);
    printf("  Capacity: %lu\n", node->capacity);
    printf("  Number of Edges: %zu\n", node->num_edges);
    for(n = 0; n < node->num_edges; n++) {
      edge = &node->edges[n];
      printf("    Edge:\n");
      printf("      To: %s\n", graph->nodes[edge->node].name);
      printf("      Bandwidth: %lu\n", edge->bandwidth);
      printf("      Latency: %lu\n", edge->latency);
    }
  }
  printf("==========\n");
}

/* TODO: Error handling improvement? */
static inline void sicm_graph_read(char *filename, sicm_graph *graph) {
  FILE *file;
  size_t i, j, n, x;
  char found;
  sicm_node *cur_node, *tmp_node;
  sicm_edge *cur_edge, *tmp_edge;
  char *token;
  /* 0 = not in a comment
   * 1 = in a comment, ignore all tokens until end of it
   */
  char comment;
  /* 0 = not in a node statement or definition
   * 1 = got the "node" keyword, read the node type or error
   * 2 = got the node type, read the node name
   * 3 = got the node type and name, get the "{" token or error
   */
  char node;
  /* 0 = not in an edge definition
   * 1 = got the "edge" keyword, read the target of the edge
   * 2 = got the edge target name, get the "{" token or error
   * 3 = in the edge definition, only field definitions valid now
   */
  char edge;
  /* 0 = not in a field definition
   * 1 = got the field name, now read the value
   * Uses field_name to store the field name until we reach the value
   */
  char field;
  char *field_name;

  if(!graph) {
    fprintf(stderr, "Graph hasn't been initialized. Aborting.\n");
    exit(1);
  }

  file = fopen(filename, "r");
  if(!file) {
    fprintf(stderr, "Failed to open the file. Aborting.\n");
    exit(1);
  }

  /* Read each whitespace-delimited string */
  cur_node = NULL;
  cur_edge = NULL;
  comment = 0;
  node = 0;
  edge = 0;
  field = 0;
  token = malloc(sizeof(char) * MAX_TOKEN_LEN);
  while(fscanf(file, "%s", token) != EOF) {
    /* Ignore the line if it's a comment */
    if(!strcmp(token, "/*")) {
      comment = 1;
    } else if(!strcmp(token, "*/")) {
      comment = 0;
      continue;
    }
    if(comment) continue;

    if(node == 4) {
      /* We're in a node definition, only legal statements are edge
       * definitions and field definitions
       */
      if(edge) {
        /* We're in an edge definition. */
        if(edge == 1) {
          /* Get the edge target */
          cur_edge->node = get_node_from_name(token, graph);
          edge = 2;
        } else if(edge == 2) {
          /* Get the start of the edge definition */
          edge = 3;
        } else if(edge == 3 && !strcmp(token, "};")) {
          /* Edge definition ends */
          edge = 0;
        } else if(edge == 3) {
          /* In an edge definition, look for fields now */
          if(field) {
            /* If field, we've already read the field name */
            set_edge_field(field_name, token, cur_edge);
            free(field_name);
            field = 0;
          } else {
            /* Need to read the field name */
            field_name = malloc(sizeof(char) * (strlen(token) + 1));
            strcpy(field_name, token);
            field = 1;
          }
        } else {
          fprintf(stderr, "Impossible situation. Aborting.\n");
          exit(1);
        }
      } else if(!strcmp(token, "edge")) {
        /* Declare a new edge */
        cur_edge = new_edge(cur_node);
        edge = 1;
      } else if(!strcmp(token, "};")) {
        /* End the node definition. Go back to top-level. */
        node = 0;
      } else {
        if(field) {
          /* Already read the field name. Read the value. */
          set_node_field(field_name, token, cur_node);
          free(field_name);
          field = 0;
        } else {
          /* Has to be a field definition then. Store the field name. */
          field_name = malloc(sizeof(char) * (strlen(token) + 1));
          strcpy(field_name, token);
          field = 1;
        }
      }
    } else if(node) {
      /* We're in a node declaration already */
      if(node == 3) {
        /* We've already gotten the node type and name, now for the definition */
        if(!strcmp(token, "{")) {
          node = 4;
        } else {
          fprintf(stderr, "Expected '{' to begin a node definition. Aborting.\n");
          exit(1);
        }
      } else if(node == 2) {
        /* We've already gotten the node type, now the token is the name */
        cur_node->name = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(cur_node->name, token);
        node = 3;
      } else if(node == 1) {
        /* Read the node type */
        if(!strcmp(token, "compute")) {
          cur_node->type = SICM_NODE_COMPUTE;
        } else if(!strcmp(token, "mem")) {
          cur_node->type = SICM_NODE_MEMORY;
        } else {
          fprintf(stderr, "Invalid node type.\n");
          exit(1);
        }
        node = 2;
      } else {
        fprintf(stderr, "Impossible situation. Aborting.\n");
        exit(1);
      }
    } else if(!strcmp(token, "node")) {
      /* Found a node statement, which looks like:
       * node [type] [name]
       * Allocate a new initialized node.
       */
      graph->num_nodes++;
      graph->nodes = realloc(graph->nodes, sizeof(sicm_node) * graph->num_nodes);
      cur_node = &graph->nodes[graph->num_nodes - 1];
      cur_node->name = NULL;
      cur_node->numa = -1;
      cur_node->capacity = 0;
      cur_node->type = SICM_NODE_INVALID;
      cur_node->num_edges = 0;
      cur_node->edges = NULL;
      node = 1; /* The next token will be a node type */
    } else {
      /* Not a node statement, or in a node definition, or a comment. Error. */
      fprintf(stderr, "Unrecognized top-level statement. Aborting.\n");
      exit(1);
    }
  }

  /* Make sure each edge has a counterpart.
   * This *could* be done while reading the file,
   * but for code readability is here. Adjust if 
   * it becomes a performance concern (it probably won't).
   */
  for(i = 0; i < graph->num_nodes; i++) {
    cur_node = &graph->nodes[i];
    for(n = 0; n < cur_node->num_edges; n++) {
      cur_edge = &cur_node->edges[n];

      /* See if we already have a counterpart for this edge by iterating
       * over this edge's node's edges and comparing pointers
       */
      found = 0;
      for(x = 0; x < graph->nodes[cur_edge->node].num_edges; x++) {
        tmp_edge = &graph->nodes[cur_edge->node].edges[x];
        if(tmp_edge->node == i) {
          /* Already have the counterpart to cur_edge, so break */
          found = 1;
          break;
        }
      }
      if(!found) {
        /* If we didn't find the counterpart already, create it */
        tmp_edge = new_edge(&graph->nodes[cur_edge->node]);
        tmp_edge->node = i;
        tmp_edge->bandwidth = cur_edge->bandwidth;
        tmp_edge->latency = cur_edge->latency;
        tmp_edge->derived = cur_edge->derived;
      }
      tmp_edge->counterpart = n;
      cur_edge->counterpart = x;
    }
  }

  /* Now create derived edges, which are defined as edges that aren't in the
   * file, but are required to meet the requirement of having an edge between
   * every compute node and each memory node.
   */
  for(i = 0; i < graph->num_nodes; i++) {
    cur_node = &graph->nodes[i];
    if(cur_node->type == SICM_NODE_COMPUTE) {
      /* If it's a compute node, make sure it's got an edge to
       * every memory node.
       */
      for(x = 0; x < graph->num_nodes; x++) {
        tmp_node = &graph->nodes[x];
        if((tmp_node != cur_node) && (tmp_node->type == SICM_NODE_MEMORY)) {
          /* tmp_node is a memory node, make sure cur_node has an edge to it */
          found = 0;
          for(j = 0; j < cur_node->num_edges; j++) {
            tmp_edge = &cur_node->edges[j];
            if(&graph->nodes[tmp_edge->node] == tmp_node) {
              found = 1;
              break;
            }
          }
          if(!found) {
            /* Didn't find an edge between cur_node and tmp_node */
            set_derived_edge(graph, i, x);
          }
        }
      }
    }
  }

  sicm_graph_print(graph);
  if(token) free(token);

  fclose(file);
}

static inline void sicm_graph_fini(sicm_graph *graph) {
  size_t i;
  sicm_node *node;

  if(graph) {
    if(graph->nodes) {
      for(i = 0; i < graph->num_nodes; i++) {
        node = &graph->nodes[i];
        if(node->name) {
          free(node->name);
        }
        if(node->edges) {
          free(node->edges);
        }
      }
      free(graph->nodes);
    }
    free(graph);
  }
}
