/*
 * Contains code for parsing output of the high-level interface.
 * Reads information about each allocation site into a structure,
 * as well as information that tools like /usr/bin/time output.
 */

#pragma once

#include <inttypes.h>
#include <limits.h>
#include "sicm_tree.h"

/* For parsing information about sites */
typedef struct site {
	float bandwidth;
	uintmax_t peak_rss, accesses;
} site;
typedef site * siteptr;
use_tree(unsigned, siteptr);
typedef struct app_info {
	uintmax_t peak_rss, site_peak_rss, time, max_time, min_time, num_times,
						num_pebs_sites, num_mbi_sites;
	tree(unsigned, siteptr) sites;
} app_info;

/* Reads in profiling information from the file pointer, returns
 * a tree containing all sites and their bandwidth, peak RSS,
 * and number of accesses (if applicable).
 */
static inline app_info *sh_parse_site_info(FILE *file) {
	char *line, *tok, *ptr;
	ssize_t len, read;
	size_t total_time, tmp_time;
	long long num_sites, node;
	siteptr cur_site;
	int mbi, pebs, pebs_site, i, hours, minutes;
	float bandwidth, seconds;
	tree_it(unsigned, siteptr) it;
	app_info *info;

	info = malloc(sizeof(app_info));
	info->sites = tree_make(unsigned, siteptr);
	info->peak_rss = 0;
	info->time = 0;
	info->num_times = 0;
	info->max_time = 0;
	info->min_time = ULONG_MAX;
	info->num_pebs_sites = 0;
	info->num_mbi_sites = 0;

	if(!file) {
		fprintf(stderr, "Invalid file pointer. Aborting.\n");
		exit(1);
	}

	/* Read in from stdin and fill in the tree with the sites */
	num_sites = 0;
	mbi = 0;
	pebs = 0;
	pebs_site = 0;
	line = NULL;
	len = 0;
	total_time = 0;
	while(read = getline(&line, &len, file) != -1) {
		tok = strtok(line, " \t");
		if(!tok) break;

		/* Find the beginning or end of some results */
		if(strcmp(tok, "=====") == 0) {
			/* Get whether it's the end of results, or MBI, or PEBS */
			tok = strtok(NULL, " ");
			if(!tok) break;
			if(strcmp(tok, "MBI") == 0) {
				/* Need to keep parsing to get the site number */
				tok = strtok(NULL, " ");
				if(!tok || strcmp(tok, "RESULTS") != 0) {
					fprintf(stderr, "Error parsing.\n");
					exit(1);
				}
				tok = strtok(NULL, " ");
				if(!tok || strcmp(tok, "FOR") != 0) {
					fprintf(stderr, "Error parsing.\n");
					exit(1);
				}
				tok = strtok(NULL, " ");
				if(!tok || strcmp(tok, "SITE") != 0) {
					fprintf(stderr, "Error parsing.\n");
					exit(1);
				}
				tok = strtok(NULL, " ");
				if(!tok) break;
				mbi = strtoimax(tok, NULL, 10);
				it = tree_lookup(info->sites, mbi);
				if(tree_it_good(it)) {
					cur_site = tree_it_val(it);
				} else {
					cur_site = malloc(sizeof(site));
					cur_site->bandwidth = 0;
					cur_site->peak_rss = 0;
					cur_site->accesses = 0;
					tree_insert(info->sites, mbi, cur_site);
					info->num_mbi_sites++;
				}
			} else if(strcmp(tok, "PEBS") == 0) {
				pebs = 1;
				continue; /* Don't need the rest of this line */
			} else if(strcmp(tok, "END") == 0) {
				mbi = 0;
				pebs = 0;
				continue;
			} else {
				fprintf(stderr, "Found '=====', but no descriptor. Aborting.\n");
				fprintf(stderr, "%s\n", line);
				exit(1);
			}
			len = 0;
			free(line);
			line = NULL;
			continue;
		}

		/* If we're already in a block of results */
		if(mbi) {
			/* We've already gotten the first token, use it */
			if(tok && strcmp(tok, "Average") == 0) {
				tok = strtok(NULL, " ");
				if(tok && strcmp(tok, "bandwidth:") == 0) {
					/* Get the average bandwidth value for this site */
					tok = strtok(NULL, " ");
					bandwidth = strtof(tok, NULL);
					cur_site->bandwidth = bandwidth;
				} else {
					fprintf(stderr, "Got 'Average', but no expected tokens. Aborting.\n");
					exit(1);
				}
			} else {
				fprintf(stderr, "In a block of MBI results, but no expected tokens.\n");
				exit(1);
			}
			continue;
		} else if(pebs) {
			/* We're in a block of PEBS results */
			if(tok && (strcmp(tok, "Site") == 0)) {
				/* Get the site number */
				tok = strtok(NULL, " ");
				if(tok) {
					/* Get the site number, then wait for the next line */
					pebs_site = strtoimax(tok, NULL, 10);
					it = tree_lookup(info->sites, pebs_site);
					if(tree_it_good(it)) {
						cur_site = tree_it_val(it);
					} else {
						cur_site = malloc(sizeof(site));
						cur_site->bandwidth = 0;
						cur_site->peak_rss = 0;
						cur_site->accesses = 0;
						tree_insert(info->sites, pebs_site, cur_site);
						info->num_pebs_sites++;
					}
				} else {
					fprintf(stderr, "Got 'Site' but no expected site number. Aborting.\n");
					exit(1);
				}
			} else if(tok && (strcmp(tok, "Totals:") == 0)) {
				/* Ignore the totals */
				continue;
			} else {
				/* Get some information about a site */
				if(tok && (strcmp(tok, "Accesses:") == 0)) {
					tok = strtok(NULL, " ");
					if(!tok) {
						fprintf(stderr, "Got 'Accesses:' but no value. Aborting.\n");
						exit(1);
					}
					cur_site->accesses = strtoimax(tok, NULL, 10);
				} else if(tok && (strcmp(tok, "Peak") == 0)) {
					tok = strtok(NULL, " ");
					if(tok && (strcmp(tok, "RSS:") == 0)) {
						tok = strtok(NULL, " ");
						if(!tok) {
							fprintf(stderr, "Got 'Peak RSS:' but no value. Aborting.\n");
							exit(1);
						}
						cur_site->peak_rss = strtoumax(tok, NULL, 10);
					} else {
						fprintf(stderr, "Got 'Peak' but not 'RSS:'. Aborting.\n");
						exit(1);
					}
				} else {
					fprintf(stderr, "Got a site number but no expected information. Aborting.\n");
					exit(1);
				}
			}
		} else {
			/* Parse the output of /usr/bin/time to get peak RSS */
			if(tok && strcmp(tok, "Maximum") == 0) {
				tok = strtok(NULL, " ");
				if(!tok || !(strcmp(tok, "resident") == 0)) {
					continue;
				}
				tok = strtok(NULL, " ");
				if(!tok || !(strcmp(tok, "set") == 0)) {
					continue;
				}
				tok = strtok(NULL, " ");
				if(!tok || !(strcmp(tok, "size") == 0)) {
					continue;
				}
				tok = strtok(NULL, " ");
				if(!tok || !(strcmp(tok, "(kbytes):") == 0)) {
					continue;
				}
				tok = strtok(NULL, " ");
				if(!tok) {
					continue;
				}
				info->peak_rss = strtoimax(tok, NULL, 10) * 1024; /* it's in kilobytes */
			} else if(tok && (strcmp(tok, "Elapsed") == 0)) {
				/* Skip the next six tokens to get the elapsed wall clock time */
				tok = strtok(NULL, " ");
				if(!tok || !(strcmp(tok, "(wall") == 0)) {
					continue;
				}
				tok = strtok(NULL, " ");
				tok = strtok(NULL, " ");
				tok = strtok(NULL, " ");
				tok = strtok(NULL, " ");
				tok = strtok(NULL, " ");
				tok = strtok(NULL, " ");
				if(!tok) {
					continue;
				}
				/* Now this string is in hh:mm:ss or m:ss. We want seconds. */
				/* Count the number of colons. */
				i = 0;
				ptr = tok;
				while((ptr = strchr(ptr, ':')) != NULL) {
					i++;
					ptr++;
				}
				if(i == 2) {
					/* Get hours, then minutes, then seconds. */
					hours = 0;
					minutes = 0;
					seconds = 0;
					sscanf(tok, "%d:%d:%f", &hours, &minutes, &seconds);
				} else if(i == 1) {
					/* Minutes, then seconds. */
					hours = 0;
					minutes = 0;
					seconds = 0;
					sscanf(tok, "%d:%f", &minutes, &seconds);
				} else {
					fprintf(stderr, "Got a GNU time wall clock time that was unfamiliar. Aborting.\n");
					exit(1);
				}
				info->num_times++;
				tmp_time = ((hours * 60 * 60) + (minutes * 60) + ((int) seconds));
				total_time += tmp_time;
				info->time = total_time / info->num_times;
				if(tmp_time > info->max_time) {
					info->max_time = tmp_time;
				}
				if(tmp_time < info->min_time) {
					info->min_time = tmp_time;
				}
			}
		}
	}
	free(line);

	info->site_peak_rss = 0;
	tree_traverse(info->sites, it) {
		info->site_peak_rss += tree_it_val(it)->peak_rss;
	}

	return info;
}
