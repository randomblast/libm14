/* lib14.h
 * MPEG4 Part 14 library
 *
 * Copyright (C) 2010 Josh Channings <josh@channings.me.uk>
 */

#ifndef __LIB14_H
#define __LIB14_H

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

typedef struct _m14_atom m14_atom;
typedef struct _m14_file m14_file;
typedef struct _m14_results m14_results;

struct _m14_file {
	/* Information about read file */
	int fd;			///< File Descriptor for read file
	void *rf;		///< Pointer to mmap'd read file
	uint32_t rl;	///< Length of read file

	/**
	 * This is a special atom.
	 * parent == NULL, and it has no code. size should never be referenced, until it's time
	 * to write the file, at which point it is computed by m14_compute_atom_size()
	 */
	m14_atom *root;
};
struct _m14_atom {
	// These 2 vars translate directly into the atom
	uint32_t size;
	uint32_t code;

	
	void *data;		///< Pointer to data region
	void *mdata;	///< Metadata

	/// What should we treat this atom as? Still in the readfile, or edited
	enum
	{
		READ		///< Data resides in the mmap'd readfile; it hasn't been altered
	,	WRITE		///< Data is owned by this struct. Length is this->size - m14_atom_header_length
	,	CONTAINER	///< This atom has no data - it's a container
	} data_type;

	struct m14_file *f;

	struct m14_atom *parent;

	uint32_t n_children;
	struct m14_atom **children;
};
struct _m14_results {
	uint32_t size;
	m14_atom **a;
};

/* m14_file functions */
m14_file *m14_file_open(char *filename);			///< Open a file and parse into a tree
int m14_file_close(m14_file*);						///< Cleanup and close
int m14_file_write(m14_file*, char *path);			///< Write changes out

/* m14_atom functions */
m14_atom *m14_atom_parse(void*, m14_file*);			///< New atom from file data
m14_atom *m14_atom_copy(m14_atom*);					///< Copy an atom. Returns an orphan.
int m14_atom_edit(m14_atom*);						///< Prepare atom for editing
int m14_atom_save(m14_atom*);						///< Prepare atom for writing to disk
int m14_atom_prepend(m14_atom *dest, m14_atom *a);	///< Prepend a to dest
int m14_atom_append(m14_atom *dest, m14_atom *a);	///< Append a to dest
int m14_atom_orphan(m14_atom*);						///< Remove an atom from a tree
char *m14_atom_describe(m14_atom*, void*, int len);	///< Explain atom contents (for testing)

/* Helper functions */
int m14_is_container(uint32_t code);				///< Can this fourcc contain children?
int m14_atom_header_length(uint32_t code);			///< How many bytes for size/code/flags
m14_results *m14_find(char *path, m14_atom *root);	///< Perform a tree search
int m14_print_tree(m14_atom*);						///< Debug an atom tree
/// Debug an atom tree by callback
int m14_print_callback(m14_atom*, char *(*)(m14_atom*, void *arg, int len), void *arg, int depth);
uint32_t m14_swap_ends(uint32_t);					///< Swap between little-endian/big-endian
int m14_recurse(m14_atom*, int(*fn)(m14_atom*));	///< Call a function on a whole tree

/* Atom describers */
char *m14_describe_stco(m14_atom*, int len);
char *m14_describe_hdlr(m14_atom*, int len);

#endif // __LIB14_H

