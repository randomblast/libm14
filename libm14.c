/* lib14.c
 * MPEG4 Part 14 library
 *
 * Copyright (C) 2010 Josh Channings <josh@channings.me.uk>
 */
#include "libm14.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/* m14_file functions */
m14_file *m14_file_open(char *filename) {
	m14_file *f = malloc(sizeof(m14_file));
	m14_atom *cur;
	uint32_t pos = 0;
	struct stat rfstat;

	// Open the file, or bomb
	f->fd = open(filename, O_RDONLY);
	if(f->fd == -1)
		return NULL;

	fstat(f->fd, &rfstat);
	f->rl = rfstat.st_size;

	// MMAP the file, or bomb
	f->rf = mmap(NULL, f->rl, PROT_READ, MAP_SHARED, f->fd, 0);

	// Check ftyp

	// Set up root atom
	f->root = malloc(sizeof(m14_atom));
	f->root->size = f->rl;
	f->root->code = 0;
	f->root->data_type = CONTAINER;

	// Main read loop
	while(pos < f->rl)
	{
		cur = m14_atom_parse(f->rf + pos);
		if(cur == NULL)
			return f;
		m14_atom_append(f->root, cur);
		pos += cur->size;
	}
	
	return f;
}
int m14_file_close(m14_file *f) {
	munmap(f->rf, f->rl);
	close(f->fd);
}
int m14_file_write(m14_file *f, char *path) {
}

/* m14_atom functions */
m14_atom *m14_atom_parse(void *data) {
	m14_atom *a = malloc(sizeof(m14_atom));

	a->parent = NULL;
	a->n_children = NULL;

	memcpy(&a->size, data, 4);
	a->size = m14_swap_ends(a->size);

	// Basic validity test
	if(a->size < 8)
	{
		free(a);
		return NULL;
	}
	
	memcpy(&a->code, data + 4, 4);
	a->code = m14_swap_ends(a->code);

	// Add children
	if(m14_is_container(a->code))
	{
		a->data_type = CONTAINER;

		m14_atom *child;
		void *pos = data + m14_atom_header_length(a->code);

		while(pos < data + a->size)
		{
			child = m14_atom_parse(pos);
			if(child == NULL) return NULL;
			m14_atom_append(a, child);
			pos += child->size;
		}

	// Add pointer to data
	} else if(a->size != 8)
	{
		a->data_type = READ;
		a->data = data + m14_atom_header_length(a->code);
	}

	return a;
}
m14_atom *m14_atom_copy(m14_atom *a) {
}
int m14_atom_edit(m14_atom *a) {
	void *buf;

	if(a->data_type == WRITE)
		return; // Do nothing, this function has already been run
	
	if(a->data_type == READ)
	{
		// Copy data
		buf = malloc(a->size - 8);
		buf = memcpy(buf, a->data, a->size - 8);

		// Update struct
		a->data = buf;
		a->data_type = WRITE;
	}
}
int m14_atom_save(m14_atom *a) {
}
int m14_atom_prepend(m14_atom *dest, m14_atom *a) {
}
int m14_atom_append(m14_atom *dest, m14_atom *a) {
	if(dest->data_type != CONTAINER)
		return -1;
	
	// Modify children list
	dest->children = realloc(dest->children, sizeof(m14_atom*) * (dest->n_children + 1));
	dest->children[dest->n_children] = a;
	dest->n_children++;

	// Reparent atom
	m14_atom_orphan(a);
	a->parent = dest;
}
int m14_atom_orphan(m14_atom *a) {
	uint32_t i;
	m14_atom *parent = a->parent;

	if(parent == NULL)
		return -1;

	for(i = 0;i < parent->n_children;i++)
	{
		if(a->parent == NULL)
			parent->children[i - 1] = parent->children[i];

		else if(parent->children[i] == a)
			a->parent = NULL;
	}

	parent->n_children--;
	realloc(parent->children, sizeof(m14_atom*) * parent->n_children);

	return 0;
}

/* Helper functions */
int m14_is_container(uint32_t code) {
	/* This list must be kept in ascending order */
	static uint32_t codes[] = {
	};

	static uint32_t n_codes = (sizeof(codes) / sizeof(uint32_t));
	int i;

	for(i = 0;i < n_codes;i++)
		if(codes[i] == code)
			return 1;
	
	return 0;
}
int m14_atom_header_length(uint32_t code) {
	// SIZE CODE FLAG = 12 bytes
	static uint32_t b12[] = {
		0x6d657461	// meta
	};

	// SIZE CODE FLAG FLAG = 16 bytes
	static uint32_t b16[] = {
		0x64617461	// data
	};

	static uint32_t n_b12 = (sizeof(b12) / sizeof(uint32_t));
	static uint32_t n_b16 = (sizeof(b16) / sizeof(uint32_t));
	int i;

	for(i = 0;i < n_b12;i++)
		if(b12[i] == code)
			return 12;

	for(i = 0;i < n_b16;i++)
		if(b16[i] == code)
			return 16;
	
	return 8;
}
m14_results *m14_find(char *path, m14_atom *root) {
}
int m14_print_tree(m14_atom *a, int depth) {
	int cols = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	int i, data_length = cols - (depth * 2) - 16;

	/* Clean up code */
	uint64_t code = m14_swap_ends(a->code) << 32;

	int _depth = depth;
	while(--_depth > 0)
		printf("  ");

	if(a->code != 0)
		printf("[%s] %08x %.*s\n", (char*) &code, a->size, data_length, (char*) a->data);

	for(i = 0;i < a->n_children;i++)
		m14_print_tree(a->children[i], depth + 1);
}
uint32_t m14_swap_ends(uint32_t x) {
	return	(x>>24)
		|	((x<<8) & 0x00ff0000)
		|	((x>>8) & 0x0000ff00)
		|	(x<<24);
}
