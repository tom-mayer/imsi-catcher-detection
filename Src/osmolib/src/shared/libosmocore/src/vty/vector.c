/* Generic vector interface routine
 * Copyright (C) 1997 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <stdlib.h>
#include <unistd.h>

#include <osmocom/vty/vector.h>
#include <osmocom/vty/vty.h>
#include <osmocom/core/talloc.h>
#include <memory.h>

void *tall_vty_vec_ctx;

/* Initialize vector : allocate memory and return vector. */
vector vector_init(unsigned int size)
{
	vector v = talloc_zero(tall_vty_vec_ctx, struct _vector);
	if (!v)
		return NULL;

	/* allocate at least one slot */
	if (size == 0)
		size = 1;

	v->alloced = size;
	v->active = 0;
	v->index = _talloc_zero(tall_vty_vec_ctx, sizeof(void *) * size,
				"vector_init:index");
	if (!v->index) {
		talloc_free(v);
		return NULL;
	}
	return v;
}

void vector_only_wrapper_free(vector v)
{
	talloc_free(v);
}

void vector_only_index_free(void *index)
{
	talloc_free(index);
}

void vector_free(vector v)
{
	talloc_free(v->index);
	talloc_free(v);
}

vector vector_copy(vector v)
{
	unsigned int size;
	vector new = talloc_zero(tall_vty_vec_ctx, struct _vector);
	if (!new)
		return NULL;

	new->active = v->active;
	new->alloced = v->alloced;

	size = sizeof(void *) * (v->alloced);
	new->index = _talloc_zero(tall_vty_vec_ctx, size, "vector_copy:index");
	if (!new->index) {
		talloc_free(new);
		return NULL;
	}
	memcpy(new->index, v->index, size);

	return new;
}

/* Check assigned index, and if it runs short double index pointer */
void vector_ensure(vector v, unsigned int num)
{
	if (v->alloced > num)
		return;

	v->index = talloc_realloc_size(tall_vty_vec_ctx, v->index,
				       sizeof(void *) * (v->alloced * 2));
	memset(&v->index[v->alloced], 0, sizeof(void *) * v->alloced);
	v->alloced *= 2;

	if (v->alloced <= num)
		vector_ensure(v, num);
}

/* This function only returns next empty slot index.  It dose not mean
   the slot's index memory is assigned, please call vector_ensure()
   after calling this function. */
int vector_empty_slot(vector v)
{
	unsigned int i;

	if (v->active == 0)
		return 0;

	for (i = 0; i < v->active; i++)
		if (v->index[i] == 0)
			return i;

	return i;
}

/* Set value to the smallest empty slot. */
int vector_set(vector v, void *val)
{
	unsigned int i;

	i = vector_empty_slot(v);
	vector_ensure(v, i);

	v->index[i] = val;

	if (v->active <= i)
		v->active = i + 1;

	return i;
}

/* Set value to specified index slot. */
int vector_set_index(vector v, unsigned int i, void *val)
{
	vector_ensure(v, i);

	v->index[i] = val;

	if (v->active <= i)
		v->active = i + 1;

	return i;
}

/* Look up vector.  */
void *vector_lookup(vector v, unsigned int i)
{
	if (i >= v->active)
		return NULL;
	return v->index[i];
}

/* Lookup vector, ensure it. */
void *vector_lookup_ensure(vector v, unsigned int i)
{
	vector_ensure(v, i);
	return v->index[i];
}

/* Unset value at specified index slot. */
void vector_unset(vector v, unsigned int i)
{
	if (i >= v->alloced)
		return;

	v->index[i] = NULL;

	if (i + 1 == v->active) {
		v->active--;
		while (i && v->index[--i] == NULL && v->active--) ;	/* Is this ugly ? */
	}
}

/* Count the number of not emplty slot. */
unsigned int vector_count(vector v)
{
	unsigned int i;
	unsigned count = 0;

	for (i = 0; i < v->active; i++)
		if (v->index[i] != NULL)
			count++;

	return count;
}
