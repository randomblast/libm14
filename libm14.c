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
		void *pos = data + 8;

		while(pos < data + a->size)
		{
			child = m14_atom_parse(pos);
			if(child == NULL) return NULL;
			m14_atom_append(a, child);
			pos += a->size;
		}

	// Add pointer to data
	} else if(a->size != 8)
	{
		a->data_type = READ;
		a->data = data + 8;
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
	int32_t last = -1, i = n_codes / 2;


	for(i = 0;i < n_codes;i++)
		if(codes[i] == code)
			return 1;
	
	return 0;
/* THIS IS THE CLEVER VERSION:
	while(0 < i < n_codes - 1)
	{
		if(last == i) return 0;
		else last = i;

		if(codes[i] == code) return 1;
		else if(codes[i] < code) i = i >= 2 ? i / 2 : i--;
		else if(codes[i] > code) i = i <= n_codes / 2 ? i * 2 : i++;
	}

	return 0;
	*/
}
m14_results *m14_find(char *path, m14_atom *root) {
}
int m14_print_tree(m14_atom *a, int depth) {
	int i, data_length = 80 - (depth * 2) - 16;

	/* Clean up code */
	uint64_t code = m14_swap_ends(a->code) << 32;

	int _depth = depth;
	while(--_depth > 0)
		printf("  ");

	if(a->code != 0)
		printf("[%s] %08x %s\n", (char*) &code, a->size, (char*) a->data, data_length);

	for(i = 0;i < a->n_children;i++)
		m14_print_tree(a->children[i], depth + 1);
}
uint32_t m14_swap_ends(uint32_t x) {
	return	(x>>24)
		|	((x<<8) & 0x00ff0000)
		|	((x>>8) & 0x0000ff00)
		|	(x<<24);
}
