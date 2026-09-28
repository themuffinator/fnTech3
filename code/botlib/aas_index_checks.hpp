/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_BOTLIB_AAS_INDEX_CHECKS_HPP
#define FNTECH3_BOTLIB_AAS_INDEX_CHECKS_HPP

/*
 * The checks a bot navigation file's loaders make before anything follows a
 * number one of its tables gives into another: the tree of nodes every point
 * query and trace descends, the geometry the movement and reachability code
 * walks (areas, faces, edges and vertexes), and the routing tables (area
 * settings, reachabilities, portals and clusters) whose numbers index the
 * routing caches. Quake III's loader, for the files of Quake III, Team Arena,
 * Quake Live and Elite Force, and RTCW's loader take them from here, after
 * each has decoded a file into the engine's tables and before either marks it
 * loaded. Each is written so that no sum can overflow. Every navigation file
 * of every installed retail game passes all of them
 * (docs/fntech3/2026-09-28-aas-index-checks.md).
 *
 * Three records are dummies. Node 0 stands for solid, which a child names as
 * 0, so no descent reads it; face 0 and edge 0 cannot be named by the signed
 * face and edge indexes, whose zero carries no sign, so their contents never
 * matter. Area 0, portal 0 and cluster 0 are dummies too, but routing reads
 * them (every table is walked from 0, and an area outside every cluster is
 * routed through portal 0), so they are checked like the rest.
 */

#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

/* `count` records from `first` lie inside a table of `total`. */
static inline bool AAS_RangeInside( int first, int count, int total ) {
	return first >= 0 && count >= 0 && first <= total && count <= total - first;
}

/* `index` names one of the `total` records of a table. */
static inline bool AAS_IndexInside( int index, int total ) {
	return index >= 0 && index < total;
}

/* A face or edge index names record |index|, turned round when negative.
 * Zero has no sign to carry, so record 0 is never named. */
static inline bool AAS_SignedIndexInside( int index, int total ) {
	return index != 0 && index != INT_MIN && AAS_IndexInside( index < 0 ? -index : index, total );
}

/* A node's child names a node as a positive number, area -child as a
 * negative one, or solid as 0. */
static inline bool AAS_ChildInside( int child, int numNodes, int numAreas ) {
	return child >= 0 ? child < numNodes : child != INT_MIN && -child < numAreas;
}

/* What AAS_NodeTreeFault finds walking the nodes from node 1. */
typedef enum {
	AAS_TREE_VALID,		/* each node it reaches is reached once */
	AAS_TREE_CYCLE,		/* a child is its own node, or a node above it */
	AAS_TREE_SHARED,	/* a child is already below another node */
	AAS_TREE_OUTSIDE	/* a child is none of the file's nodes or areas */
} aasTreeFault_t;

/* A node on the path AAS_NodeTreeFault walks, and the child it takes next. */
typedef struct {
	int node;
	int side;
} aasTreeStep_t;

/* The bytes of scratch AAS_NodeTreeFault walks in: a path as long as the
 * nodes, and a bit for each node. */
static inline size_t AAS_NodeTreeScratchSize( int numNodes ) {
	const size_t nodes = numNodes > 0 ? (size_t)numNodes : 0;
	return sizeof( aasTreeStep_t ) * nodes + ( nodes / 32 + 1 ) * sizeof( unsigned int );
}

/* Walks the `numNodes` nodes from node 1 as every descent does, but in
 * `scratch` alone, and returns the first fault it meets. Each node it reaches
 * must be reached once: a child that leads back to its own node or to one
 * above it never lets a descent end, and a subtree two nodes share is walked
 * twice by any descent of both, and twice again for each share below it. An
 * area ends every descent, and several leaves naming one area are normal,
 * since the planes of the tree may cut an area into several pieces. A node
 * that node 1 does not reach is never followed and is left alone. No depth
 * is refused: the point query loops, and each trace and entity link keeps a
 * stack of its own that stops, with an error, when it is full. On a fault,
 * `*node` and `*child` are the node and the child that break the tree. */
static inline aasTreeFault_t AAS_NodeTreeFault( const aas_node_t *nodes, int numNodes, int numAreas, void *scratch,
	int *node, int *child ) {
	aasTreeStep_t *path = (aasTreeStep_t *)scratch;
	unsigned int *reached;
	int depth, next, i;

	*node = *child = -1;
	if ( numNodes < 2 ) {
		return AAS_TREE_VALID;	/* no tree to walk: the loaders refuse this first */
	}
	reached = (unsigned int *)( path + numNodes );
	memset( reached, 0, ( (size_t)numNodes / 32 + 1 ) * sizeof( *reached ) );
	reached[0] = 1u << 1;
	path[0].node = 1;
	path[0].side = 0;
	depth = 1;
	while ( depth > 0 ) {
		aasTreeStep_t *step = &path[depth - 1];
		aasTreeFault_t fault = AAS_TREE_VALID;

		if ( step->side == 2 ) {
			depth--;	/* both children walked */
			continue;
		}
		next = nodes[step->node].children[step->side++];
		if ( !AAS_ChildInside( next, numNodes, numAreas ) ) {
			fault = AAS_TREE_OUTSIDE;
		} else if ( next <= 0 ) {
			continue;	/* solid, or an area, ends a descent */
		} else if ( reached[next >> 5] & ( 1u << ( next & 31 ) ) ) {
			/* reached before: on this path, a cycle; off it, a share */
			fault = AAS_TREE_SHARED;
			for ( i = 0; i < depth; i++ ) {
				if ( path[i].node == next ) {
					fault = AAS_TREE_CYCLE;
				}
			}
		}
		if ( fault != AAS_TREE_VALID ) {
			*node = step->node;
			*child = next;
			return fault;
		}
		/* every node on the path was reached once, so it is never longer than the nodes */
		reached[next >> 5] |= 1u << ( next & 31 );
		path[depth].node = next;
		path[depth].side = 0;
		depth++;
	}
	return AAS_TREE_VALID;
}

/* The most bytes of travel times Quake III's routing may lay out for a file:
 * for each area, a row for each reachability that leaves it with a time for
 * each link that enters it (AAS_CalculateAreaTravelTimes). It sizes them in
 * an int, which reachability ranges that overlap could make wrap long before
 * any allocation fails. The largest installed file needs 1,056,244 bytes,
 * 1,530,632 with 64-bit pointers; this is over forty times either. */
#define AAS_MAX_TRAVEL_TIME_BYTES ( 64 * 1024 * 1024 )

/* Quake III's routing reverses at most this many reachabilities of each area
 * (AAS_CreateReversedReachability). */
#define AAS_MAX_REVERSED_REACHABILITIES 128

/* What AAS_IndexFault finds, in the order it looks. */
typedef enum {
	AAS_INDEX_VALID,
	AAS_INDEX_NO_ROOT,
	AAS_INDEX_PLANE_PAIR,
	AAS_INDEX_SETTINGS_COUNT,
	AAS_INDEX_NODE_PLANE,
	AAS_INDEX_NODE_CHILD,
	AAS_INDEX_TREE_CYCLE,
	AAS_INDEX_TREE_SHARED,
	AAS_INDEX_AREA_FACES,
	AAS_INDEX_FACE_INDEX,
	AAS_INDEX_FACE_PLANE,
	AAS_INDEX_FACE_EDGES,
	AAS_INDEX_FACE_AREA,
	AAS_INDEX_EDGE_INDEX,
	AAS_INDEX_EDGE_VERTEX,
	AAS_INDEX_CLUSTER_COUNTS,
	AAS_INDEX_CLUSTER_PORTALS,
	AAS_INDEX_PORTAL_INDEX,
	AAS_INDEX_PORTAL_AREA,
	AAS_INDEX_PORTAL_CLUSTER,
	AAS_INDEX_PORTAL_NUMBER,
	AAS_INDEX_AREA_REACHABILITIES,
	AAS_INDEX_AREA_CLUSTER,
	AAS_INDEX_AREA_NUMBER,
	AAS_INDEX_AREA_PORTAL,
	AAS_INDEX_AREA_UNCLUSTERED,
	AAS_INDEX_PORTAL_OWNER,
	AAS_INDEX_PORTAL_BORDER,
	AAS_INDEX_REACH_AREA,
	AAS_INDEX_CLUSTER_NUMBERS,
	AAS_INDEX_CLUSTER_NUMBER_SHARED,
	AAS_INDEX_REVERSED_LINKS,
	AAS_INDEX_TRAVEL_TIMES
} aasIndexFault_t;

#if defined( __GNUC__ ) || defined( __clang__ )
static inline aasIndexFault_t AAS_IndexRefusal( aasIndexFault_t fault, char *text, size_t textSize,
	const char *format, ... ) __attribute__(( format( printf, 4, 5 ) ));
#endif

/* Words `fault` into `text`, as "node 12 has a bad plane", and returns it. */
static inline aasIndexFault_t AAS_IndexRefusal( aasIndexFault_t fault, char *text, size_t textSize,
	const char *format, ... ) {
	va_list args;

	if ( text && textSize ) {
		va_start( args, format );
		vsnprintf( text, textSize, format, args );
		va_end( args );
	}
	return fault;
}

/* The bytes of scratch AAS_IndexFault works in, for tables of the given
 * sizes: the tree walk's, a bit for every number clusters may hold (one for
 * each area and two for each portal) with each cluster's first number, or a
 * count of links into each area, whichever is largest. */
template <class World>
static inline size_t AAS_IndexScratchSize( const World &world ) {
	const size_t areas = world.numareas > 0 ? (size_t)world.numareas : 0;
	const size_t portals = world.numportals > 0 ? (size_t)world.numportals : 0;
	const size_t clusters = world.numclusters > 0 ? (size_t)world.numclusters : 0;
	const size_t tree = AAS_NodeTreeScratchSize( world.numnodes );
	const size_t numbers = ( clusters + 1 ) * sizeof( int ) + ( ( areas + 2 * portals ) / 32 + 1 ) * sizeof( unsigned int );
	const size_t links = ( areas + 1 ) * sizeof( int );
	size_t size = tree > numbers ? tree : numbers;

	return size > links ? size : links;
}

/*
 * Checks every number a navigation file's tables give into one another, as
 * the botlib follows them, and returns the first fault, worded into `text`;
 * AAS_INDEX_VALID when there is none. `world` holds the decoded tables under
 * the names of aas_t, and `scratch` at least AAS_IndexScratchSize( world )
 * bytes. `quakeRouting` adds the limits of Quake III's routing tables, which
 * RTCW lays out with limits of its own (rtcw_routing_init.hpp).
 */
template <class World>
static inline aasIndexFault_t AAS_IndexFault( const World &world, bool quakeRouting, void *scratch, char *text,
	size_t textSize ) {
	const int numAreas = world.numareas;
	int i, j, node, child;

	if ( text && textSize ) {
		text[0] = '\0';
	}

	/* every descent starts at node 1, and each plane's opposite is the
	 * plane numbered one away (planenum ^ 1): the traces turn their hit
	 * plane round, and the ladder test takes a face's back plane */
	if ( world.numnodes < 2 ) {
		return AAS_IndexRefusal( AAS_INDEX_NO_ROOT, text, textSize, "has no root node" );
	}
	if ( world.numplanes & 1 ) {
		return AAS_IndexRefusal( AAS_INDEX_PLANE_PAIR, text, textSize, "plane %d has no opposite plane",
			world.numplanes - 1 );
	}
	/* an area's number indexes both tables */
	if ( world.numareasettings != numAreas ) {
		return AAS_IndexRefusal( AAS_INDEX_SETTINGS_COUNT, text, textSize, "has %d area settings for %d areas",
			world.numareasettings, numAreas );
	}

	/* the node tree, which every point query, trace and entity link descends */
	for ( i = 1; i < world.numnodes; i++ ) {
		if ( !AAS_IndexInside( world.nodes[i].planenum, world.numplanes ) ) {
			return AAS_IndexRefusal( AAS_INDEX_NODE_PLANE, text, textSize, "node %d has a bad plane", i );
		}
		for ( j = 0; j < 2; j++ ) {
			if ( !AAS_ChildInside( world.nodes[i].children[j], world.numnodes, numAreas ) ) {
				return AAS_IndexRefusal( AAS_INDEX_NODE_CHILD, text, textSize, "node %d has a bad child", i );
			}
		}
	}
	switch ( AAS_NodeTreeFault( world.nodes, world.numnodes, numAreas, scratch, &node, &child ) ) {
	case AAS_TREE_CYCLE:
		return AAS_IndexRefusal( AAS_INDEX_TREE_CYCLE, text, textSize,
			"child %d of node %d leads back to that node or one above it", child, node );
	case AAS_TREE_SHARED:
		return AAS_IndexRefusal( AAS_INDEX_TREE_SHARED, text, textSize,
			"child %d of node %d is already below another node", child, node );
	case AAS_TREE_OUTSIDE:	/* refused above */
		return AAS_IndexRefusal( AAS_INDEX_NODE_CHILD, text, textSize, "node %d has a bad child", node );
	default:
		break;
	}

	/* the geometry: areas, their faces, the faces' edges and the edges' vertexes */
	for ( i = 0; i < numAreas; i++ ) {
		if ( !AAS_RangeInside( world.areas[i].firstface, world.areas[i].numfaces, world.faceindexsize ) ) {
			return AAS_IndexRefusal( AAS_INDEX_AREA_FACES, text, textSize, "area %d has a bad face range", i );
		}
	}
	for ( i = 0; i < world.faceindexsize; i++ ) {
		if ( !AAS_SignedIndexInside( world.faceindex[i], world.numfaces ) ) {
			return AAS_IndexRefusal( AAS_INDEX_FACE_INDEX, text, textSize, "face index %d names no face", i );
		}
	}
	for ( i = 1; i < world.numfaces; i++ ) {
		if ( !AAS_IndexInside( world.faces[i].planenum, world.numplanes ) ) {
			return AAS_IndexRefusal( AAS_INDEX_FACE_PLANE, text, textSize, "face %d has a bad plane", i );
		}
		if ( !AAS_RangeInside( world.faces[i].firstedge, world.faces[i].numedges, world.edgeindexsize ) ) {
			return AAS_IndexRefusal( AAS_INDEX_FACE_EDGES, text, textSize, "face %d has a bad edge range", i );
		}
		/* area 0 is the solid side of a face */
		if ( !AAS_IndexInside( world.faces[i].frontarea, numAreas ) || !AAS_IndexInside( world.faces[i].backarea, numAreas ) ) {
			return AAS_IndexRefusal( AAS_INDEX_FACE_AREA, text, textSize, "face %d has a bad area", i );
		}
	}
	for ( i = 0; i < world.edgeindexsize; i++ ) {
		if ( !AAS_SignedIndexInside( world.edgeindex[i], world.numedges ) ) {
			return AAS_IndexRefusal( AAS_INDEX_EDGE_INDEX, text, textSize, "edge index %d names no edge", i );
		}
	}
	for ( i = 1; i < world.numedges; i++ ) {
		if ( !AAS_IndexInside( world.edges[i].v[0], world.numvertexes )
			|| !AAS_IndexInside( world.edges[i].v[1], world.numvertexes ) ) {
			return AAS_IndexRefusal( AAS_INDEX_EDGE_VERTEX, text, textSize, "edge %d has a bad vertex", i );
		}
	}

	/* the routing tables. Each cluster numbers the areas it holds from 0,
	 * and a portal takes a number in both clusters it joins; those numbers
	 * index each cluster's caches and travel times. */
	for ( i = 0; i < world.numclusters; i++ ) {
		const int clusterAreas = world.clusters[i].numareas;
		const int reachable = world.clusters[i].numreachabilityareas;

		if ( clusterAreas < 0 || reachable < 0 || reachable > clusterAreas ) {
			return AAS_IndexRefusal( AAS_INDEX_CLUSTER_COUNTS, text, textSize, "cluster %d has bad area counts", i );
		}
		if ( !AAS_RangeInside( world.clusters[i].firstportal, world.clusters[i].numportals, world.portalindexsize ) ) {
			return AAS_IndexRefusal( AAS_INDEX_CLUSTER_PORTALS, text, textSize, "cluster %d has a bad portal range", i );
		}
	}
	for ( i = 0; i < world.portalindexsize; i++ ) {
		if ( !AAS_IndexInside( world.portalindex[i], world.numportals ) ) {
			return AAS_IndexRefusal( AAS_INDEX_PORTAL_INDEX, text, textSize, "portal index %d names no portal", i );
		}
	}
	for ( i = 0; i < world.numportals; i++ ) {
		const aas_portal_t *portal = &world.portals[i];

		if ( !AAS_IndexInside( portal->areanum, numAreas ) ) {
			return AAS_IndexRefusal( AAS_INDEX_PORTAL_AREA, text, textSize, "portal %d has a bad area", i );
		}
		if ( !AAS_IndexInside( portal->frontcluster, world.numclusters )
			|| !AAS_IndexInside( portal->backcluster, world.numclusters ) ) {
			return AAS_IndexRefusal( AAS_INDEX_PORTAL_CLUSTER, text, textSize, "portal %d has a bad cluster", i );
		}
		/* portal 0, the dummy every area outside a cluster is routed
		 * through, joins no cluster; its numbers must still not be
		 * negative, which no cache bound would catch */
		for ( j = 0; j < 2; j++ ) {
			const int number = portal->clusterareanum[j];
			const int side = j ? portal->backcluster : portal->frontcluster;

			if ( number < 0 || ( i > 0 && number >= world.clusters[side].numareas ) ) {
				return AAS_IndexRefusal( AAS_INDEX_PORTAL_NUMBER, text, textSize,
					"portal %d has a bad number in cluster %d", i, side );
			}
		}
	}
	for ( i = 0; i < numAreas; i++ ) {
		const int cluster = world.areasettings[i].cluster;

		/* area 0 too: routing lays out rows for its reachabilities */
		if ( !AAS_RangeInside( world.areasettings[i].firstreachablearea, world.areasettings[i].numreachableareas,
			world.reachabilitysize ) ) {
			return AAS_IndexRefusal( AAS_INDEX_AREA_REACHABILITIES, text, textSize,
				"area %d has a bad reachability range", i );
		}
		/* a positive cluster is the area's own; a negative one names the
		 * portal the area is */
		if ( cluster > 0 ? cluster >= world.numclusters : cluster < 0 && !AAS_SignedIndexInside( cluster, world.numportals ) ) {
			return AAS_IndexRefusal( AAS_INDEX_AREA_CLUSTER, text, textSize, "area %d has a bad cluster", i );
		}
		if ( cluster > 0 && !AAS_IndexInside( world.areasettings[i].clusterareanum, world.clusters[cluster].numareas ) ) {
			return AAS_IndexRefusal( AAS_INDEX_AREA_NUMBER, text, textSize,
				"area %d has a bad number in cluster %d", i, cluster );
		}
		if ( cluster < 0 && world.portals[-cluster].areanum != i ) {
			return AAS_IndexRefusal( AAS_INDEX_AREA_PORTAL, text, textSize,
				"area %d is portal %d, which is another area", i, -cluster );
		}
		/* an area routing can start or end in must be in a cluster, or be
		 * a portal; area 0 is never either. A file with no clusters at
		 * all has them built as it loads, from scratch. */
		if ( i > 0 && !cluster && world.areasettings[i].numreachableareas > 0 && world.numclusters > 0 ) {
			return AAS_IndexRefusal( AAS_INDEX_AREA_UNCLUSTERED, text, textSize,
				"area %d has reachabilities but no cluster", i );
		}
	}
	for ( i = 1; i < world.numportals; i++ ) {
		if ( world.areasettings[world.portals[i].areanum].cluster != -i ) {
			return AAS_IndexRefusal( AAS_INDEX_PORTAL_OWNER, text, textSize,
				"portal %d is area %d, which is not that portal", i, world.portals[i].areanum );
		}
	}
	/* a cluster's portals are the ones on its border: routing numbers each
	 * one inside the cluster whose list names it */
	for ( i = 0; i < world.numclusters; i++ ) {
		for ( j = 0; j < world.clusters[i].numportals; j++ ) {
			const int portal = world.portalindex[world.clusters[i].firstportal + j];

			if ( world.portals[portal].frontcluster != i && world.portals[portal].backcluster != i ) {
				return AAS_IndexRefusal( AAS_INDEX_PORTAL_BORDER, text, textSize,
					"cluster %d lists portal %d, which is not on its border", i, portal );
			}
		}
	}
	/* reachability 0, a dummy leading to area 0, too */
	for ( i = 0; i < world.reachabilitysize; i++ ) {
		if ( !AAS_IndexInside( world.reachability[i].areanum, numAreas ) ) {
			return AAS_IndexRefusal( AAS_INDEX_REACH_AREA, text, textSize, "reachability %d leads to a bad area", i );
		}
	}
	/* every cluster number names one area: an area of that cluster, or a
	 * portal on its border. Routing lays out a cache for each number. */
	{
		const long long maxNumbers = (long long)numAreas + 2 * (long long)world.numportals;
		int *first = (int *)scratch;
		unsigned int *taken = (unsigned int *)( first + world.numclusters + 1 );
		long long numbers = 0;

		for ( i = 0; i < world.numclusters; i++ ) {
			first[i] = (int)numbers;
			numbers += world.clusters[i].numareas;
			if ( numbers > maxNumbers ) {
				return AAS_IndexRefusal( AAS_INDEX_CLUSTER_NUMBERS, text, textSize,
					"clusters hold more numbers than %d areas and %d portals can take", numAreas, world.numportals );
			}
		}
		first[world.numclusters] = (int)numbers;
		memset( taken, 0, ( (size_t)maxNumbers / 32 + 1 ) * sizeof( *taken ) );
		for ( i = 1; i < numAreas; i++ ) {
			const int cluster = world.areasettings[i].cluster;
			int sides, clusters[2], locals[2];

			if ( cluster > 0 ) {
				sides = 1;
				clusters[0] = cluster;
				locals[0] = world.areasettings[i].clusterareanum;
			} else if ( cluster < 0 ) {
				sides = 2;
				for ( j = 0; j < 2; j++ ) {
					clusters[j] = j ? world.portals[-cluster].backcluster : world.portals[-cluster].frontcluster;
					locals[j] = world.portals[-cluster].clusterareanum[j];
				}
				if ( clusters[0] == clusters[1] && locals[0] == locals[1] ) {
					sides = 1;	/* a portal back onto its own cluster takes one number */
				}
			} else {
				continue;
			}
			for ( j = 0; j < sides; j++ ) {
				const int bit = first[clusters[j]] + locals[j];

				if ( taken[bit >> 5] & ( 1u << ( bit & 31 ) ) ) {
					return AAS_IndexRefusal( AAS_INDEX_CLUSTER_NUMBER_SHARED, text, textSize,
						"area %d shares number %d in cluster %d with another area", i, locals[j], clusters[j] );
				}
				taken[bit >> 5] |= 1u << ( bit & 31 );
			}
		}
	}
	if ( !quakeRouting ) {
		return AAS_INDEX_VALID;
	}

	/* Quake III's routing: one reversed link for each of the first 128
	 * reachabilities of each area but area 0, laid out in a table with one
	 * link a reachability, and the travel times each area's links need */
	{
		int *incoming = (int *)scratch;
		long long links = 0, bytes;

		memset( incoming, 0, ( (size_t)numAreas + 1 ) * sizeof( *incoming ) );
		for ( i = 1; i < numAreas; i++ ) {
			const int first = world.areasettings[i].firstreachablearea;
			int count = world.areasettings[i].numreachableareas;

			if ( count > AAS_MAX_REVERSED_REACHABILITIES ) {
				count = AAS_MAX_REVERSED_REACHABILITIES;
			}
			links += count;
			if ( links > world.reachabilitysize ) {
				return AAS_IndexRefusal( AAS_INDEX_REVERSED_LINKS, text, textSize,
					"areas lay out more reachabilities than the file's %d", world.reachabilitysize );
			}
			for ( j = 0; j < count; j++ ) {
				incoming[world.reachability[first + j].areanum]++;
			}
		}
		bytes = (long long)numAreas * (long long)sizeof( unsigned short ** );
		for ( i = 0; i < numAreas; i++ ) {
			const long long rows = world.areasettings[i].numreachableareas;
			const long long times = ( (long long)incoming[i] + (long long)sizeof( long ) - 1 )
				/ (long long)sizeof( long ) * (long long)sizeof( long );

			bytes += rows * (long long)sizeof( unsigned short * ) + rows * times * (long long)sizeof( unsigned short );
			if ( bytes > AAS_MAX_TRAVEL_TIME_BYTES ) {
				return AAS_IndexRefusal( AAS_INDEX_TRAVEL_TIMES, text, textSize,
					"routing would need more than %d bytes of travel times", AAS_MAX_TRAVEL_TIME_BYTES );
			}
		}
	}
	return AAS_INDEX_VALID;
}

#endif
