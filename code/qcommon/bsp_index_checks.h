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
 * sides, a side's plane and a surface's vertices and indexes. The collision
 * loader and every renderer's world loader take them from here, so an index
 * they both read meets the same rule in each. Each is written so that no sum
 * can overflow. Every map of every installed retail game passes all of them
 * (docs/fntech3/2026-09-27-bsp-index-checks.md).
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

#endif
