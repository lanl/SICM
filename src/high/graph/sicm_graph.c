#include <stdio.h>
#include <stdlib.h>
#include "sicm_graph.h"

static char sh_graph_initialized = 0;
static sicm_graph *graph = NULL;

/* Reads in and initializes the graph. Simply returns silently if it fails,
 * so that the graph file is optional.
 */
__attribute__((constructor))
void sh_graph_init() {
  char *filename;

  if(sh_graph_initialized) {
    return;
  }

  /* Get graph filename from environment variable */
  filename = getenv("SH_GRAPH_FILE");
  if(!filename) {
    goto finish;
  }
  
  graph = sicm_graph_init();
  sicm_graph_read(filename, graph);

finish:
  sh_graph_initialized = 1;
  return;
}

__attribute__((destructor))
void sh_graph_terminate() {
  sicm_graph_fini(graph);
}
