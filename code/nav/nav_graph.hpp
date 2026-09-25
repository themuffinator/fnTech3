/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_NAV_GRAPH_HPP
#define FNTECH3_NAV_GRAPH_HPP

/* The navigation graph Raven's games ask the engine to hold for them.
 *
 * This is not Quake III's area awareness and shares nothing with it. Jedi
 * Academy's bots never call one of the AAS imports -- the retail module has no
 * call thunk for any number in that block, and the installation ships no `.aas`
 * file for any map. What it asks for instead is a graph of numbered nodes with
 * positions and radii, edges between them with costs, and a shortest path
 * between any two. See docs/fntech3/2026-09-22-jka-trace.md.
 *
 * The graph is the engine's to hold and the game's to fill: the module either
 * asks for a `.nav` file to be loaded or builds it itself, point by point and
 * connection by connection, and then asks for the paths to be worked out. It
 * never asks for the graph to be created or destroyed, so this owns its own
 * lifetime and is cleared when a map is.
 *
 * Everything here answers truthfully about a graph that may be empty. A query
 * against an empty graph says there is no node and no path, which is what an
 * empty graph means; it does not say zero, because node zero is a node. */

#include <cmath>
#include <cstddef>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fntech3 {
namespace nav {

/* No such node, and no such path. Both are what the games' own code tests
 * against, and neither is a valid node number. */
inline constexpr int NoNode = -1;
inline constexpr int Unreachable = -1;

/* This engine's own bounds, not a claim about any game's. A graph larger than
 * this is refused rather than truncated; the retail multiplayer graphs are far
 * smaller, and nothing has yet been measured that says where Raven's own
 * ceiling is. */
inline constexpr int MaxNodes = 1024;
inline constexpr int MaxEdgesPerNode = 64;
/* A single edge may cost no more than this. A pair of nodes further apart
 * than the largest map is a graph this engine will not believe, and clamping
 * keeps the path arithmetic inside an int. */
inline constexpr int MaxEdgeCost = 1 << 24;

struct Edge {
	int to = NoNode;
	int cost = 0;
};

struct Node {
	float origin[3] = { 0.0f, 0.0f, 0.0f };
	int flags = 0;
	int radius = 0;
	std::vector<Edge> edges;
};

class Graph {
public:
	void Clear()
	{
		nodes_.clear();
		cost_.clear();
		next_.clear();
		failedNodes_.clear();
		failedEdges_.clear();
		checked_.clear();
		pathsCalculated_ = false;
	}

	int NodeCount() const { return int( nodes_.size() ); }
	bool Holds( int id ) const { return id >= 0 && id < NodeCount(); }
	const Node *At( int id ) const
	{
		return Holds( id ) ? &nodes_[std::size_t( id )] : nullptr;
	}

	/* Add a node at a point. Returns its number, or NoNode when this engine
	 * will hold no more. Adding invalidates any paths already worked out. */
	int AddNode( const float origin[3], int flags, int radius )
	{
		if ( NodeCount() >= MaxNodes || !origin ) return NoNode;
		Node node;
		for ( int axis = 0; axis < 3; ++axis ) node.origin[axis] = origin[axis];
		node.flags = flags;
		node.radius = radius;
		nodes_.push_back( std::move( node ) );
		pathsCalculated_ = false;
		return NodeCount() - 1;
	}

	/* Join two nodes, in both directions, at the distance between them. A
	 * connection that already exists is left alone rather than doubled. */
	bool Connect( int first, int second )
	{
		if ( !Holds( first ) || !Holds( second ) || first == second ) return false;
		const int cost = Distance( first, second );
		if ( !Join( first, second, cost ) || !Join( second, first, cost ) ) return false;
		pathsCalculated_ = false;
		return true;
	}

	int EdgeCount( int id ) const
	{
		const Node *node = At( id );
		return node ? int( node->edges.size() ) : 0;
	}

	int EdgeAt( int id, int index ) const
	{
		const Node *node = At( id );
		if ( !node || index < 0 || index >= int( node->edges.size() ) ) return NoNode;
		return node->edges[std::size_t( index )].to;
	}

	bool AreNeighbors( int first, int second ) const
	{
		const Node *node = At( first );
		if ( !node || !Holds( second ) ) return false;
		for ( const Edge &edge : node->edges ) if ( edge.to == second ) return true;
		return false;
	}

	/* Work out the cheapest path between every pair of nodes. This is what
	 * the module asks for once, after it has finished building the graph, and
	 * what a `.nav` file exists to avoid repeating. */
	void CalculatePaths();
	bool PathsCalculated() const { return pathsCalculated_; }
	void SetPathsCalculated( bool value ) { pathsCalculated_ = value; }

	/* What a path costs, or Unreachable. Unworked paths are not guessed at:
	 * until CalculatePaths has run, every pair is unreachable. */
	int PathCost( int from, int to ) const;
	/* The next node to walk to along that path, or NoNode. */
	int NextOnPath( int from, int to ) const;

	/* The node nearest a point, counting only nodes whose flags the caller
	 * asked for, and never a node that entity has already failed at. */
	int NearestNode( const float origin[3], int flags, int entity = NoNode ) const;

	/* Nodes an entity could not reach, which it asks to be forgotten each
	 * time it gives up on a route. */
	void ClearFailedNodes( int entity ) { failedNodes_.erase( entity ); }
	void AddFailedNode( int entity, int node )
	{
		if ( Holds( node ) ) failedNodes_[entity].insert( node );
	}
	bool NodeFailed( int entity, int node ) const
	{
		const auto found = failedNodes_.find( entity );
		return found != failedNodes_.end() && found->second.count( node ) != 0;
	}

	/* Edges something is standing in the way of. These are the whole graph's,
	 * not one entity's, because a door closed for one bot is closed for all. */
	void ClearAllFailedEdges() { failedEdges_.clear(); }
	void AddFailedEdge( int from, int to )
	{
		if ( Holds( from ) && Holds( to ) ) failedEdges_.insert( Pair( from, to ) );
	}
	bool EdgeFailed( int from, int to ) const
	{
		return failedEdges_.count( Pair( from, to ) ) != 0;
	}
	int FailedEdgeCount() const { return int( failedEdges_.size() ); }

	/* Nodes already looked at during one search, cleared before the next. */
	void ClearCheckedNodes() { checked_.clear(); }
	bool Checked( int node ) const { return checked_.count( node ) != 0; }
	void SetChecked( int node, bool value )
	{
		if ( !Holds( node ) ) return;
		if ( value ) checked_.insert( node );
		else checked_.erase( node );
	}

private:
	int Distance( int first, int second ) const;
	bool Join( int from, int to, int cost );
	static long long Pair( int from, int to )
	{
		return ( static_cast<long long>( from ) << 32 ) | (unsigned)to;
	}

	std::vector<Node> nodes_;
	/* Row-major over the nodes that exist, not over the ceiling. */
	std::vector<int> cost_;
	std::vector<int> next_;
	bool pathsCalculated_ = false;

	std::unordered_map<int, std::unordered_set<int>> failedNodes_;
	std::unordered_set<long long> failedEdges_;
	std::unordered_set<int> checked_;
};

inline int Graph::Distance( int first, int second ) const
{
	const Node &a = nodes_[std::size_t( first )];
	const Node &b = nodes_[std::size_t( second )];
	double total = 0.0;
	for ( int axis = 0; axis < 3; ++axis ) {
		const double span = double( a.origin[axis] ) - double( b.origin[axis] );
		total += span * span;
	}
	/* A cost is a whole number of units, and never zero for two distinct
	 * nodes: a free step between them would make every path through them cost
	 * the same as one that skipped them. */
	const double length = std::sqrt( total );
	if ( !( length < double( MaxEdgeCost ) ) ) return MaxEdgeCost;
	const int cost = int( length + 0.5 );
	return cost < 1 ? 1 : cost;
}

inline bool Graph::Join( int from, int to, int cost )
{
	Node &node = nodes_[std::size_t( from )];
	for ( Edge &edge : node.edges ) {
		if ( edge.to == to ) { edge.cost = cost; return true; }
	}
	if ( int( node.edges.size() ) >= MaxEdgesPerNode ) return false;
	Edge edge;
	edge.to = to;
	edge.cost = cost;
	node.edges.push_back( edge );
	return true;
}

inline void Graph::CalculatePaths()
{
	const int count = NodeCount();
	cost_.assign( std::size_t( count ) * std::size_t( count ), Unreachable );
	next_.assign( std::size_t( count ) * std::size_t( count ), NoNode );
	if ( count == 0 ) { pathsCalculated_ = true; return; }

	const int beyond = std::numeric_limits<int>::max() / 4;
	std::vector<int> best( std::size_t( count ) * std::size_t( count ), beyond );
	for ( int node = 0; node < count; ++node ) {
		best[std::size_t( node ) * count + node] = 0;
		next_[std::size_t( node ) * count + node] = node;
		for ( const Edge &edge : nodes_[std::size_t( node )].edges ) {
			if ( !Holds( edge.to ) ) continue;
			const std::size_t at = std::size_t( node ) * count + edge.to;
			if ( edge.cost < best[at] ) {
				best[at] = edge.cost;
				next_[at] = edge.to;
			}
		}
	}
	for ( int through = 0; through < count; ++through ) {
		for ( int from = 0; from < count; ++from ) {
			const int reach = best[std::size_t( from ) * count + through];
			if ( reach >= beyond ) continue;
			for ( int to = 0; to < count; ++to ) {
				const int rest = best[std::size_t( through ) * count + to];
				if ( rest >= beyond ) continue;
				const std::size_t at = std::size_t( from ) * count + to;
				if ( reach + rest < best[at] ) {
					best[at] = reach + rest;
					next_[at] = next_[std::size_t( from ) * count + through];
				}
			}
		}
	}
	for ( std::size_t at = 0; at < best.size(); ++at ) {
		cost_[at] = best[at] >= beyond ? Unreachable : best[at];
		if ( cost_[at] == Unreachable ) next_[at] = NoNode;
	}
	pathsCalculated_ = true;
}

inline int Graph::PathCost( int from, int to ) const
{
	if ( !pathsCalculated_ || !Holds( from ) || !Holds( to ) ) return Unreachable;
	return cost_[std::size_t( from ) * NodeCount() + to];
}

inline int Graph::NextOnPath( int from, int to ) const
{
	if ( !pathsCalculated_ || !Holds( from ) || !Holds( to ) ) return NoNode;
	return next_[std::size_t( from ) * NodeCount() + to];
}

inline int Graph::NearestNode( const float origin[3], int flags, int entity ) const
{
	if ( !origin ) return NoNode;
	int nearest = NoNode;
	double closest = 0.0;
	for ( int id = 0; id < NodeCount(); ++id ) {
		const Node &node = nodes_[std::size_t( id )];
		if ( flags != 0 && ( node.flags & flags ) == 0 ) continue;
		if ( entity != NoNode && NodeFailed( entity, id ) ) continue;
		double total = 0.0;
		for ( int axis = 0; axis < 3; ++axis ) {
			const double span = double( node.origin[axis] ) - double( origin[axis] );
			total += span * span;
		}
		if ( nearest == NoNode || total < closest ) {
			nearest = id;
			closest = total;
		}
	}
	return nearest;
}

} // namespace nav
} // namespace fntech3

#endif
