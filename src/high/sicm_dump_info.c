#include <stdio.h>
#include "sicm_parsing.h"

int main() {
	app_info *info;

	info = sh_parse_site_info(stdin);
	printf("Peak RSS: %zu\n", info->peak_rss);
	printf("Peak RSS of Sites: %zu\n", info->site_peak_rss);
	printf("Runtime: %zu (%c%c%zu)\n", info->time, '\xc2', '\xb1', info->max_time - info->min_time);
	printf("Number of iterations: %zu\n", info->num_times);
	printf("Number of PEBS sites: %zu\n", info->num_pebs_sites);
	printf("Number of MBI sites: %zu\n", info->num_mbi_sites);
}
