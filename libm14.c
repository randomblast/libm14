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
	f->root->f = f;

	// Main read loop
	while(pos < f->rl)
	{
		cur = m14_atom_parse(f->rf + pos, f);
		if(cur == NULL)
			return f;
		m14_atom_append(f->root, cur);
		pos += cur->size;
	}
	
	// Do atom read functions
	m14_recurse(f->root, &m14_atom_read);

	return f;
}
int m14_file_close(m14_file *f) {
	munmap(f->rf, f->rl);
	close(f->fd);
}
int m14_file_write(m14_file *f, char *path) {
}

/* m14_atom functions */
m14_atom *m14_atom_parse(void *data, m14_file *f) {
	m14_atom *a = malloc(sizeof(m14_atom));

	a->f = f;
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
			child = m14_atom_parse(pos, f);
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

	// Propagate m14_file pointer
	a->f = dest->f;
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
char *m14_atom_describe(m14_atom *a, void *arg, int len) {
	static struct {uint32_t code; char *(*fn)(m14_atom*, int len);} describers[] = {
		{0x7374636f, &m14_describe_stco}
	,	{0x68646c72, &m14_describe_hdlr}
	};

	char *ret;
	int i;
	
	if(a->data_type == CONTAINER)
	{
		ret = malloc(10);
		ret = strcpy(ret, "Container");
		return ret;
	}

	for(i = 0;i < sizeof(describers) / (sizeof(uint32_t) + sizeof(void*));i++)
		if(describers[i].code == a->code)
			return describers[i].fn(a, len);

	ret = malloc(len);
	snprintf(ret, len, "%s", a->data);
	return ret;
}
int	m14_atom_read(m14_atom *a) {
	static struct {uint32_t code; int (*fn)(m14_atom*);} readers[] = {
		{0x7374636f, &m14_read_stco}
	};

	int i;

	for(i = 0;i < sizeof(readers) / (sizeof(uint32_t) + sizeof(void*));i++)
		if(readers[i].code == a->code)
			return readers[i].fn(a);
}
int m14_atom_write(m14_atom *a) {
	static struct {uint32_t code; int (*fn)(m14_atom*);} writers[] = {
		{0x7374636f, &m14_write_stco}
	};

	int i;

	for(i = 0;i < sizeof(writers) / (sizeof(uint32_t) + sizeof(void*));i++)
		if(writers[i].code == a->code)
			return writers[i].fn(a);
}
uint32_t m14_atom_size(m14_atom *a) {
	static struct {uint32_t code; uint32_t (*fn)(m14_atom*);} sizers[] = {
	};

	int i;

	for(i = 0;i < sizeof(sizers) / (sizeof(uint32_t) + sizeof(void*));i++)
		if(sizers[i].code == a->code)
			return sizers[i].fn(a);

	return a->size;
}

/* Helper functions */
int m14_is_container(uint32_t code) {
	/* This list should be kept in the order of most frequent occurence */
	static uint32_t codes[] = {
		0x00000000	// Root atom
	,	0x7374626c	// stbl
	,	0x6d646961	// mdia
	,	0x6d696e66	// minf
	,	0x64696e66	// dinf
	,	0x7472616b	// trak
	,	0x65647473	// edts
	,	0x75647461	// udta User Data
	,	0x696c7374	// ilst iTunes Metadata
	,	0x6d6f6f76	// moov
	,	0x6d657461	// meta

	// iTunes tags - live under /moov/udta/meta/ilst
	,	0x61617274	// aart Album Artist
	,	0x636f7672	// covr Album Cover
	,	0x63707274	// cprt Copyright
	,	0x6370696c	// cpil Compilation
	,	0x6469736b	// disk Disk Number
	,	0x676e7265	// gnre Genre
	,	0x74726b6e	// trkn	Track Number
	,	0x746d706f	// tmpo BPM
	,	0x72746e67	// rtng Rating/Advisory
	,	0x7374696b	// stik Type of file
	,	0x70637374	// pcst Podcast flag
	,	0x63617467	// catg Category
	,	0x6b657977	// keyw Keyword
	,	0x7075726c	// purl Podcast URL
	,	0x65676964	// egid Episode Global Unique ID
	,	0x64657363	// desc	Description
	,	0x74766e6e	// tvnn TV Network Name
	,	0x74767368	// tvsh TV Show
	,	0x7476656e	// tven TV Episode Number
	,	0x7476736e	// tvsn TV Season Number
	,	0x74766573	// tves
	,	0x70757264	// purd Purchase Date
	,	0x70676170	// pgap Gapless Playback flag
	,	0xa9415254	// ©ART Artist
	,	0xa9616c62	// ©alb Album
	,	0xa9636d74	// ©cmt Comment
	,	0xa9646179	// ©day Year
	,	0xa967656e	// ©gen Genre
	,	0xa9677270	// ©grp Grouping
	,	0xa9746f6f	// ©too Encoder
	,	0xa96c7972	// ©lyr Lyrics
	,	0xa96e616d	// ©nam Title
	,	0xa9777274	// ©wrt Composer
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
int m14_print_tree(m14_atom *a) {
	return m14_print_callback(a, &m14_atom_describe, NULL, 0);
}
int m14_print_callback(m14_atom *a, char *(*fn)(m14_atom*, void*, int), void *arg, int depth) {
	int cols = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	int i, data_length = cols - (depth * 2) - 16;

	/* Clean up code */
	uint64_t code = m14_swap_ends(a->code) << 32;

	int _depth = depth;
	while(--_depth > 0)
		printf("  ");

	if(a->code != 0)
	{
		char *result = (char*) fn(a, arg, data_length);
		printf("[%s] %08x %s\n", (char*) &code, a->size, result);
		free(result);
	}

	for(i = 0;i < a->n_children;i++)
		m14_print_callback(a->children[i], fn, arg, depth + 1);
}
uint32_t m14_swap_ends(uint32_t x) {
	return	(x>>24)
		|	((x<<8) & 0x00ff0000)
		|	((x>>8) & 0x0000ff00)
		|	(x<<24);
}
int m14_recurse(m14_atom *a, int(*fn)(m14_atom*)) {
	int i;

	fn(a);

	for(i = 0;i < a->n_children;i++)
		m14_recurse(a->children[i], fn);
}

/* Atom describers */
char *m14_describe_stco(m14_atom *a, int len) {
	char *ret = malloc(len);

	uint32_t n_chunks = ((m14_mdata_stco*) a->mdata)->n_chunks;

	snprintf(
		ret
	,	len
	,	"%4d chunks from %08x to %08x"
	,	n_chunks
	,	((m14_mdata_stco*) a->mdata)->rel_offsets[0]
	,	((m14_mdata_stco*) a->mdata)->rel_offsets[n_chunks - 1]
	);
	return ret;
}
char *m14_describe_hdlr(m14_atom *a, int len) {
	char *ret = malloc(len);
	snprintf(ret, len, "%s", a->data + 24);
	return ret;
}

/* Atom readers */
int m14_read_stco(m14_atom *a) {
	// Take offsets relative to file from a->data, find offsets relative to mdat

	m14_atom *root = a->f->root;

	uint32_t i, tmp;
	static uint32_t mdat_pos = 0;
	m14_mdata_stco *mdata = malloc(sizeof(m14_mdata_stco));

	// Get mdat pos
	for(i = 0;i < root->n_children && mdat_pos == 0;i++)
		if(root->children[i]->code == 0x6d646174) // mdat
			mdat_pos = (uint32_t) (root->children[i]->data - a->f->rf);

	// Get number of chunks in table
	memcpy((void*) &mdata->n_chunks, a->data + 4, sizeof(uint32_t));
	mdata->n_chunks = m14_swap_ends(mdata->n_chunks);

	// Allocate array
	mdata->rel_offsets = malloc(4 * mdata->n_chunks);
	
	for(i = 0;i < mdata->n_chunks;i++)
	{
		memcpy((void*) &tmp, a->data + 8 + (i * 4), 4);
		mdata->rel_offsets[i] = m14_swap_ends(tmp) - mdat_pos;
	}
		
	a->mdata = (void*) mdata;
}

/* Atom writers */
int m14_write_stco(m14_atom *a) {
	// Take offsets relative to mdat from a->mdata, find offsets relative to file
	static uint32_t mdat_pos = 0;
	m14_atom *root = a->f->root;
	
	int i;

	// Get mdat pos
	for(i = 0;i < root->n_children && mdat_pos == 0;i++)
		if(root->children[i]->code == 0x6d646174) // mdat
			mdat_pos = (uint32_t) (root->children[i]->data - a->f->rf);
}

