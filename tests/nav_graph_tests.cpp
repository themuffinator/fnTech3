/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* The navigation graph Raven's games fill and query. Every answer here is
 * about a graph this test built, so what is checked is that the graph says
 * what is in it -- including that an empty one says it holds nothing rather
 * than answering zero, which is a node. */

#include "../code/nav/nav_graph.hpp"

#include <cstdio>
#include <iostream>

using fntech3::nav::Graph;
using fntech3::nav::MaxEdgesPerNode;
using fntech3::nav::MaxNodes;
using fntech3::nav::NoNode;
using fntech3::nav::Unreachable;

namespace {

int failures;

void Expect( bool condition, const char *message )
{
	if ( !condition ) {
		std::cerr << "FAIL: " << message << "\n";
		++failures;
	}
}

int Add( Graph &graph, float x, float y, float z, int flags = 0, int radius = 0 )
{
	const float origin[3] = { x, y, z };
	return graph.AddNode( origin, flags, radius );
}

/* An empty graph holds nothing, and says so rather than answering zero. */
void CheckAnEmptyGraphHoldsNothing()
{
	Graph graph;
	Expect( graph.NodeCount() == 0, "an empty graph holds no node" );
	Expect( graph.At( 0 ) == nullptr && graph.At( -1 ) == nullptr,
		"there is no node zero in an empty graph" );
	Expect( graph.EdgeCount( 0 ) == 0 && graph.EdgeAt( 0, 0 ) == NoNode,
		"a node that is not there has no edge" );
	Expect( !graph.AreNeighbors( 0, 1 ), "nothing neighbours nothing" );
	Expect( graph.PathCost( 0, 0 ) == Unreachable && graph.NextOnPath( 0, 0 ) == NoNode,
		"there is no path across an empty graph" );
	const float somewhere[3] = { 1.0f, 2.0f, 3.0f };
	Expect( graph.NearestNode( somewhere, 0 ) == NoNode,
		"nothing is near a point when there is nothing" );
	Expect( !graph.PathsCalculated(), "an empty graph has not been worked out" );

	/* Working out an empty graph is a finished answer, not a refusal. */
	graph.CalculatePaths();
	Expect( graph.PathsCalculated(), "an empty graph works out at once" );
	Expect( graph.PathCost( 0, 0 ) == Unreachable, "and still has no path in it" );
}

/* Nodes are numbered as they arrive and keep their positions. */
void CheckNodesAreNumberedAsTheyArrive()
{
	Graph graph;
	Expect( Add( graph, 0, 0, 0, 3, 40 ) == 0, "the first node is node zero" );
	Expect( Add( graph, 100, 0, 0 ) == 1, "the second is node one" );
	Expect( graph.NodeCount() == 2, "and the graph holds both" );

	const fntech3::nav::Node *first = graph.At( 0 );
	Expect( first && first->origin[0] == 0.0f && first->flags == 3 && first->radius == 40,
		"a node keeps the point, flags and radius it was made with" );
	Expect( graph.AddNode( nullptr, 0, 0 ) == NoNode, "a node needs a point" );

	graph.Clear();
	Expect( graph.NodeCount() == 0 && graph.At( 0 ) == nullptr,
		"clearing a graph empties it" );
}

/* An edge joins two nodes both ways, at the distance between them. */
void CheckEdgesJoinBothWays()
{
	Graph graph;
	Add( graph, 0, 0, 0 );
	Add( graph, 300, 400, 0 );          /* 500 units away */
	Expect( graph.Connect( 0, 1 ), "two nodes connect" );
	Expect( graph.AreNeighbors( 0, 1 ) && graph.AreNeighbors( 1, 0 ),
		"a connection runs both ways" );
	Expect( graph.EdgeCount( 0 ) == 1 && graph.EdgeAt( 0, 0 ) == 1,
		"and shows up as one edge" );
	Expect( graph.At( 0 )->edges[0].cost == 500,
		"an edge costs the distance between its nodes" );

	/* The same connection twice is still one edge. */
	Expect( graph.Connect( 0, 1 ) && graph.EdgeCount( 0 ) == 1,
		"connecting twice does not double an edge" );
	Expect( !graph.Connect( 0, 0 ), "a node does not connect to itself" );
	Expect( !graph.Connect( 0, 9 ) && !graph.Connect( -1, 1 ),
		"a node that is not there does not connect" );
	Expect( graph.EdgeAt( 0, 1 ) == NoNode && graph.EdgeAt( 0, -1 ) == NoNode,
		"an edge that is not there is no node" );
}

/* Two nodes a step apart still cost something to walk between. */
void CheckAStepIsNeverFree()
{
	Graph graph;
	Add( graph, 0, 0, 0 );
	Add( graph, 0, 0, 0 );              /* the same point, a different node */
	Expect( graph.Connect( 0, 1 ) && graph.At( 0 )->edges[0].cost == 1,
		"a step between two nodes is never free" );
}

/* Paths are worked out across the graph, not guessed at. */
void CheckPathsAreWorkedOut()
{
	Graph graph;
	Add( graph, 0, 0, 0 );              /* 0 */
	Add( graph, 100, 0, 0 );            /* 1 */
	Add( graph, 200, 0, 0 );            /* 2 */
	Add( graph, 5000, 0, 0 );           /* 3, joined to nothing */
	graph.Connect( 0, 1 );
	graph.Connect( 1, 2 );

	/* Until the paths are worked out, nothing claims to know one. */
	Expect( graph.PathCost( 0, 2 ) == Unreachable && !graph.PathsCalculated(),
		"an unworked graph knows no path" );

	graph.CalculatePaths();
	Expect( graph.PathsCalculated(), "the graph reports that it was worked out" );
	Expect( graph.PathCost( 0, 0 ) == 0, "a node costs nothing to reach itself" );
	Expect( graph.PathCost( 0, 1 ) == 100 && graph.PathCost( 0, 2 ) == 200,
		"a path costs the sum of its edges" );
	Expect( graph.PathCost( 2, 0 ) == 200, "and costs the same coming back" );
	Expect( graph.NextOnPath( 0, 2 ) == 1,
		"the way to a far node is through the near one" );
	Expect( graph.NextOnPath( 2, 0 ) == 1, "and back again" );
	Expect( graph.PathCost( 0, 3 ) == Unreachable && graph.NextOnPath( 0, 3 ) == NoNode,
		"a node joined to nothing is not reachable" );

	/* Adding to the graph makes the worked-out paths stale, and it says so
	 * rather than answering from them. */
	Add( graph, 300, 0, 0 );
	Expect( !graph.PathsCalculated() && graph.PathCost( 0, 2 ) == Unreachable,
		"adding a node unworks the paths" );
	graph.CalculatePaths();
	graph.Connect( 2, 4 );
	Expect( !graph.PathsCalculated(), "so does adding an edge" );
	graph.CalculatePaths();
	Expect( graph.PathCost( 0, 4 ) == 300 && graph.NextOnPath( 0, 4 ) == 1,
		"and the longer path is then known" );
}

/* The cheapest of two routes is the one that is found. */
void CheckTheCheapestRouteWins()
{
	Graph graph;
	Add( graph, 0, 0, 0 );              /* 0 */
	Add( graph, 0, 1000, 0 );           /* 1, the long way round */
	Add( graph, 100, 0, 0 );            /* 2, the short way */
	Add( graph, 200, 0, 0 );            /* 3 */
	graph.Connect( 0, 1 );
	graph.Connect( 1, 3 );
	graph.Connect( 0, 2 );
	graph.Connect( 2, 3 );
	graph.CalculatePaths();
	Expect( graph.PathCost( 0, 3 ) == 200, "the cheaper of two routes is the cost" );
	Expect( graph.NextOnPath( 0, 3 ) == 2, "and the cheaper one is the way" );
}

/* The nearest node counts only the nodes the caller asked for. */
void CheckTheNearestNodeIsTheOneAskedFor()
{
	Graph graph;
	Add( graph, 0, 0, 0, 1 );           /* 0, flag 1 */
	Add( graph, 10, 0, 0, 2 );          /* 1, flag 2 */
	Add( graph, 400, 0, 0, 1 );         /* 2, flag 1 */
	const float here[3] = { 12.0f, 0.0f, 0.0f };

	Expect( graph.NearestNode( here, 0 ) == 1, "with no flags asked for, the nearest wins" );
	Expect( graph.NearestNode( here, 1 ) == 0, "with a flag asked for, only those count" );
	Expect( graph.NearestNode( here, 4 ) == NoNode, "and a flag nothing carries finds nothing" );
	Expect( graph.NearestNode( nullptr, 0 ) == NoNode, "nothing is near nowhere" );

	/* A node an entity already failed at is not offered to it again. */
	graph.AddFailedNode( 7, 1 );
	Expect( graph.NearestNode( here, 0, 7 ) == 0,
		"a node this entity failed at is not the nearest one for it" );
	Expect( graph.NearestNode( here, 0, 8 ) == 1, "but it still is for another" );
	graph.ClearFailedNodes( 7 );
	Expect( graph.NearestNode( here, 0, 7 ) == 1, "until that entity forgets" );
}

/* Failed nodes belong to one entity; failed edges belong to the map. */
void CheckWhatFailedBelongsToWhom()
{
	Graph graph;
	Add( graph, 0, 0, 0 );
	Add( graph, 100, 0, 0 );
	graph.Connect( 0, 1 );

	Expect( !graph.NodeFailed( 3, 1 ), "nothing has failed to begin with" );
	graph.AddFailedNode( 3, 1 );
	Expect( graph.NodeFailed( 3, 1 ) && !graph.NodeFailed( 4, 1 ),
		"a node one entity failed at has not failed for another" );
	graph.AddFailedNode( 3, 9 );
	Expect( !graph.NodeFailed( 3, 9 ), "a node that is not there cannot be failed at" );

	Expect( graph.FailedEdgeCount() == 0, "no edge is blocked to begin with" );
	graph.AddFailedEdge( 0, 1 );
	Expect( graph.EdgeFailed( 0, 1 ) && !graph.EdgeFailed( 1, 0 ),
		"a blocked edge is blocked the way it was reported" );
	Expect( graph.FailedEdgeCount() == 1, "and counts once" );
	graph.AddFailedEdge( 0, 9 );
	Expect( graph.FailedEdgeCount() == 1, "an edge between nodes that are not there is not one" );
	graph.ClearAllFailedEdges();
	Expect( graph.FailedEdgeCount() == 0 && !graph.EdgeFailed( 0, 1 ),
		"clearing forgets every blocked edge" );
}

/* Nodes looked at during one search are forgotten before the next. */
void CheckCheckedNodesAreForgotten()
{
	Graph graph;
	Add( graph, 0, 0, 0 );
	Expect( !graph.Checked( 0 ), "nothing has been looked at yet" );
	graph.SetChecked( 0, true );
	Expect( graph.Checked( 0 ), "a node can be marked as looked at" );
	graph.SetChecked( 0, false );
	Expect( !graph.Checked( 0 ), "and unmarked" );
	graph.SetChecked( 0, true );
	graph.SetChecked( 5, true );
	Expect( !graph.Checked( 5 ), "a node that is not there is not looked at" );
	graph.ClearCheckedNodes();
	Expect( !graph.Checked( 0 ), "and clearing forgets them all" );
}

/* This engine's own ceilings are refusals, never silent truncation. */
void CheckTheEnginesOwnCeilings()
{
	Graph graph;
	for ( int made = 0; made < MaxNodes; ++made ) {
		if ( Add( graph, float( made ), 0, 0 ) != made ) {
			Expect( false, "every node up to the ceiling is held" );
			return;
		}
	}
	Expect( graph.NodeCount() == MaxNodes, "the graph fills to its ceiling" );
	Expect( Add( graph, 1, 1, 1 ) == NoNode, "and refuses the one past it" );

	Graph crowded;
	for ( int made = 0; made <= MaxEdgesPerNode; ++made ) Add( crowded, float( made ), 0, 0 );
	for ( int to = 1; to <= MaxEdgesPerNode; ++to ) {
		const bool joined = crowded.Connect( 0, to );
		Expect( joined == ( to <= MaxEdgesPerNode ), "edges join up to the ceiling" );
	}
	Expect( crowded.EdgeCount( 0 ) == MaxEdgesPerNode,
		"a node holds no more edges than it may" );
}

} // namespace

int main()
{
	CheckAnEmptyGraphHoldsNothing();
	CheckNodesAreNumberedAsTheyArrive();
	CheckEdgesJoinBothWays();
	CheckAStepIsNeverFree();
	CheckPathsAreWorkedOut();
	CheckTheCheapestRouteWins();
	CheckTheNearestNodeIsTheOneAskedFor();
	CheckWhatFailedBelongsToWhom();
	CheckCheckedNodesAreForgotten();
	CheckTheEnginesOwnCeilings();

	if ( failures != 0 ) {
		std::cerr << failures << " navigation graph checks failed\n";
		return 1;
	}
	std::printf( "navigation graph checks passed\n" );
	return 0;
}
