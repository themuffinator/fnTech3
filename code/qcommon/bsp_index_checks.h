/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_QCOMMON_BSP_INDEX_CHECKS_H
#define FNTECH3_QCOMMON_BSP_INDEX_CHECKS_H

/*
 * The checks a map's loaders make before following a number one lump gives
 * into another: a node's plane and children, a leaf's area and its ranges of
 * leaf brushes and leaf surfaces, a model's brushes and surfaces, a brush's
 * sides, a side's plane and a surface's vertices and indexes, and the shape of
 * the tree the nodes make. The collision loader and every renderer's world
 * loader take them from here, so an index they both read meets the same rule
 * in each. Each is written so that no sum can overflow. Every map of every
 * installed retail game passes all of them
 * (docs/fntech3/2026-09-27-bsp-index-checks.md,
 * docs/fntech3/2026-09-27-bsp-node-trees.md).
 */

/* `count` records from `first` lie inside a table of `total`. */
static inline qboolean BSP_RangeInside( int first, int count, int total ) {
	return first >= 0 && count >= 0 && first <= total && count <= total - first ? qtrue : qfalse;
}

/* `index` names one of the `total` records of a table. */
static inline qboolean BSP_IndexInside( int index, int total ) {
	return index >= 0 && index < total ? qtrue : qfalse;
}

/* A node's child names one of the map's nodes, or one of its leafs as
 * -1 - leaf. */
static inline qboolean BSP_ChildInside( int child, int numNodes, int numLeafs ) {
	return child >= 0 ? BSP_IndexInside( child, numNodes ) : BSP_IndexInside( -1 - child, numLeafs );
}

/* A leaf's area is -1, for none, or one of MAX_MAP_AREAS: a snapshot's and a
 * scene's area bits hold no more (MAX_MAP_AREA_BYTES). */
static inline qboolean BSP_LeafAreaValid( int area ) {
	return area >= -1 && area < MAX_MAP_AREAS ? qtrue : qfalse;
}

/* A leaf's cluster is -1, for none, or a row of the visibility table, whose
 * size the table itself gives. */
static inline qboolean BSP_LeafClusterValid( int cluster ) {
	return cluster >= -1 ? qtrue : qfalse;
}

/* The most nodes a path from node 0 may pass through. Each descent of the
 * tree -- the collision traces and leaf lists, the renderers' R_SetParent and
 * their world, light and mark walks -- recurses, or keeps a stack, as deep as
 * the path it follows; this deep, the deepest recursion of the Windows x86
 * build takes 152 KB of its 1 MB main-thread stack. The IHV translator
 * (bsp_v43.h) refuses deeper trees as well, and its model lighting keeps a
 * stack this deep. The deepest installed tree is 137 nodes deep. */
#define BSP_MAX_NODE_DEPTH 2048
#define BSP_TREE_TEXT_( value ) #value
#define BSP_TREE_TEXT( value ) BSP_TREE_TEXT_( value )

/* What BSP_NodeTreeFault finds walking a map's nodes from node 0. */
typedef enum {
	BSP_TREE_VALID,		/* each node it reaches is reached once, no path too deep */
	BSP_TREE_CYCLE,		/* a child is its own node, or a node above it */
	BSP_TREE_SHARED,	/* a child is already below another node */
	BSP_TREE_TOO_DEEP,	/* a child lies more than BSP_MAX_NODE_DEPTH nodes down */
	BSP_TREE_OUTSIDE	/* a child is none of the map's nodes or leafs */
} bspTreeFault_t;

/* A node on the path BSP_NodeTreeFault walks, and the child it takes next. */
typedef struct {
	int node;
	int side;
} bspTreeStep_t;

/* The bytes of scratch BSP_NodeTreeFault walks in: its path, and a bit for
 * each node. */
static inline int BSP_NodeTreeScratchSize( int numNodes ) {
	return (int)sizeof( bspTreeStep_t ) * BSP_MAX_NODE_DEPTH
		+ ( numNodes > 0 ? numNodes / 32 + 1 : 1 ) * (int)sizeof( unsigned int );
}

/* Walks the `numNodes` nodes from node 0 as every descent does, but without
 * recursion and in `scratch` alone, and returns the first fault it meets.
 * Each node it reaches must be reached once. A child that leads back to its
 * own node or to one above it would never let a descent end; a subtree that
 * two nodes share is walked twice by a descent of both, and twice again for
 * each share below it. No path may pass through more than BSP_MAX_NODE_DEPTH
 * nodes. A leaf ends every descent, so two nodes may name one, and no descent
 * follows a node that node 0 does not reach, so such a node is left alone; no
 * installed map has either. On a fault, `*node` and `*child` are the node and
 * the child that break the tree. */
static inline bspTreeFault_t BSP_NodeTreeFault( const dnode_t *nodes, int numNodes, int numLeafs, void *scratch,
	int *node, int *child ) {
	bspTreeStep_t *path = (bspTreeStep_t *)scratch;
	unsigned int *reached = (unsigned int *)( path + BSP_MAX_NODE_DEPTH );
	int depth, next, i;

	*node = *child = -1;
	if ( numNodes <= 0 ) {
		return BSP_TREE_VALID;
	}
	memset( reached, 0, (size_t)( numNodes / 32 + 1 ) * sizeof( *reached ) );
	reached[0] = 1;
	path[0].node = 0;
	path[0].side = 0;
	depth = 1;
	while ( depth > 0 ) {
		bspTreeStep_t *step = &path[depth - 1];
		bspTreeFault_t fault = BSP_TREE_VALID;

		if ( step->side == 2 ) {
			depth--;	/* both children walked */
			continue;
		}
		next = LittleLong( nodes[step->node].children[step->side++] );
		if ( !BSP_ChildInside( next, numNodes, numLeafs ) ) {
			fault = BSP_TREE_OUTSIDE;
		} else if ( next < 0 ) {
			continue;	/* a leaf ends a descent */
		} else if ( reached[next >> 5] & ( 1u << ( next & 31 ) ) ) {
			/* reached before: on this path, a cycle; off it, a share */
			fault = BSP_TREE_SHARED;
			for ( i = 0; i < depth; i++ ) {
				if ( path[i].node == next ) {
					fault = BSP_TREE_CYCLE;
				}
			}
		} else if ( depth == BSP_MAX_NODE_DEPTH ) {
			fault = BSP_TREE_TOO_DEEP;
		}
		if ( fault != BSP_TREE_VALID ) {
			*node = step->node;
			*child = next;
			return fault;
		}
		reached[next >> 5] |= 1u << ( next & 31 );
		path[depth].node = next;
		path[depth].side = 0;
		depth++;
	}
	return BSP_TREE_VALID;
}

/* How a loader words a fault: "child <child> of node <node> <text>". */
static inline const char *BSP_NodeTreeFaultText( bspTreeFault_t fault ) {
	switch ( fault ) {
	case BSP_TREE_CYCLE:
		return "leads back to that node or one above it";
	case BSP_TREE_SHARED:
		return "is already below another node";
	case BSP_TREE_TOO_DEEP:
		return "is more than " BSP_TREE_TEXT( BSP_MAX_NODE_DEPTH ) " nodes deep";
	case BSP_TREE_OUTSIDE:
		return "is none of the map's nodes or leafs";
	default:
		return "is sound";
	}
}

#endif
