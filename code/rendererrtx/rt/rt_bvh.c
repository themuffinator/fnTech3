/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Path tracer BVHs for the compute backend

Without VK_KHR_ray_tracing_pipeline the path tracer traverses bounding volume
hierarchies of its own (shaders/rt_trace_sw.glsl). Each instance's triangles
get a binary BVH over their bounds, split by the surface area heuristic over
twelve bins of the centroids along their widest axis; leaves hold up to four
triangles where splitting pays. A node is 32 bytes (rtBvhNode_t): an interior
node's children are adjacent, so it names only the left one. Leaves list
their triangles' corners (rtBvhTri_t) so traversal reads one record per
candidate, and each record names its triangle in the index buffer for the
hit's shading. Trees stay shallower than the traversal's stack
(RT_SW_STACK): past RT_BVH_MAX_DEPTH a node is split at its middle, which
halves it, until it fits a leaf.

Included by vk.c.
===========================================================================
*/

#include <float.h>

#define RT_BVH_BINS			12
#define RT_BVH_LEAF_SIZE	4
#define RT_BVH_MAX_LEAF		64		// a leaf may grow to this where splitting stops paying
#define RT_BVH_MAX_DEPTH	40		// then middle splits: 2^22 triangles stay below RT_SW_STACK (64)
#define RT_BVH_MAX_TRIS		( 1u << 22 )

typedef struct {
	rtBvhNode_t		*nodes;
	rtBvhTri_t		*tris;
	uint32_t		numNodes;
	uint32_t		numTris;
	uint32_t		nodeCapacity;
	uint32_t		triCapacity;
	// build scratch, per triangle of the tree being built
	float			*bounds;		// min xyz, max xyz
	float			*centroids;
	uint32_t		*ids;
	uint32_t		scratchCapacity;
} rtBvh_t;

typedef struct {
	uint32_t	node;			// the node, relative to the tree's first
	uint32_t	start, end;		// its triangles, positions in ids[]
	uint32_t	depth;
} rtBvhTask_t;

static void RT_BvhFree( rtBvh_t *bvh )
{
	if ( bvh->nodes ) RT_HostFree( bvh->nodes );
	if ( bvh->tris ) RT_HostFree( bvh->tris );
	if ( bvh->bounds ) RT_HostFree( bvh->bounds );
	if ( bvh->centroids ) RT_HostFree( bvh->centroids );
	if ( bvh->ids ) RT_HostFree( bvh->ids );
	Com_Memset( bvh, 0, sizeof( *bvh ) );
}

static void RT_BvhReset( rtBvh_t *bvh )
{
	bvh->numNodes = 0;
	bvh->numTris = 0;
}

// Grow *array (elements of size bytes, *capacity of them, the first used
// kept) to hold at least count.
static qboolean RT_BvhGrow( void **array, uint32_t *capacity, uint32_t used, uint32_t count, size_t size )
{
	uint32_t newCapacity;
	void *grown;

	if ( count <= *capacity ) {
		return qtrue;
	}
	newCapacity = *capacity ? *capacity : 1024;
	while ( newCapacity < count ) {
		if ( newCapacity > ( 1u << 28 ) ) {
			return qfalse;
		}
		newCapacity *= 2;
	}
	if ( (uint64_t)newCapacity * size > (uint64_t)INT_MAX ) {
		return qfalse;
	}
	grown = RT_HostAlloc( (int)( newCapacity * size ) );
	if ( !grown ) {
		return qfalse;
	}
	if ( *array ) {
		if ( used ) {
			Com_Memcpy( grown, *array, (size_t)used * size );
		}
		RT_HostFree( *array );
	}
	*array = grown;
	*capacity = newCapacity;
	return qtrue;
}

static float RT_BvhArea( const float *bmin, const float *bmax )
{
	const float dx = bmax[0] - bmin[0];
	const float dy = bmax[1] - bmin[1];
	const float dz = bmax[2] - bmin[2];
	if ( !( dx >= 0.0f ) || !( dy >= 0.0f ) || !( dz >= 0.0f ) ) {
		return 0.0f;
	}
	return 2.0f * ( dx * dy + dy * dz + dz * dx );
}

static void RT_BvhClearBounds( float *bmin, float *bmax )
{
	bmin[0] = bmin[1] = bmin[2] = FLT_MAX;
	bmax[0] = bmax[1] = bmax[2] = -FLT_MAX;
}

// box: min xyz, max xyz
static void RT_BvhAddBox( float *bmin, float *bmax, const float *box )
{
	int i;
	for ( i = 0; i < 3; i++ ) {
		bmin[i] = MIN( bmin[i], box[i] );
		bmax[i] = MAX( bmax[i], box[3 + i] );
	}
}

static int RT_BvhBin( float value, float lo, float scale )
{
	const int bin = (int)( ( value - lo ) * scale );
	return bin < 0 ? 0 : bin >= RT_BVH_BINS ? RT_BVH_BINS - 1 : bin;
}

/*
The cheapest surface area heuristic split of ids[start, end) along axis, as
the last bin of its left side, or -1 when every plane leaves a side empty.
*cost receives that split's summed area-weighted triangle counts.
*/
static int RT_BvhBestSplit( const rtBvh_t *bvh, uint32_t start, uint32_t end, int axis, float lo, float scale,
	float *cost )
{
	float bins[RT_BVH_BINS][6];
	uint32_t binCount[RT_BVH_BINS];
	float rightArea[RT_BVH_BINS];
	uint32_t rightCount[RT_BVH_BINS];
	float leftMin[3], leftMax[3], rightMin[3], rightMax[3];
	uint32_t leftCount = 0, count = 0;
	int best = -1;
	uint32_t i;
	int b;

	for ( b = 0; b < RT_BVH_BINS; b++ ) {
		RT_BvhClearBounds( &bins[b][0], &bins[b][3] );
		binCount[b] = 0;
	}
	for ( i = start; i < end; i++ ) {
		const uint32_t id = bvh->ids[i];
		const int bin = RT_BvhBin( bvh->centroids[id * 3 + axis], lo, scale );
		binCount[bin]++;
		RT_BvhAddBox( &bins[bin][0], &bins[bin][3], &bvh->bounds[id * 6] );
	}

	// sweep from the right, then evaluate each plane from the left
	RT_BvhClearBounds( rightMin, rightMax );
	for ( b = RT_BVH_BINS - 1; b > 0; b-- ) {
		if ( binCount[b] ) {
			RT_BvhAddBox( rightMin, rightMax, bins[b] );
		}
		count += binCount[b];
		rightCount[b - 1] = count;
		rightArea[b - 1] = RT_BvhArea( rightMin, rightMax );
	}
	*cost = FLT_MAX;
	RT_BvhClearBounds( leftMin, leftMax );
	for ( b = 0; b < RT_BVH_BINS - 1; b++ ) {
		float splitCost;
		if ( binCount[b] ) {
			RT_BvhAddBox( leftMin, leftMax, bins[b] );
		}
		leftCount += binCount[b];
		if ( leftCount == 0 || rightCount[b] == 0 ) {
			continue;
		}
		splitCost = RT_BvhArea( leftMin, leftMax ) * (float)leftCount + rightArea[b] * (float)rightCount[b];
		if ( splitCost < *cost ) {
			*cost = splitCost;
			best = b;
		}
	}
	return best;
}

/*
Append a BVH over count triangles to bvh: triangle t has corners
indices[t * 3 .. t * 3 + 2] in vertices and is named primBias + t in its leaf
record. *nodeOffset and *triOffset receive where the tree starts; node
indices and leaf ranges inside it are relative to them. A triangle with a
corner outside the vertex array gets an inverted box and a degenerate
record, so no ray meets it.
*/
static qboolean RT_BvhAppend( rtBvh_t *bvh, const rtxRtPackedVertex_t *vertices, uint32_t numVertices,
	const uint32_t *indices, uint32_t count, uint32_t primBias, uint32_t *nodeOffset, uint32_t *triOffset )
{
	rtBvhTask_t tasks[RT_BVH_MAX_DEPTH * 2 + 32];
	uint32_t numTasks;
	uint32_t localNodes = 1;
	uint32_t i;

	*nodeOffset = bvh->numNodes;
	*triOffset = bvh->numTris;
	if ( count == 0 ) {
		return qfalse;
	}
	if ( count > RT_BVH_MAX_TRIS ||
		!RT_BvhGrow( (void **)&bvh->nodes, &bvh->nodeCapacity, bvh->numNodes, bvh->numNodes + 2 * count, sizeof( rtBvhNode_t ) ) ||
		!RT_BvhGrow( (void **)&bvh->tris, &bvh->triCapacity, bvh->numTris, bvh->numTris + count, sizeof( rtBvhTri_t ) ) ) {
		ri.Printf( PRINT_WARNING, "RTX RT: no memory for a BVH over %u triangles\n", count );
		return qfalse;
	}
	if ( count > bvh->scratchCapacity ) {
		uint32_t capacity = bvh->scratchCapacity;
		uint32_t unused;
		if ( !RT_BvhGrow( (void **)&bvh->bounds, &capacity, 0, count, 6 * sizeof( float ) ) ) {
			return qfalse;
		}
		unused = bvh->scratchCapacity;
		if ( !RT_BvhGrow( (void **)&bvh->centroids, &unused, 0, count, 3 * sizeof( float ) ) ) {
			return qfalse;
		}
		unused = bvh->scratchCapacity;
		if ( !RT_BvhGrow( (void **)&bvh->ids, &unused, 0, count, sizeof( uint32_t ) ) ) {
			return qfalse;
		}
		bvh->scratchCapacity = capacity;
	}

	for ( i = 0; i < count; i++ ) {
		float *box = &bvh->bounds[i * 6];
		float *centroid = &bvh->centroids[i * 3];
		int v, k;

		RT_BvhClearBounds( box, box + 3 );
		for ( v = 0; v < 3; v++ ) {
			const uint32_t index = indices[i * 3 + v];
			if ( index >= numVertices ) {
				break;
			}
			for ( k = 0; k < 3; k++ ) {
				box[k] = MIN( box[k], vertices[index].xyz[k] );
				box[3 + k] = MAX( box[3 + k], vertices[index].xyz[k] );
			}
		}
		if ( v < 3 ) {
			// inverted: the slab test never enters it
			box[0] = box[1] = box[2] = 1.0f;
			box[3] = box[4] = box[5] = -1.0f;
			centroid[0] = centroid[1] = centroid[2] = 0.0f;
		} else {
			for ( k = 0; k < 3; k++ ) {
				centroid[k] = 0.5f * ( box[k] + box[3 + k] );
			}
		}
		bvh->ids[i] = i;
	}

	tasks[0].node = 0;
	tasks[0].start = 0;
	tasks[0].end = count;
	tasks[0].depth = 0;
	numTasks = 1;

	while ( numTasks > 0 ) {
		const rtBvhTask_t task = tasks[--numTasks];
		rtBvhNode_t *node = &bvh->nodes[ *nodeOffset + task.node ];
		const uint32_t n = task.end - task.start;
		float cmin[3], cmax[3];
		float extent, scale = 0.0f, bestCost = FLT_MAX;
		int axis, bestBin = -1;
		uint32_t mid;

		RT_BvhClearBounds( node->bmin, node->bmax );
		RT_BvhClearBounds( cmin, cmax );
		for ( i = task.start; i < task.end; i++ ) {
			const uint32_t id = bvh->ids[i];
			const float *c = &bvh->centroids[id * 3];
			RT_BvhAddBox( node->bmin, node->bmax, &bvh->bounds[id * 6] );
			cmin[0] = MIN( cmin[0], c[0] ); cmax[0] = MAX( cmax[0], c[0] );
			cmin[1] = MIN( cmin[1], c[1] ); cmax[1] = MAX( cmax[1], c[1] );
			cmin[2] = MIN( cmin[2], c[2] ); cmax[2] = MAX( cmax[2], c[2] );
		}

		if ( n <= RT_BVH_LEAF_SIZE || ( task.depth >= RT_BVH_MAX_DEPTH && n <= RT_BVH_MAX_LEAF ) ||
			numTasks + 2 > ARRAY_LEN( tasks ) ) {
			node->first = task.start;
			node->count = n;
			continue;
		}

		axis = 0;
		if ( cmax[1] - cmin[1] > cmax[axis] - cmin[axis] ) axis = 1;
		if ( cmax[2] - cmin[2] > cmax[axis] - cmin[axis] ) axis = 2;
		extent = cmax[axis] - cmin[axis];
		if ( task.depth < RT_BVH_MAX_DEPTH && extent > 1e-6f * MAX( 1.0f, fabsf( cmin[axis] ) ) ) {
			scale = (float)RT_BVH_BINS / extent;
			bestBin = RT_BvhBestSplit( bvh, task.start, task.end, axis, cmin[axis], scale, &bestCost );
		}

		// a leaf when no split beats testing every triangle here
		if ( n <= RT_BVH_MAX_LEAF && bestBin >= 0 ) {
			const float nodeArea = RT_BvhArea( node->bmin, node->bmax );
			if ( nodeArea > 0.0f && 1.0f + bestCost / nodeArea >= (float)n ) {
				node->first = task.start;
				node->count = n;
				continue;
			}
		}

		mid = task.start;
		if ( bestBin >= 0 ) {
			uint32_t lo = task.start;
			uint32_t hi = task.end;
			while ( lo < hi ) {
				const uint32_t id = bvh->ids[lo];
				if ( RT_BvhBin( bvh->centroids[id * 3 + axis], cmin[axis], scale ) <= bestBin ) {
					lo++;
				} else {
					hi--;
					bvh->ids[lo] = bvh->ids[hi];
					bvh->ids[hi] = id;
				}
			}
			mid = lo;
		}
		if ( mid <= task.start || mid >= task.end ) {
			// too deep, or the centroids coincide: halve the range
			mid = task.start + n / 2;
		}

		node->first = localNodes;
		node->count = 0;
		tasks[numTasks].node = localNodes + 1;
		tasks[numTasks].start = mid;
		tasks[numTasks].end = task.end;
		tasks[numTasks].depth = task.depth + 1;
		numTasks++;
		tasks[numTasks].node = localNodes;
		tasks[numTasks].start = task.start;
		tasks[numTasks].end = mid;
		tasks[numTasks].depth = task.depth + 1;
		numTasks++;
		localNodes += 2;
	}

	for ( i = 0; i < count; i++ ) {
		const uint32_t id = bvh->ids[i];
		rtBvhTri_t *tri = &bvh->tris[ *triOffset + i ];
		const uint32_t i0 = indices[id * 3 + 0];
		const uint32_t i1 = indices[id * 3 + 1];
		const uint32_t i2 = indices[id * 3 + 2];
		const uint32_t prim = primBias + id;

		Com_Memset( tri, 0, sizeof( *tri ) );
		if ( i0 < numVertices && i1 < numVertices && i2 < numVertices ) {
			VectorCopy( vertices[i0].xyz, tri->v0 );
			VectorCopy( vertices[i1].xyz, tri->v1 );
			VectorCopy( vertices[i2].xyz, tri->v2 );
		}
		Com_Memcpy( &tri->v0[3], &prim, sizeof( prim ) );
	}
	bvh->numNodes += localNodes;
	bvh->numTris += count;
	return qtrue;
}

// Update a single cached tree with unchanged triangle connectivity. Leaves
// retain their triangle order; child bounds are ready before their parent
// because the builder always allocates children after it. The caller checks
// the index array exactly before choosing refit instead of build.
static qboolean RT_BvhRefit( rtBvh_t *bvh, const rtxRtPackedVertex_t *vertices, uint32_t numVertices,
	const uint32_t *indices, uint32_t count, uint32_t oldBias, uint32_t newBias )
{
	uint32_t i;
	if ( !bvh->numNodes || bvh->numTris != count ) return qfalse;
	for ( i = 0; i < count; i++ ) {
		rtBvhTri_t *tri = &bvh->tris[i];
		uint32_t prim, local, v;
		float *corners[3] = { tri->v0, tri->v1, tri->v2 };
		Com_Memcpy( &prim, &tri->v0[3], sizeof( prim ) );
		if ( prim < oldBias || prim - oldBias >= count ) return qfalse;
		local = prim - oldBias;
		for ( v = 0; v < 3; v++ ) {
			const uint32_t index = indices[local * 3 + v];
			int k;
			if ( index >= numVertices ) return qfalse;
			for ( k = 0; k < 3; k++ ) {
				const float x = vertices[index].xyz[k];
				if ( !( x >= -FLT_MAX && x <= FLT_MAX ) ) return qfalse;
				corners[v][k] = x;
			}
		}
		prim = newBias + local;
		Com_Memcpy( &tri->v0[3], &prim, sizeof( prim ) );
	}
	for ( i = bvh->numNodes; i-- > 0; ) {
		rtBvhNode_t *n = &bvh->nodes[i];
		uint32_t j;
		int k;
		RT_BvhClearBounds( n->bmin, n->bmax );
		if ( n->count ) {
			if ( n->first > count || n->count > count - n->first ) return qfalse;
			for ( j = n->first; j < n->first + n->count; j++ ) {
				const rtBvhTri_t *tri = &bvh->tris[j];
				for ( k = 0; k < 3; k++ ) {
					n->bmin[k] = MIN( n->bmin[k], MIN( tri->v0[k], MIN( tri->v1[k], tri->v2[k] ) ) );
					n->bmax[k] = MAX( n->bmax[k], MAX( tri->v0[k], MAX( tri->v1[k], tri->v2[k] ) ) );
				}
			}
		} else {
			if ( n->first <= i || n->first >= bvh->numNodes - 1 ) return qfalse;
			for ( k = 0; k < 3; k++ ) {
				n->bmin[k] = MIN( bvh->nodes[n->first].bmin[k], bvh->nodes[n->first + 1].bmin[k] );
				n->bmax[k] = MAX( bvh->nodes[n->first].bmax[k], bvh->nodes[n->first + 1].bmax[k] );
			}
		}
	}
	return qtrue;
}

// Expected traversal work relative to the root area. Pure translation and
// uniform scale leave this unchanged; deformation that makes child boxes
// overlap substantially triggers a rebuild in the caller.
static float RT_BvhCost( const rtBvh_t *bvh )
{
	double cost = 0.0;
	uint32_t i;
	float rootArea;
	if ( !bvh->numNodes ) return 0.0f;
	rootArea = RT_BvhArea( bvh->nodes[0].bmin, bvh->nodes[0].bmax );
	for ( i = 0; i < bvh->numNodes; i++ ) {
		const rtBvhNode_t *n = &bvh->nodes[i];
		cost += (double)RT_BvhArea( n->bmin, n->bmax ) * ( n->count ? n->count : 1u );
	}
	return rootArea > 0.0f ? (float)( cost / rootArea ) : 0.0f;
}
