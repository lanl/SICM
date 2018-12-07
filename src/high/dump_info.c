#include <stdio.h>
#include "sicm_parsing.h"

int main() {
  app_info *info;

  info = sh_parse_site_info(stdin);
  printf("Peak RSS: %zu\n", info->peak_rss);
  info->peak_rss = 0;
  printf("Peak RSS of Sites: %zu\n", sh_get_peak_rss(info));
  printf("Runtime: %zu\n", info->time);
}
