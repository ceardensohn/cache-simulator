/*
 * csim.c
 *
 * Simulate a cache that records and prints the hits misses and evictions for
 * various traces.
 *
 * This file is a part of COMP280, Project #5
 * 
 * Authors:
 * 1. Chris Eardensohn (ceardensohn@sandiego.edu)
 * 2. Carolina Canales (ccanalesvillarreal@sandiego.edu)
 *
 *
 */
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "cachelab.h"
#include <math.h>

typedef unsigned long int mem_addr;
// line to store bit data into a line struct
struct Line {
	unsigned int valid;
	unsigned int tag;
	unsigned int LRU;
};
typedef struct Line Line;

//struct set to store lines
struct Set {
	int num_lines;
	Line *lines;
};
typedef struct Set Set;

//struct cache to hold sets
struct Cache {
	int num_sets;
	Set *sets;
};
typedef struct Cache Cache;

// forward declaration
void simulateCache(char *trace_file, int num_sets, int E, int s, int b,  int verbose);
void compCache(Cache cache, mem_addr *set_bit, mem_addr *tag_bit, int *miss_count,
	   	int *hit_count, int *eviction_count);
void updateLRU(Cache cache, mem_addr *set_bit, int i);

/**
 * Prints out a reminder of how to run the program.
 U(Cache cache, mem_adr *set_bit, int i){
 * @param executable_name Strign containing the name of the executable.
 */
void usage(char *executable_name) {
	printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>", executable_name);
}

int main(int argc, char *argv[]) {

	int verbose_mode = 0;
	int s;
	int num_sets = 2;	
	int E;
	int b;
	extern char *optarg;
	char *trace_filename = NULL;
	
	opterr = 0;

	int c = -1;

	// Note: adding a colon after the letter states that this option should be
	// followed by an additional value (e.g. "-s 1")
	while ((c = getopt(argc, argv, "v:s:E:b:t:")) != -1) {
		switch (c) {
			case 'v':
				// enable verbose mode
				verbose_mode = 1;
				break;
			case 's':
				// specify the number of sets
				// Note: optarg is set by getopt to the string that follows
				// this option (e.g. "-s 2" would assign optarg to the string "2")
				num_sets = 1 << strtol(optarg, NULL, 10);
				//directly grabs set bit argument to set bit variable
				s = atoi(optarg);
				break;
			case 't':
				// specify the trace filename
				trace_filename = optarg;
				break;
			case 'E':
				//grabs set lines argument and stores it to E
				E = atoi(optarg);
				break;
			case 'b':
				// grabs byte bit argument and stores to b
				b = atoi(optarg);
				break;
			default:
				usage(argv[0]);
				exit(1);
		}
	}

	// TODO: When you are ready to start using the user defined options, you
	// should add some code here that makes sure they have actually specified
	// the options that are required (e.g. -t and -s are both required).

	if (verbose_mode) {
		printf("Verbose mode enabled.\n");
		printf("Trace filename: %s\n", trace_filename);
		printf("Number of sets: %d\n", num_sets);
		printf("Number of lines: %d\n", E);
		printf("Number of byte offset bits: %d\n", b);
	}

	simulateCache(trace_filename, num_sets, E, s, b, verbose_mode);

    return 0;
}

/**
 * Simulates cache with the specified organization (S, E, B) on the given
 * trace file.
 *
 * @param trace_file Name of the file with the memory addresses.
 * @param num_sets Number of sets in the simulator.
 * @param E Number of lines in each cache set.
 * @param s Number of set index bits.
 * @param b Number of byte offset bits in each cache block.
 * @param verbose Whether to print out extra information about what the
 *   simulator is doing (1 = yes, 0 = no).
 */
void simulateCache(char *trace_file, int num_sets, int E, int s, int b, int verbose) {
	// Variables to track how many hits, misses, and evictions we've had so
	// far during simulation.
	int hit_count = 0;
	int miss_count = 0;
	int eviction_count = 0;

	// TODO: This is where you will fill in the code to perform the actual
	// cache simulation. Make sure you split your work into multiple functions
	// so that each function is as simple as possible.
	
	//open file to read from
	FILE *my_file = fopen(trace_file, "r");
	if (my_file == NULL) {
		printf("Error: file open %s\n", trace_file);
		exit(1);
	}	
	//declare variables to save fscanf into
	int ret = 0;
	int byte = 0;
	char operation[10];
	//memory address variables
	mem_addr a;
	mem_addr set_bit;
	mem_addr tag_bit;
	//set up mask to pack bit values
	mem_addr set_mask = (mem_addr)pow(2,s) - 1;
	mem_addr tag_mask = (mem_addr)pow(2,64 -(b + s)) - 1;

	//initialize cache
	Cache cache;
	cache.num_sets = num_sets;
	cache.sets = calloc(num_sets, sizeof(Set));
	int i;
	for (i = 0; i < cache.num_sets; i++) {
		cache.sets[i].num_lines = E;
		cache.sets[i].lines = calloc(E, sizeof(Line));
		int j;
		for (j = 0; j < E; j++) {
			cache.sets[i].lines[j].LRU = j;
		}
	}
	
	//check if scan brought in the operation address and byte block
	ret = fscanf(my_file, "%s %lx, %d", operation, &a, &byte);
	
	//loop through file of addresses
	while (ret == 3) {
		if (strcmp(operation, "I") != 0) {
			set_bit = set_mask & (a >> b);
			tag_bit = tag_mask & (a >> (s + b));
			compCache(cache, &set_bit, &tag_bit, &miss_count, &hit_count, &eviction_count);
		}
		if (strcmp(operation, "M") == 0) {
			compCache(cache, &set_bit, &tag_bit, &miss_count, &hit_count, &eviction_count);
		}
		ret = fscanf(my_file, "%s %lx, %d", operation, &a, &byte);
	}

	fclose(my_file);
	
	//loop through cache sets to free structs
	for (i = 0; i < cache.num_sets; i++)
	{
		free(cache.sets[i].lines);
	}
	free(cache.sets);
	printSummary(hit_count, miss_count, eviction_count);
}

/**
 * compares memory address to the cache to look and count for hit, miss, and evictions
 *
 * @param cache The cache holding all the cache lines
 * @param set_bit Bits to hold set identification data
 * @param tag_bit Bits to hold tag identification data
 * @param miss_count Counter for how many misses
 * @param hit_count Counter for how many hits
 * @param eviction_count Counter for how many evictions
 */
void compCache(Cache cache, mem_addr *set_bit, mem_addr *tag_bit, int *miss_count,
	   	int *hit_count, int *eviction_count){
	int line_found	= 0;
	int i;
	int line = 0;
	int LRU;
	
	// checks if this is a hit
	//for loop to go through the lines of that set index
	for(i = 0; i < cache.sets[*set_bit].num_lines; i++) {
		//check for valid bit
		if(cache.sets[*set_bit].lines[i].valid == 1) {
			//check for tag bit
			if(cache.sets[*set_bit].lines[i].tag == *tag_bit) {
				line = i;
				(*hit_count)++;
				line_found = 1;
				cache.sets[*set_bit].lines[i].LRU = 0;
			}	
		}
	}
	
	// find line that has to be updated
	if(line_found == 0){
		(*miss_count)++;
		for(i = 0; i < cache.sets[*set_bit].num_lines; i++){
			//have to find the highest lru, then go change that linee
			if(cache.sets[*set_bit].lines[i].valid == 0){
				line = i;
			   	line_found = 1;
				cache.sets[*set_bit].lines[i].tag = *tag_bit;
				cache.sets[*set_bit].lines[i].valid = 1;
				cache.sets[*set_bit].lines[i].LRU = 0;
				break;
			}
		}
		
		
		if(line_found == 0){
			LRU = 0;
			for(i = 0; i < cache.sets[*set_bit].num_lines; i++){
				if(cache.sets[*set_bit].lines[i].LRU > LRU){
					LRU = cache.sets[*set_bit].lines[i].LRU;
				}
			}
			
			//update LRU line
			for(i = 0; i < cache.sets[*set_bit].num_lines; i++){
				if(cache.sets[*set_bit].lines[i].LRU == LRU){
					(*eviction_count)++;
					line = i;
					line_found = 1;
					cache.sets[*set_bit].lines[i].tag = *tag_bit;
					cache.sets[*set_bit].lines[i].valid = 1;
					cache.sets[*set_bit].lines[i].LRU = 0;
					break;
				}
			}
		}
	}
	//update all other LRUs 
	for(i = 0; i < cache.sets[*set_bit].num_lines; i++){
		if(i != line){
			cache.sets[*set_bit].lines[i].LRU ++;
		}
	}
}
 /*

	}
*/
void updateLRU(Cache cache, mem_addr *set_bit, int i){
	for(int j = 0; j < cache.sets[*set_bit].num_lines; j++){	
		if(j !=	i){
			if(cache.sets[*set_bit].lines[i].LRU == 0){
				if(cache.sets[*set_bit].num_lines != 1) {
				   	cache.sets[*set_bit].lines[i].LRU = cache.sets[*set_bit].num_lines - 1;
	   			}
	 		}
	
	   		else {
		   		if(cache.sets[*set_bit].num_lines != 1){
			   		cache.sets[*set_bit].lines[i].LRU = cache.sets[*set_bit].lines[i].LRU -1;
        	 	}
			}
		}
		else
			return;
		}
	}
