/*
 * This file is part of Pytricia.
 * Joel Sommers <jsommers@colgate.edu>
 * 
 * Pytricia is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Pytricia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with Pytricia.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * $Id: patricia.c 6811 2009-07-06 20:41:10Z robin $
 * Dave Plonka <plonka@doit.wisc.edu>
 *
 * This product includes software developed by the University of Michigan,
 * Merit Network, Inc., and their contributors. 
 *
 * This file had been called "radix.c" in the MRT sources.
 *
 * I renamed it to "patricia.c" since it's not an implementation of a general
 * radix trie.  Also I pulled in various requirements from "prefix.c" and
 * "demo.c" so that it could be used as a standalone API.
 */

/* From copyright.txt:
 * 
 * Copyright (c) 1997, 1998, 1999
 * 
 * 
 * The Regents of the University of Michigan ("The Regents") and Merit Network,
 * Inc.  All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1.  Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 3.  All advertising materials mentioning features or use of 
 *     this software must display the following acknowledgement:  
 * This product includes software developed by the University of Michigan, Merit
 * Network, Inc., and their contributors. 
 * 4.  Neither the name of the University, Merit Network, nor the
 *     names of their contributors may be used to endorse or 
 *     promote products derived from this software without 
 *     specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 */

#define COPYRIGHT \
"This product includes software developed by the University of Michigan, Merit "\
"Network, Inc., and their contributors."

#include <assert.h> 
#include <ctype.h> 
#include <errno.h> 
#include <math.h> 
#include <stddef.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/types.h> 
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h> 
#endif

#include "patricia.h"

#define Delete free

/* { from prefix.c */

/* prefix_tochar
 * convert prefix information to bytes
 */
u_char *
prefix_tochar (prefix_t * prefix)
{
	if (prefix == NULL)
	return (NULL);

	return ((u_char *) & prefix->add.sin);
}

int 
comp_with_mask (void *addr, void *dest, u_int mask)
{

	if ( /* mask/8 == 0 || */ memcmp (addr, dest, mask / 8) == 0) {
	   	int n = mask / 8;
	   	int m = ((-1) << (8 - (mask % 8)));

	   	if (mask % 8 == 0 || (((u_char *)addr)[n] & m) == (((u_char *)dest)[n] & m))
			return (1);
	}
	return (0);
}

/* inet_pton substitute implementation
 * Uses inet_addr to convert an IP address in dotted decimal notation into 
 * unsigned long and copies the result to dst.
 * Only supports AF_INET.  Follows standard error return conventions of 
 * inet_pton.
 */
int
local_inet_pton (int af, const char *src, void *dst)
{
	u_long result;  

	if (af == AF_INET) {
	result = inet_addr(src);
	if (result == INADDR_NONE)
		return 0;
	else {
			memcpy (dst, &result, 4);
		return 1;
		}
	}
#ifdef NT
#ifdef HAVE_IPV6
	else if (af == AF_INET6) {
		struct in6_addr Address;
		return (inet6_addr(src, &Address));
	}
#endif /* HAVE_IPV6 */
#endif /* NT */
#ifndef NT
	else {

	errno = EAFNOSUPPORT;
	return -1;
	}
#endif /* NT */
}

/* this allows imcomplete prefix */
int
my_inet_pton (int af, const char *src, void *dst)
{
	if (af == AF_INET) {
		int i, c, val;
		u_char xp[4] = {0, 0, 0, 0};

		for (i = 0; ; i++) {
		c = *src++;
		if (!isdigit (c))
		return (-1);
		val = 0;
		do {
		val = val * 10 + c - '0';
		if (val > 255)
			return (0);
		c = *src++;
		} while (c && isdigit (c));
			xp[i] = val;
		if (c == '\0')
		break;
			if (c != '.')
				return (0);
		if (i >= 3)
		return (0);
		}
	memcpy (dst, xp, 4);
		return (1);
#ifdef HAVE_IPV6
	} else if (af == AF_INET6) {
		return (local_inet_pton (af, src, dst));
#endif /* HAVE_IPV6 */
	} else {
#ifndef NT
	errno = EAFNOSUPPORT;
#endif /* NT */
	return -1;
	}
}

/* 
 * convert prefix information to ascii string with length
 * thread safe and (almost) re-entrant implementation
 */
char *
prefix_toa2x (prefix_t *prefix, char *buff, int with_len)
{
	if (prefix == NULL)
	return ("(Null)");
	assert (prefix->ref_count >= 0);
	if (buff == NULL) {

		struct buffer {
			char buffs[16][48+5];
			u_int i;
		} *buffp;

#    if 0
	THREAD_SPECIFIC_DATA (struct buffer, buffp, 1);
#    else
		{ /* for scope only */
	   static struct buffer local_buff;
		   buffp = &local_buff;
	} 
#    endif
	if (buffp == NULL) {
		/* XXX should we report an error? */
		return (NULL);
	}

	buff = buffp->buffs[buffp->i++%16];
	}
	if (prefix->family == AF_INET) {
	u_char *a;
	assert (prefix->bitlen <= 32);
	a = prefix_touchar (prefix);
	if (with_len) {
		snprintf (buff, 64, "%d.%d.%d.%d/%d", a[0], a[1], a[2], a[3],
			 prefix->bitlen);
	}
	else {
		snprintf (buff, 64, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	}
	return (buff);
	}
#ifdef HAVE_IPV6
	else if (prefix->family == AF_INET6) {
	char *r;
	r = (char *) inet_ntop (AF_INET6, &prefix->add.sin6, buff, 48 /* a guess value */ );
	if (r && with_len) {
		assert (prefix->bitlen <= 128);
		snprintf (buff+strlen(buff), 64-strlen(buff), "/%d", prefix->bitlen);
	}
	return (buff);
	}
#endif /* HAVE_IPV6 */
	else
	return (NULL);
}

/* prefix_toa2
 * convert prefix information to ascii string
 */
char *
prefix_toa2 (prefix_t *prefix, char *buff)
{
	return (prefix_toa2x (prefix, buff, 0));
}

/* prefix_toa
 */
char *
prefix_toa (prefix_t * prefix)
{
	return (prefix_toa2 (prefix, (char *) NULL));
}

int
New_Prefix (int family, void *dest, int bitlen, prefix_t *prefix)
{
	int dynamic_allocated = 0;
	int default_bitlen = 32;

#ifdef HAVE_IPV6
	if (family == AF_INET6) {
		default_bitlen = 128;
		if (prefix == NULL) {
			prefix = calloc(1, sizeof (prefix6_t));
			dynamic_allocated++;
		}
		memcpy (&prefix->add.sin6, dest, 16);
	}
	else
#endif /* HAVE_IPV6 */
	if (family == AF_INET) {
		if (prefix == NULL) {
#ifndef NT
			prefix = calloc(1, sizeof (prefix4_t));
#else
			//for some reason, compiler is getting
			//prefix4_t size incorrect on NT
			prefix = calloc(1, sizeof (prefix_t)); 
#endif /* NT */
		
			dynamic_allocated++;
		}
		memcpy (&prefix->add.sin, dest, 4);
	}
	else {
		return 0;
	}

	prefix->bitlen = (bitlen >= 0)? bitlen: default_bitlen;
	prefix->family = family;
	prefix->ref_count = 0;
	if (dynamic_allocated) {
		prefix->ref_count++;
   }
/* fprintf(stderr, "[C %s, %d]\n", prefix_toa (prefix), prefix->ref_count); */
	return 1;
}

/* } */

/* #define PATRICIA_DEBUG 1 */

static int num_active_patricia = 0;

/* these routines support continuous mask only */

patricia_tree_t *
New_Patricia (int maxbits)
{
	patricia_tree_t *patricia = calloc(1, sizeof *patricia);

	patricia->maxbits = maxbits;
	patricia->head = NULL;
	patricia->num_active_node = 0;
	patricia->frozen = 0;
	assert (maxbits <= PATRICIA_MAXBITS); /* XXX */
	num_active_patricia++;
	return (patricia);
}


/*
 * if func is supplied, it will be called as func(node->data)
 * before deleting the node
 */

void
Clear_Patricia (patricia_tree_t *patricia, void_fn1_t func)
{
	assert (patricia);
	if (patricia->head) {
		patricia_node_t *Xstack[PATRICIA_MAXBITS+1];
		patricia_node_t **Xsp = Xstack;
		patricia_node_t *Xrn = patricia->head;

		while (Xrn) {
			patricia_node_t *l = Xrn->l;
			patricia_node_t *r = Xrn->r;

			if (Xrn->data) {
				if (func)
					func (Xrn->data);
			}
			else {
				assert (Xrn->data == NULL);
			}
			if(!patricia->frozen) {
				Delete (Xrn);
			}
			patricia->num_active_node--;

			if (l) {
				if (r) {
					*Xsp++ = r;
				}
				Xrn = l;
			} else if (r) {
				Xrn = r;
			} else if (Xsp != Xstack) {
				Xrn = *(--Xsp);
			} else {
				Xrn = (patricia_node_t *) 0;
			}
		}
	}
	assert (patricia->num_active_node == 0);
	if(patricia->frozen && patricia->head) {
		Delete (patricia->head);
	}
		/* Delete (patricia); */
}


void
Destroy_Patricia (patricia_tree_t *patricia, void_fn1_t func)
{
	Clear_Patricia (patricia, func);
	Delete (patricia);
	num_active_patricia--;
}


/*
 * if func is supplied, it will be called as func(node->prefix, node->data)
 */

void
patricia_process (patricia_tree_t *patricia, void_fn2_t func)
{
	patricia_node_t *node;
	assert (func);

	PATRICIA_WALK (patricia->head, node) {
		func (&node->prefix, node->data);
	} PATRICIA_WALK_END;
}


patricia_node_t *
patricia_search_exact (patricia_tree_t *patricia, prefix_t *prefix)
{
	patricia_node_t *node;
	u_char *addr;
	u_int bitlen;

	assert (patricia);
	assert (prefix);
	assert (prefix->bitlen <= patricia->maxbits);

	if (patricia->head == NULL)
	return (NULL);

	node = patricia->head;
	addr = prefix_touchar (prefix);
	bitlen = prefix->bitlen;

	while (node->bit < bitlen) {

	if (BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
#ifdef PATRICIA_DEBUG
		if (node->data)
				fprintf (stderr, "patricia_search_exact: take right %s/%d\n", 
					 prefix_toa (&node->prefix), node->prefix.bitlen);
		else
				fprintf (stderr, "patricia_search_exact: take right at %d\n", 
			 node->bit);
#endif /* PATRICIA_DEBUG */
		node = node->r;
	}
	else {
#ifdef PATRICIA_DEBUG
		if (node->data)
			fprintf (stderr, "patricia_search_exact: take left %s/%d\n", 
				 prefix_toa (&node->prefix), node->prefix.bitlen);
		else
				fprintf (stderr, "patricia_search_exact: take left at %d\n", 
			 node->bit);
#endif /* PATRICIA_DEBUG */
		node = node->l;
	}

	if (node == NULL)
		return (NULL);
	}

#ifdef PATRICIA_DEBUG
	if (&node->prefix)
		fprintf (stderr, "patricia_search_exact: stop at %s/%d\n", 
			 prefix_toa (&node->prefix), node->prefix.bitlen);
	else
		fprintf (stderr, "patricia_search_exact: stop at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
	if (node->bit > bitlen || node->data == NULL)
		return (NULL);
	assert (node->bit == bitlen);
	assert (node->bit == node->prefix.bitlen);
	if (comp_with_mask (prefix_tochar (&node->prefix), prefix_tochar (prefix),
			bitlen)) {
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_search_exact: found %s/%d\n", 
			 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
		return (node);
	}
	return (NULL);
}


/* if inclusive != 0, "best" may be the given prefix itself */
patricia_node_t *
patricia_search_best2 (patricia_tree_t *patricia, prefix_t *prefix, int inclusive)
{
	patricia_node_t *node;
	patricia_node_t *stack[PATRICIA_MAXBITS + 1];
	u_char *addr;
	u_int bitlen;
	int cnt = 0;

	assert (patricia);
	assert (prefix);
	assert (prefix->bitlen <= patricia->maxbits);

	if (patricia->head == NULL)
	return (NULL);

	node = patricia->head;
	addr = prefix_touchar (prefix);
	bitlen = prefix->bitlen;

	while (node->bit < bitlen) {

		if (node->data) {
#ifdef PATRICIA_DEBUG
			fprintf (stderr, "patricia_search_best: push %s/%d\n", 
				 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
			stack[cnt++] = node;
		}

		if (BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
#ifdef PATRICIA_DEBUG
			if (&node->prefix)
				fprintf (stderr, "patricia_search_best: take right %s/%d\n", 
					 prefix_toa (&node->prefix), node->prefix.bitlen);
			else
				fprintf (stderr, "patricia_search_best: take right at %d\n", 
					 node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->r;
		}
		else {
#ifdef PATRICIA_DEBUG
			if (&node->prefix)
				fprintf (stderr, "patricia_search_best: take left %s/%d\n", 
					 prefix_toa (&node->prefix), node->prefix.bitlen);
			else
				fprintf (stderr, "patricia_search_best: take left at %d\n", 
					 node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->l;
		}

		if (node == NULL)
			break;
	}

	if (inclusive && node && node->data && node->bit <= bitlen)
		stack[cnt++] = node;

#ifdef PATRICIA_DEBUG
	if (node == NULL)
		fprintf (stderr, "patricia_search_best: stop at null\n");
	else if (&node->prefix)
		fprintf (stderr, "patricia_search_best: stop at %s/%d\n", 
			 prefix_toa (&node->prefix), node->prefix.bitlen);
	else
		fprintf (stderr, "patricia_search_best: stop at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */

	if (cnt <= 0)
		return (NULL);

	while (--cnt >= 0) {
		node = stack[cnt];
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_search_best: pop %s/%d\n", 
			 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
		if (comp_with_mask (prefix_tochar (&node->prefix), 
				prefix_tochar (prefix),
				node->prefix.bitlen)) {
#ifdef PATRICIA_DEBUG
			fprintf (stderr, "patricia_search_best: found %s/%d\n", 
				 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
			return (node);
		}
	}
	return (NULL);
}


patricia_node_t *
patricia_search_best (patricia_tree_t *patricia, prefix_t *prefix)
{
	return (patricia_search_best2 (patricia, prefix, 1));
}


patricia_node_t *
patricia_lookup (patricia_tree_t *patricia, prefix_t *prefix)
{
	patricia_node_t *node, *new_node, *parent, *glue;
	u_char *addr, *test_addr;
	u_int bitlen, check_bit, differ_bit;
        u_int i;
	int j, r;

	assert (patricia);
	assert (prefix);
	assert (prefix->bitlen <= patricia->maxbits);

	if (patricia->head == NULL) {
		node = calloc(1, sizeof *node);
		node->bit = prefix->bitlen;
		node->prefix = *prefix;
		node->parent = NULL;
		node->l = node->r = NULL;
		node->data = NULL;
		patricia->head = node;
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_lookup: new_node #0 %s/%d (head)\n", 
			prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		patricia->num_active_node++;
		return (node);
	}

	addr = prefix_touchar (prefix);
	bitlen = prefix->bitlen;
	node = patricia->head;

	while (node->bit < bitlen || node->data == NULL) {

		if (node->bit < patricia->maxbits &&
			BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
			if (node->r == NULL)
				break;
#ifdef PATRICIA_DEBUG
			if (&node->prefix)
				fprintf (stderr, "patricia_lookup: take right %s/%d\n", 
					prefix_toa (&node->prefix), node->prefix.bitlen);
			else
				fprintf (stderr, "patricia_lookup: take right at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->r;
		}
		else {
			if (node->l == NULL)
				break;
#ifdef PATRICIA_DEBUG
			if (&node->prefix)
				fprintf (stderr, "patricia_lookup: take left %s/%d\n", 
					prefix_toa (&node->prefix), node->prefix.bitlen);
			else
				fprintf (stderr, "patricia_lookup: take left at %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
			node = node->l;
		}

		assert (node);
	}

	assert (node->data);
#ifdef PATRICIA_DEBUG
	fprintf (stderr, "patricia_lookup: stop at %s/%d\n", 
		 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */

	test_addr = prefix_touchar (&(node->prefix));
	/* find the first bit different */
	check_bit = (node->bit < bitlen)? node->bit: bitlen;
	differ_bit = 0;
	for (i = 0; i*8 < check_bit; i++) {
		if ((r = (addr[i] ^ test_addr[i])) == 0) {
			differ_bit = (i + 1) * 8;
			continue;
		}
		/* I know the better way, but for now */
		for (j = 0; j < 8; j++) {
			if (BIT_TEST (r, (0x80 >> j)))
				break;
		}
		/* must be found */
		assert (j < 8);
		differ_bit = i * 8 + j;
		break;
	}
	if (differ_bit > check_bit)
		differ_bit = check_bit;
#ifdef PATRICIA_DEBUG
	fprintf (stderr, "patricia_lookup: differ_bit %d\n", differ_bit);
#endif /* PATRICIA_DEBUG */

	parent = node->parent;
	while (parent && parent->bit >= differ_bit) {
		node = parent;
		parent = node->parent;
#ifdef PATRICIA_DEBUG
		if (&node->prefix)
			fprintf (stderr, "patricia_lookup: up to %s/%d\n", 
				 prefix_toa (&node->prefix), node->prefix.bitlen);
		else
			fprintf (stderr, "patricia_lookup: up to %d\n", node->bit);
#endif /* PATRICIA_DEBUG */
	}

	if (differ_bit == bitlen && node->bit == bitlen) {
		if (node->data) {
#ifdef PATRICIA_DEBUG 
			fprintf (stderr, "patricia_lookup: found %s/%d\n", 
				 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
			return (node);
		}
		node->prefix = *prefix;
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_lookup: new node #1 %s/%d (glue mod)\n",
			 prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		assert (node->data == NULL);
		return (node);
	}

	new_node = calloc(1, sizeof *new_node);
	new_node->bit = prefix->bitlen;
	new_node->prefix = *prefix;
	new_node->parent = NULL;
	new_node->l = new_node->r = NULL;
	new_node->data = NULL;
	patricia->num_active_node++;

	if (node->bit == differ_bit) {
		new_node->parent = node;
		if (node->bit < patricia->maxbits &&
			BIT_TEST (addr[node->bit >> 3], 0x80 >> (node->bit & 0x07))) {
			assert (node->r == NULL);
			node->r = new_node;
		}
		else {
			assert (node->l == NULL);
			node->l = new_node;
		}
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_lookup: new_node #2 %s/%d (child)\n", 
			 prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
		return (new_node);
	}

	if (bitlen == differ_bit) {
		if (bitlen < patricia->maxbits &&
			BIT_TEST (test_addr[bitlen >> 3], 0x80 >> (bitlen & 0x07))) {
			new_node->r = node;
		}
		else {
			new_node->l = node;
		}
		new_node->parent = node->parent;
		if (node->parent == NULL) {
			assert (patricia->head == node);
			patricia->head = new_node;
		}
		else if (node->parent->r == node) {
			node->parent->r = new_node;
		}
		else {
			node->parent->l = new_node;
		}
		node->parent = new_node;
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_lookup: new_node #3 %s/%d (parent)\n", 
			 prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
	}
	else {
		glue = calloc(1, sizeof *glue);
		glue->bit = differ_bit;
		memset(&glue->prefix, 0, sizeof(prefix_t));
		glue->parent = node->parent;
		glue->data = NULL;
		patricia->num_active_node++;
		if (differ_bit < patricia->maxbits &&
			BIT_TEST (addr[differ_bit >> 3], 0x80 >> (differ_bit & 0x07))) {
			glue->r = new_node;
			glue->l = node;
		}
		else {
			glue->r = node;
			glue->l = new_node;
		}
		new_node->parent = glue;

		if (node->parent == NULL) {
			assert (patricia->head == node);
			patricia->head = glue;
		}
		else if (node->parent->r == node) {
			node->parent->r = glue;
		}
		else {
			node->parent->l = glue;
		}
		node->parent = glue;
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_lookup: new_node #4 %s/%d (glue+node)\n", 
			 prefix_toa (prefix), prefix->bitlen);
#endif /* PATRICIA_DEBUG */
	}
	return (new_node);
}


void
patricia_remove (patricia_tree_t *patricia, patricia_node_t *node)
{
	patricia_node_t *parent, *child;

	assert (patricia);
	assert (node);

	if (node->r && node->l) {
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_remove: #0 %s/%d (r & l)\n", 
			 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
	
		/* Also I needed to clear data pointer -- masaki */
		node->data = NULL;
		return;
	}

	if (node->r == NULL && node->l == NULL) {
#ifdef PATRICIA_DEBUG
		fprintf (stderr, "patricia_remove: #1 %s/%d (!r & !l)\n", 
			 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
		parent = node->parent;
		Delete (node);
		patricia->num_active_node--;

		if (parent == NULL) {
			assert (patricia->head == node);
			patricia->head = NULL;
			return;
		}

		if (parent->r == node) {
			parent->r = NULL;
			child = parent->l;
		}
		else {
			assert (parent->l == node);
			parent->l = NULL;
			child = parent->r;
		}

		if (parent->data)
			return;

		/* we need to remove parent too */

		if (parent->parent == NULL) {
			assert (patricia->head == parent);
			patricia->head = child;
		}
		else if (parent->parent->r == parent) {
			parent->parent->r = child;
		}
		else {
			assert (parent->parent->l == parent);
			parent->parent->l = child;
		}
		child->parent = parent->parent;
		Delete (parent);
		patricia->num_active_node--;
		return;
	}

#ifdef PATRICIA_DEBUG
	fprintf (stderr, "patricia_remove: #2 %s/%d (r ^ l)\n", 
		 prefix_toa (&node->prefix), node->prefix.bitlen);
#endif /* PATRICIA_DEBUG */
	if (node->r) {
		child = node->r;
	}
	else {
		assert (node->l);
		child = node->l;
	}
	parent = node->parent;
	child->parent = parent;

	Delete (node);
	patricia->num_active_node--;

	if (parent == NULL) {
		assert (patricia->head == node);
		patricia->head = child;
		return;
	}

	if (parent->r == node) {
		parent->r = child;
	}
	else {
		assert (parent->l == node);
		parent->l = child;
	}
}
