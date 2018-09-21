#include <stdio.h>
#include <stdlib.h>
#include "sicm_graph.h"
#include "sicm_detect.h"

static const char sicm_detect_usage_str[] = 
  "Usage: program [filename]\n";

int main(int argc, char **argv) {
  sicm_graph *graph;
  char *filename;

  if(argc != 2) {
    fprintf(stderr, sicm_detect_usage_str);
    exit(1);
  }

  filename = argv[1];
  graph = sicm_graph_init();
  sicm_graph_read(filename, graph);
  sicm_graph_fini(graph);
}
