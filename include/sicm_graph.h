#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  unsigned long bandwidth, latency;
  sicm_node *node; /* Pointer to the node */
};

struct sicm_node {
  char *name;
  int numa;
  unsigned long capacity;
  enum sicm_node_type type;
  size_t num_edges;
  sicm_edge *edges;
};

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
static inline sicm_node *get_node_from_name(char *node_name, sicm_graph *graph) {
  size_t i;
  sicm_node *node;
  
  for(i = 0; i < graph->num_nodes; i++) {
    node = &graph->nodes[i];
    if(!strcmp(node_name, node->name)) {
      printf("Found the node: %s\n", node_name);
      return node;
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
      printf("      Bandwidth: %lu\n", edge->bandwidth);
      printf("      Latency: %lu\n", edge->latency);
    }
  }
  printf("==========\n");
}

static inline void sicm_graph_read(char *filename, sicm_graph *graph) {
  FILE *file;
  size_t i, n;
  sicm_node *cur_node;
  sicm_edge *cur_edge;
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
        if(edge == 1) {
          /* Get the edge target */
          printf("Making a new edge to node '%s'.\n", token);
          cur_edge->node = get_node_from_name(token, graph);
          edge = 2;
        } else if(edge == 2) {
          /* Get the start of the edge definition */
          printf("Starting an edge definition\n");
          edge = 3;
        } else if(edge == 3 && !strcmp(token, "};")) {
          /* Edge definition ends */
          printf("Ending edge definition.\n");
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
        printf("Making a new edge\n");
        cur_node->num_edges++;
        cur_node->edges = realloc(cur_node->edges, sizeof(sicm_edge) * cur_node->num_edges);
        cur_edge = &cur_node->edges[cur_node->num_edges - 1];
        edge = 1;
      } else if(!strcmp(token, "};")) {
        /* End the node definition. Go back to top-level. */
        printf("Ending the node definition.\n");
        node = 0;
      } else {
        if(field) {
          /* Already read the field name. Read the value. */
          set_node_field(field_name, token, cur_node);
          free(field_name);
          field = 0;
        } else {
          /* Has to be a field definition then. Store the field name. */
          printf("Storing field name: %s\n", token);
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
          printf("Starting definition of node.\n");
          node = 4;
        } else {
          fprintf(stderr, "Expected '{' to begin a node definition. Aborting.\n");
          exit(1);
        }
      } else if(node == 2) {
        /* We've already gotten the node type, now the token is the name */
        printf("Creating a node with the name '%s'.\n", token);
        cur_node->name = malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(cur_node->name, token);
        node = 3;
      } else if(node == 1) {
        /* Read the node type */
        if(!strcmp(token, "compute")) {
          cur_node->type = SICM_NODE_COMPUTE;
          printf("Creating a compute node.\n");
        } else if(!strcmp(token, "mem")) {
          cur_node->type = SICM_NODE_MEMORY;
          printf("Creating a memory node.\n");
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
      printf("Creating a node.\n");
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
      printf("%s -> %s\n", cur_node->name, cur_edge->node->name);
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
