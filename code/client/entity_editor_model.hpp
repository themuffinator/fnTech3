/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

The entity editor's document: a map's entities as the editor holds them,
each with an identity that survives edits, undo and redo, the selection,
the history, and what differs from the text last saved. It knows nothing
of the engine, so tests compile it alone; cl_entity_editor.cpp draws it and
feeds it input.
===========================================================================
*/
#ifndef FNTECH3_CLIENT_ENTITY_EDITOR_MODEL_HPP
#define FNTECH3_CLIENT_ENTITY_EDITOR_MODEL_HPP

#include "../qcommon/entity_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fntech3::entity_editor {

using entities::Entity;
using entities::KeyValue;

struct Item {
	std::uint32_t id = 0;
	Entity entity;

	friend bool operator==( const Item &a, const Item &b ) { return a.id == b.id && a.entity == b.entity; }
};

/* What a game module allows an entity, where its source is public: the
 * keys it keeps (MAX_SPAWN_VARS) and the bytes its keys and values take,
 * each with its terminating NUL (MAX_SPAWN_VARS_CHARS). Zero where nothing
 * is known or the module takes the whole text (Ritual's and 2015's). */
struct SpawnLimits {
	int maxPairs = 0;
	int maxChars = 0;
};

/* How an entity is drawn and listed. */
enum class Category {
	World,
	Spawn,
	Light,
	Item,
	Trigger,
	Logic,
	Brush,
	Misc,
	Other
};

[[nodiscard]] inline bool IsBrushModel( const Entity &entity ) noexcept
{
	const std::string_view model = entity.Value( "model" );
	return model.size() > 1 && model[0] == '*' && model[1] >= '0' && model[1] <= '9';
}

[[nodiscard]] inline int BrushModelNumber( const Entity &entity ) noexcept
{
	if ( !IsBrushModel( entity ) ) {
		return -1;
	}
	return std::atoi( std::string( entity.Value( "model" ).substr( 1 ) ).c_str() );
}

[[nodiscard]] inline bool ContainsNoCase( std::string_view text, std::string_view part ) noexcept
{
	if ( part.empty() ) {
		return true;
	}
	for ( std::size_t i = 0; i + part.size() <= text.size(); i++ ) {
		if ( entities::EqualsNoCase( text.substr( i, part.size() ), part ) ) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] inline Category Classify( const Entity &entity ) noexcept
{
	using entities::StartsWithNoCase;
	const std::string_view name = entity.Classname();
	if ( entities::EqualsNoCase( name, "worldspawn" ) ) return Category::World;
	/* Alice's and F.A.K.K.2's func_spawn and its kin make things, not players */
	if ( StartsWithNoCase( name, "info_player" )
		|| ( ContainsNoCase( name, "spawn" ) && !StartsWithNoCase( name, "func_" ) ) ) return Category::Spawn;
	if ( StartsWithNoCase( name, "light" ) ) return Category::Light;
	if ( StartsWithNoCase( name, "item_" ) || StartsWithNoCase( name, "weapon_" )
		|| StartsWithNoCase( name, "ammo_" ) || StartsWithNoCase( name, "holdable_" )
		|| StartsWithNoCase( name, "powerup_" ) || ContainsNoCase( name, "flag" ) ) return Category::Item;
	if ( StartsWithNoCase( name, "trigger_" ) ) return Category::Trigger;
	if ( IsBrushModel( entity ) || StartsWithNoCase( name, "func_" ) ) return Category::Brush;
	if ( StartsWithNoCase( name, "target_" ) || StartsWithNoCase( name, "info_" )
		|| StartsWithNoCase( name, "path_" ) || StartsWithNoCase( name, "script_" ) ) return Category::Logic;
	if ( StartsWithNoCase( name, "misc_" ) ) return Category::Misc;
	return Category::Other;
}

[[nodiscard]] inline const char *CategoryName( Category category ) noexcept
{
	switch ( category ) {
	case Category::World: return "world";
	case Category::Spawn: return "spawn";
	case Category::Light: return "light";
	case Category::Item: return "item";
	case Category::Trigger: return "trigger";
	case Category::Logic: return "logic";
	case Category::Brush: return "brush";
	case Category::Misc: return "misc";
	case Category::Other: return "other";
	}
	return "other";
}

/* A key that names another entity's targetname: target, target2 to
 * target9, and every key ending in "target" (killtarget, deathtarget,
 * paintarget...). */
[[nodiscard]] inline bool IsTargetKey( std::string_view key ) noexcept
{
	if ( key.size() >= 6 && entities::EqualsNoCase( key.substr( key.size() - 6 ), "target" ) ) {
		return true;
	}
	return key.size() == 7 && entities::StartsWithNoCase( key, "target" ) && key[6] >= '2' && key[6] <= '9';
}

/* A key that decides whether an entity spawns, or as what: its classname,
 * its spawnflags, and the game-type filters the Quake III line's games test
 * before spawning one (gametype, and notsingle, notteam, notfree, notq3a,
 * notta, notduel, not_gametype and the like). */
[[nodiscard]] inline bool DecidesSpawn( std::string_view key ) noexcept
{
	return entities::EqualsNoCase( key, "classname" ) || entities::EqualsNoCase( key, "spawnflags" )
		|| entities::EqualsNoCase( key, "gametype" ) || entities::StartsWithNoCase( key, "not" );
}

/* Whether two entity lists spawn the same entities in the same order: as
 * many, and in each the same keys that decide spawning with the same values
 * (a classname in any case, as games compare them). A Quake III-line game
 * restarted with map_restart keeps its client game's state for each entity
 * number, which only lists that spawn alike leave valid. */
[[nodiscard]] inline bool SpawnsAlike( const std::vector<Entity> &running, const std::vector<Entity> &edited )
{
	if ( running.size() != edited.size() ) {
		return false;
	}
	const auto deciding = []( const Entity &entity ) {
		std::vector<const KeyValue *> pairs;
		for ( const KeyValue &pair : entity.pairs ) {
			if ( DecidesSpawn( pair.key ) ) {
				pairs.push_back( &pair );
			}
		}
		return pairs;
	};
	for ( std::size_t i = 0; i < running.size(); i++ ) {
		const std::vector<const KeyValue *> a = deciding( running[i] ), b = deciding( edited[i] );
		if ( a.size() != b.size() ) {
			return false;
		}
		for ( std::size_t k = 0; k < a.size(); k++ ) {
			const bool classname = entities::EqualsNoCase( a[k]->key, "classname" );
			if ( !entities::EqualsNoCase( a[k]->key, b[k]->key )
				|| ( classname ? !entities::EqualsNoCase( a[k]->value, b[k]->value ) : a[k]->value != b[k]->value ) ) {
				return false;
			}
		}
	}
	return true;
}

/* The point an entity stands at: its origin key, read as G_SpawnVector reads
 * it. False without one. */
inline bool EntityOrigin( const Entity &entity, float out[3] ) noexcept
{
	out[0] = out[1] = out[2] = 0.0f;
	const int index = entity.IndexOf( "origin" );
	if ( index < 0 ) {
		return false;
	}
	entities::ParseVector( entity.pairs[static_cast<std::size_t>( index )].value, out );
	return true;
}

/* Whether an "angle" key points up or down rather than naming a yaw: -1 is
 * up and -2 down to Quake III's G_SetMovedir and its successors', and to
 * Ritual's G_GetMovedir (a door, a plat or a push set so moves vertically). */
[[nodiscard]] inline bool AngleIsVertical( float angle ) noexcept
{
	return angle == -1.0f || angle == -2.0f;
}

/* The yaw an entity faces: its "angles" key's second component, else its
 * "angle" key, as Quake III's G_SetMovedir and spawn code read them. False
 * without either, or for an "angle" that points up or down. */
inline bool EntityYaw( const Entity &entity, float &yaw ) noexcept
{
	float angles[3] = { 0.0f, 0.0f, 0.0f };
	const int anglesIndex = entity.IndexOf( "angles" );
	if ( anglesIndex >= 0 ) {
		entities::ParseVector( entity.pairs[static_cast<std::size_t>( anglesIndex )].value, angles );
		yaw = angles[1];
		return true;
	}
	const int angleIndex = entity.IndexOf( "angle" );
	if ( angleIndex >= 0 ) {
		entities::ParseVector( entity.pairs[static_cast<std::size_t>( angleIndex )].value, angles );
		if ( AngleIsVertical( angles[0] ) ) {
			return false;
		}
		yaw = angles[0];
		return true;
	}
	return false;
}

/* Where a game stands an entity about its origin. The Quake III line (Quake
 * III, Quake Live, the Q3Test and IHV releases, Elite Force and the Jedi
 * games) and Wolfenstein's (Return to Castle Wolfenstein's two modes, Enemy
 * Territory) centre most entities on their origins, Wolfenstein with a taller
 * player and its items set on theirs; Ritual's engines and 2015's (Alice,
 * F.A.K.K.2, Medal of Honor) stand an entity on its origin. */
enum class Lineage {
	Quake3,
	Wolfenstein,
	Ritual
};

/* A box about an entity's origin. */
struct Extent {
	float mins[3] = { -8.0f, -8.0f, -8.0f };
	float maxs[3] = { 8.0f, 8.0f, 8.0f };
};

[[nodiscard]] constexpr Extent MakeExtent( float x0, float y0, float z0, float x1, float y1, float z1 ) noexcept
{
	Extent extent{};
	extent.mins[0] = x0;
	extent.mins[1] = y0;
	extent.mins[2] = z0;
	extent.maxs[0] = x1;
	extent.maxs[1] = y1;
	extent.maxs[2] = z1;
	return extent;
}

/*
A point entity's box about its origin, as its game spawns it or, where the
game sizes it from a model the editor does not read, as the game's own entity
definitions (the QUAKED comments of its source, which level editors draw) do:

  - a spawn point, the player it stands: Quake III's (-16 -16 -24)-(16 16 32),
    which Elite Force's and the Jedi games' definitions share; Wolfenstein's
    player, (-18 -18 -24)-(18 18 48) (g_client.c's playerMins and playerMaxs,
    and Enemy Territory's definition); Ritual's and 2015's, standing on its
    origin, (-16 -16 0)-(16 16 96) (F.A.K.K.2's PlayerStart.cpp, OpenMoHAA's
    playerstart.cpp);
  - an item: Quake III's ITEM_RADIUS of 15 about it; Wolfenstein's of 10, set
    on its origin unless it hangs (spawnflags 1), as its FinishSpawningItem
    sets it; Ritual's and 2015's least item box, (-10 -10 0)-(10 10 20)
    (Item::Item);
  - a light, a small box about it;
  - the rest, a box of 16 about the origin; in Ritual's and 2015's games,
    whose models stand on their origins, set on it, but for the points their
    definitions centre (info_, target_ and path_ points, scripts' origins,
    triggers, and their func_ points) and their path nodes' own boxes.
*/
[[nodiscard]] inline Extent PointExtent( Lineage lineage, const Entity &entity ) noexcept
{
	using entities::EqualsNoCase;
	using entities::StartsWithNoCase;
	const std::string_view name = entity.Classname();
	switch ( Classify( entity ) ) {
	case Category::Spawn:
		switch ( lineage ) {
		case Lineage::Quake3: return MakeExtent( -16.0f, -16.0f, -24.0f, 16.0f, 16.0f, 32.0f );
		case Lineage::Wolfenstein: return MakeExtent( -18.0f, -18.0f, -24.0f, 18.0f, 18.0f, 48.0f );
		case Lineage::Ritual: return MakeExtent( -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 96.0f );
		}
		break;
	case Category::Item:
		switch ( lineage ) {
		case Lineage::Quake3: return MakeExtent( -15.0f, -15.0f, -15.0f, 15.0f, 15.0f, 15.0f );
		case Lineage::Wolfenstein:
			return ( std::atoi( std::string( entity.Value( "spawnflags" ) ).c_str() ) & 1 )
				? MakeExtent( -10.0f, -10.0f, -10.0f, 10.0f, 10.0f, 10.0f )
				: MakeExtent( -10.0f, -10.0f, 0.0f, 10.0f, 10.0f, 10.0f );
		case Lineage::Ritual: return MakeExtent( -10.0f, -10.0f, 0.0f, 10.0f, 10.0f, 20.0f );
		}
		break;
	case Category::Light:
		return MakeExtent( -6.0f, -6.0f, -6.0f, 6.0f, 6.0f, 6.0f );
	case Category::Logic:
	case Category::Trigger:
	case Category::Brush:
		if ( lineage == Lineage::Ritual ) {
			if ( EqualsNoCase( name, "info_pathnode" ) ) {
				return MakeExtent( -24.0f, -24.0f, 0.0f, 24.0f, 24.0f, 32.0f );
			}
			if ( EqualsNoCase( name, "info_grav_pathnode" ) ) {
				return MakeExtent( -16.0f, -16.0f, 0.0f, 16.0f, 16.0f, 32.0f );
			}
			if ( StartsWithNoCase( name, "script_model" ) ) {
				return MakeExtent( -8.0f, -8.0f, 0.0f, 8.0f, 8.0f, 16.0f );
			}
		}
		break;
	case Category::World:
	case Category::Misc:
	case Category::Other:
		if ( lineage == Lineage::Ritual ) {
			return MakeExtent( -8.0f, -8.0f, 0.0f, 8.0f, 8.0f, 16.0f );
		}
		break;
	}
	return Extent{};
}

/* A field of view `horizontal` degrees wide across a 4:3 screen, on a screen
 * whose shape is `aspect`: its height kept and its width fitted to the
 * shape, as the renderers' r_fovCorrection fits a game's 4:3 field
 * (tr_q3_fov.h's R_CorrectQ3ViewportFov), so that 90 on a 16:9 screen is
 * the 106.26 by 73.74 degrees a game's 90 is drawn at there. */
inline void FitFieldOfView( double horizontal, double aspect, float &fovX, float &fovY ) noexcept
{
	constexpr double kDegreesPerRadian = 57.2957795130823208768;
	if ( !( horizontal > 0.0 && horizontal < 180.0 ) ) {
		horizontal = 90.0;
	}
	if ( !( aspect > 0.0 && aspect < 1.0e6 ) ) {
		aspect = 4.0 / 3.0;
	}
	const double tanY = std::tan( horizontal / ( 2.0 * kDegreesPerRadian ) ) * 0.75;
	const double tanX = tanY * aspect;
	fovX = static_cast<float>( std::min( 2.0 * kDegreesPerRadian * std::atan( tanX ), 178.0 ) );
	fovY = static_cast<float>( std::min( 2.0 * kDegreesPerRadian * std::atan( tanY ), 178.0 ) );
}

[[nodiscard]] inline float SnapToGrid( float value, float grid ) noexcept
{
	if ( !( grid > 0.0f ) ) {
		return value;
	}
	return std::floor( value / grid + 0.5f ) * grid;
}

/* What the editor reports about one entity. */
struct Finding {
	bool error = false;			/* a game module would reject it */
	std::string text;
};

[[nodiscard]] inline std::vector<Finding> CheckEntity( const Entity &entity, const SpawnLimits &limits,
	bool first )
{
	std::vector<Finding> findings;
	int chars = 0;
	for ( std::size_t i = 0; i < entity.pairs.size(); i++ ) {
		const KeyValue &pair = entity.pairs[i];
		const entities::TextProblem keyProblem = entities::CheckToken( pair.key, true );
		if ( keyProblem != entities::TextProblem::None ) {
			findings.push_back( { keyProblem != entities::TextProblem::TooLong,
				"key \"" + pair.key + "\": " + entities::DescribeProblem( keyProblem ) } );
		}
		const entities::TextProblem valueProblem = entities::CheckToken( pair.value, false );
		if ( valueProblem != entities::TextProblem::None ) {
			findings.push_back( { valueProblem != entities::TextProblem::TooLong,
				"value of \"" + pair.key + "\": " + entities::DescribeProblem( valueProblem ) } );
		}
		for ( std::size_t j = 0; j < i; j++ ) {
			if ( entities::EqualsNoCase( entity.pairs[j].key, pair.key ) ) {
				findings.push_back( { false, "\"" + pair.key + "\" is set twice; games read the first" } );
				break;
			}
		}
		chars += static_cast<int>( std::min<std::size_t>( pair.key.size(), entities::kTokenBytes - 1 ) ) + 1;
		chars += static_cast<int>( std::min<std::size_t>( pair.value.size(), entities::kTokenBytes - 1 ) ) + 1;
	}
	if ( limits.maxPairs > 0 && static_cast<int>( entity.pairs.size() ) > limits.maxPairs ) {
		findings.push_back( { true, std::to_string( entity.pairs.size() ) + " keys; this game takes at most "
			+ std::to_string( limits.maxPairs ) } );
	}
	if ( limits.maxChars > 0 && chars > limits.maxChars ) {
		findings.push_back( { true, std::to_string( chars ) + " bytes of keys and values; this game takes at most "
			+ std::to_string( limits.maxChars ) } );
	}
	if ( !entity.Has( "classname" ) ) {
		findings.push_back( { false, "no classname: games spawn nothing for it" } );
	}
	const bool world = entities::EqualsNoCase( entity.Classname(), "worldspawn" );
	if ( first && !world ) {
		findings.push_back( { true, "the first entity must be the worldspawn" } );
	} else if ( !first && world ) {
		findings.push_back( { true, "only the first entity may be a worldspawn" } );
	}
	return findings;
}

class Document {
public:
	static constexpr std::size_t kHistoryLimit = 256;

	/* A new document: identities restart, the history empties, and the
	 * text counts as saved. */
	void Load( std::vector<Entity> loaded )
	{
		items_.clear();
		nextId_ = 1;
		for ( Entity &entity : loaded ) {
			items_.push_back( Item{ nextId_++, std::move( entity ) } );
		}
		selected_ = 0;
		selection_.clear();
		undo_.clear();
		redo_.clear();
		revision_ = 0;
		savedRevision_ = 0;
		nextRevision_ = 1;
	}

	[[nodiscard]] const std::vector<Item> &Items() const noexcept { return items_; }
	[[nodiscard]] std::size_t Count() const noexcept { return items_.size(); }

	[[nodiscard]] int IndexOf( std::uint32_t id ) const noexcept
	{
		for ( std::size_t i = 0; i < items_.size(); i++ ) {
			if ( items_[i].id == id ) {
				return static_cast<int>( i );
			}
		}
		return -1;
	}

	[[nodiscard]] const Item *Find( std::uint32_t id ) const noexcept
	{
		const int index = IndexOf( id );
		return index >= 0 ? &items_[static_cast<std::size_t>( index )] : nullptr;
	}

	/* ---- the selection -------------------------------------------------- */
	/* The entities edits apply to. The primary one, the last chosen, is the
	 * one the keys panel shows and the gizmo stands on. */

	[[nodiscard]] std::uint32_t Selected() const noexcept { return selected_; }
	[[nodiscard]] const Item *SelectedItem() const noexcept { return Find( selected_ ); }
	[[nodiscard]] std::size_t SelectionCount() const noexcept { return selection_.size(); }

	[[nodiscard]] bool IsSelected( std::uint32_t id ) const noexcept
	{
		return id && std::find( selection_.begin(), selection_.end(), id ) != selection_.end();
	}

	/* The selected entities, in list order. */
	[[nodiscard]] std::vector<std::uint32_t> Selection() const
	{
		std::vector<std::uint32_t> ids;
		ids.reserve( selection_.size() );
		for ( const Item &item : items_ ) {
			if ( IsSelected( item.id ) ) {
				ids.push_back( item.id );
			}
		}
		return ids;
	}

	/* This entity alone, or none for 0. */
	void Select( std::uint32_t id )
	{
		selection_.clear();
		selected_ = Find( id ) ? id : 0;
		if ( selected_ ) {
			selection_.push_back( selected_ );
		}
	}

	/* This entity too, as the primary one. */
	void AddToSelection( std::uint32_t id )
	{
		if ( !Find( id ) ) {
			return;
		}
		if ( !IsSelected( id ) ) {
			selection_.push_back( id );
		}
		selected_ = id;
	}

	/* In if it was out, as the primary one; out if it was in, the primary
	 * then the last chosen of those left. */
	void ToggleSelection( std::uint32_t id )
	{
		if ( !IsSelected( id ) ) {
			AddToSelection( id );
			return;
		}
		selection_.erase( std::find( selection_.begin(), selection_.end(), id ) );
		if ( selected_ == id ) {
			selected_ = selection_.empty() ? 0 : selection_.back();
		}
	}

	/* Every entity listed from one to the other, both included, added to the
	 * selection; `to` becomes the primary one. */
	void SelectRange( std::uint32_t from, std::uint32_t to )
	{
		int first = IndexOf( from ), last = IndexOf( to );
		if ( last < 0 ) {
			return;
		}
		if ( first < 0 ) {
			first = last;
		}
		const int step = first <= last ? 1 : -1;
		for ( int i = first; ; i += step ) {
			const std::uint32_t id = items_[static_cast<std::size_t>( i )].id;
			if ( !IsSelected( id ) ) {
				selection_.push_back( id );
			}
			if ( i == last ) {
				break;
			}
		}
		selected_ = to;
	}

	/* These entities, the primary one among them or else the last. */
	void SetSelection( const std::vector<std::uint32_t> &ids, std::uint32_t primary )
	{
		selection_.clear();
		for ( const std::uint32_t id : ids ) {
			if ( Find( id ) && !IsSelected( id ) ) {
				selection_.push_back( id );
			}
		}
		selected_ = IsSelected( primary ) ? primary : selection_.empty() ? 0 : selection_.back();
	}

	[[nodiscard]] std::vector<Entity> Entities() const
	{
		std::vector<Entity> list;
		list.reserve( items_.size() );
		for ( const Item &item : items_ ) {
			list.push_back( item.entity );
		}
		return list;
	}

	[[nodiscard]] std::string Text() const { return entities::Serialize( Entities() ); }

	/* ---- history -------------------------------------------------------- */

	/* Before a change: the state as it stands goes on the undo stack. One
	 * checkpoint may cover many changes, as a drag's do. */
	void Checkpoint( std::string_view label )
	{
		undo_.push_back( Snapshot{ items_, selected_, selection_, revision_, std::string( label ) } );
		if ( undo_.size() > kHistoryLimit ) {
			undo_.erase( undo_.begin() );
		}
		redo_.clear();
		revision_ = nextRevision_++;
	}

	/* A change after its checkpoint, of a drag still in progress. */
	void Touch() noexcept { revision_ = nextRevision_++; }

	/* Bumped by every change, undo and redo, so a view can tell when what
	 * it computed from the document went stale. */
	[[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }

	[[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
	[[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }
	[[nodiscard]] std::string_view UndoLabel() const noexcept
	{
		return undo_.empty() ? std::string_view() : std::string_view( undo_.back().label );
	}
	[[nodiscard]] std::string_view RedoLabel() const noexcept
	{
		return redo_.empty() ? std::string_view() : std::string_view( redo_.back().label );
	}

	bool Undo()
	{
		if ( undo_.empty() ) {
			return false;
		}
		redo_.push_back( Snapshot{ items_, selected_, selection_, revision_, undo_.back().label } );
		Restore( undo_.back() );
		undo_.pop_back();
		return true;
	}

	bool Redo()
	{
		if ( redo_.empty() ) {
			return false;
		}
		undo_.push_back( Snapshot{ items_, selected_, selection_, revision_, redo_.back().label } );
		Restore( redo_.back() );
		redo_.pop_back();
		return true;
	}

	/* Withdraws the last checkpoint when nothing changed after it, as a
	 * click on a gizmo that never moves leaves nothing to undo. */
	void DropCheckpointIfUnchanged()
	{
		if ( !undo_.empty() && undo_.back().items == items_ ) {
			revision_ = undo_.back().revision;
			undo_.pop_back();
		}
	}

	[[nodiscard]] bool Dirty() const noexcept { return revision_ != savedRevision_; }
	void MarkSaved() noexcept { savedRevision_ = revision_; }

	/* ---- edits ---------------------------------------------------------- */
	/* Each takes a checkpoint first, and returns false, untouched, when it
	 * cannot apply. */

	/* Every entity at once, as one change that can be undone: the text
	 * reverted to, or the BSP's own. */
	void Replace( std::vector<Entity> replacement, std::string_view label )
	{
		Checkpoint( label );
		items_.clear();
		for ( Entity &entity : replacement ) {
			items_.push_back( Item{ nextId_++, std::move( entity ) } );
		}
		Select( 0 );
	}

	/* Whether the document holds these entities, key for key. */
	[[nodiscard]] bool Holds( const std::vector<Entity> &list ) const noexcept
	{
		if ( list.size() != items_.size() ) {
			return false;
		}
		for ( std::size_t i = 0; i < list.size(); i++ ) {
			if ( list[i] != items_[i].entity ) {
				return false;
			}
		}
		return true;
	}

	bool SetValue( std::uint32_t id, std::string_view key, std::string_view value )
	{
		Item *item = Mutable( id );
		if ( !item || entities::CheckToken( key, true ) != entities::TextProblem::None
			|| !Writable( value ) ) {
			return false;
		}
		const int index = item->entity.IndexOf( key );
		if ( index >= 0 && item->entity.pairs[static_cast<std::size_t>( index )].value == value ) {
			return true;
		}
		Checkpoint( "set " + std::string( key ) );
		item = Mutable( id );
		item->entity.Set( key, value );
		return true;
	}

	/* The pair at `pair`, by position, so a duplicated key can be reached. */
	bool SetPairValue( std::uint32_t id, std::size_t pair, std::string_view value )
	{
		Item *item = Mutable( id );
		if ( !item || pair >= item->entity.pairs.size() || !Writable( value ) ) {
			return false;
		}
		if ( item->entity.pairs[pair].value == value ) {
			return true;
		}
		Checkpoint( "set " + item->entity.pairs[pair].key );
		Mutable( id )->entity.pairs[pair].value.assign( value );
		return true;
	}

	bool RenameKey( std::uint32_t id, std::size_t pair, std::string_view key )
	{
		Item *item = Mutable( id );
		if ( !item || pair >= item->entity.pairs.size()
			|| entities::CheckToken( key, true ) != entities::TextProblem::None ) {
			return false;
		}
		if ( item->entity.pairs[pair].key == key ) {
			return true;
		}
		Checkpoint( "rename " + item->entity.pairs[pair].key );
		Mutable( id )->entity.pairs[pair].key.assign( key );
		return true;
	}

	bool RemovePair( std::uint32_t id, std::size_t pair )
	{
		Item *item = Mutable( id );
		if ( !item || pair >= item->entity.pairs.size() ) {
			return false;
		}
		Checkpoint( "remove " + item->entity.pairs[pair].key );
		item = Mutable( id );
		item->entity.pairs.erase( item->entity.pairs.begin() + static_cast<std::ptrdiff_t>( pair ) );
		return true;
	}

	bool AddPair( std::uint32_t id, std::string_view key, std::string_view value )
	{
		Item *item = Mutable( id );
		if ( !item || entities::CheckToken( key, true ) != entities::TextProblem::None || !Writable( value ) ) {
			return false;
		}
		Checkpoint( "add " + std::string( key ) );
		Mutable( id )->entity.pairs.push_back( KeyValue{ std::string( key ), std::string( value ) } );
		return true;
	}

	/* A new point entity at `origin`, listed last and selected. */
	std::uint32_t Create( std::string_view classname, const float origin[3] )
	{
		if ( entities::CheckToken( classname, true ) != entities::TextProblem::None ) {
			return 0;
		}
		Checkpoint( "new " + std::string( classname ) );
		Entity entity;
		entity.pairs.push_back( KeyValue{ "classname", std::string( classname ) } );
		if ( origin ) {
			entity.pairs.push_back( KeyValue{ "origin", entities::FormatVector( origin ) } );
		}
		items_.push_back( Item{ nextId_++, std::move( entity ) } );
		Select( items_.back().id );
		return selected_;
	}

	/* A copy listed after the original and selected, its origin moved by
	 * `offset`. A brush entity's copy shares the original's brush model,
	 * which only one entity may own, so it cannot be copied. */
	std::uint32_t Duplicate( std::uint32_t id, const float offset[3] )
	{
		const int index = IndexOf( id );
		if ( index <= 0 ) {
			return 0;		/* nothing, or the worldspawn */
		}
		const Item &original = items_[static_cast<std::size_t>( index )];
		if ( IsBrushModel( original.entity ) ) {
			return 0;
		}
		Checkpoint( "duplicate " + std::string( original.entity.Classname() ) );
		Item copy{ nextId_++, items_[static_cast<std::size_t>( index )].entity };
		copy.entity.line = 0;
		float origin[3];
		if ( offset && EntityOrigin( copy.entity, origin ) ) {
			for ( int axis = 0; axis < 3; axis++ ) {
				origin[axis] += offset[axis];
			}
			copy.entity.Set( "origin", entities::FormatVector( origin ) );
		}
		items_.insert( items_.begin() + index + 1, std::move( copy ) );
		Select( items_[static_cast<std::size_t>( index ) + 1].id );
		return selected_;
	}

	/* A copy of each selected entity that can be copied -- not the
	 * worldspawn, not a brush entity -- listed after its original, moved by
	 * `offset`, as one change; the copies become the selection, the primary
	 * one's copy primary. How many were made. */
	std::size_t DuplicateSelection( const float offset[3] )
	{
		std::vector<std::uint32_t> originals;
		for ( const std::uint32_t id : Selection() ) {
			const int index = IndexOf( id );
			if ( index > 0 && !IsBrushModel( items_[static_cast<std::size_t>( index )].entity ) ) {
				originals.push_back( id );
			}
		}
		if ( originals.empty() ) {
			return 0;
		}
		Checkpoint( originals.size() == 1
			? "duplicate " + std::string( Find( originals[0] )->entity.Classname() )
			: "duplicate " + std::to_string( originals.size() ) + " entities" );
		const std::uint32_t primary = selected_;
		std::vector<std::uint32_t> copies;
		std::uint32_t primaryCopy = 0;
		for ( const std::uint32_t id : originals ) {
			const int index = IndexOf( id );
			Item copy{ nextId_++, items_[static_cast<std::size_t>( index )].entity };
			copy.entity.line = 0;
			float origin[3];
			if ( offset && EntityOrigin( copy.entity, origin ) ) {
				for ( int axis = 0; axis < 3; axis++ ) {
					origin[axis] += offset[axis];
				}
				copy.entity.Set( "origin", entities::FormatVector( origin ) );
			}
			copies.push_back( copy.id );
			if ( id == primary ) {
				primaryCopy = copy.id;
			}
			items_.insert( items_.begin() + index + 1, std::move( copy ) );
		}
		SetSelection( copies, primaryCopy );
		return copies.size();
	}

	/* Entities from elsewhere -- the clipboard, another map -- listed last as
	 * one change and selected, the last of them primary. A worldspawn is not
	 * taken, nor a brush entity, whose model would be another map's; how many
	 * were. A lone entity with an origin is put at `at` when given. */
	std::size_t Paste( std::vector<Entity> list, const float at[3] )
	{
		std::vector<Entity> taken;
		for ( Entity &entity : list ) {
			if ( entities::EqualsNoCase( entity.Classname(), "worldspawn" ) || IsBrushModel( entity ) ) {
				continue;
			}
			bool writable = true;
			for ( const KeyValue &pair : entity.pairs ) {
				writable = writable && entities::CheckToken( pair.key, true ) == entities::TextProblem::None
					&& Writable( pair.value );
			}
			if ( writable ) {
				entity.line = 0;
				taken.push_back( std::move( entity ) );
			}
		}
		if ( taken.empty() ) {
			return 0;
		}
		Checkpoint( taken.size() == 1 ? "paste " + std::string( taken[0].Classname() ) : std::string( "paste" ) );
		float origin[3];
		if ( taken.size() == 1 && at && EntityOrigin( taken[0], origin ) ) {
			taken[0].Set( "origin", entities::FormatVector( at ) );
		}
		std::vector<std::uint32_t> pasted;
		for ( Entity &entity : taken ) {
			items_.push_back( Item{ nextId_++, std::move( entity ) } );
			pasted.push_back( items_.back().id );
		}
		SetSelection( pasted, pasted.back() );
		return taken.size();
	}

	/* The worldspawn stays. A selected entity deleted leaves the selection;
	 * when that empties it, the entity listed before is selected. */
	bool Delete( std::uint32_t id )
	{
		const int index = IndexOf( id );
		if ( index <= 0 ) {
			return false;
		}
		Checkpoint( "delete " + std::string( items_[static_cast<std::size_t>( index )].entity.Classname() ) );
		const bool wasSelected = IsSelected( id );
		items_.erase( items_.begin() + index );
		Forget( id );
		if ( wasSelected && selection_.empty() ) {
			Select( items_[static_cast<std::size_t>( index - 1 )].id );
		}
		return true;
	}

	/* Every selected entity but the worldspawn, as one change; the entity
	 * listed before the first of them is selected then. How many went. */
	std::size_t DeleteSelection()
	{
		std::vector<std::uint32_t> doomed;
		for ( const std::uint32_t id : Selection() ) {
			if ( IndexOf( id ) > 0 ) {
				doomed.push_back( id );
			}
		}
		if ( doomed.empty() ) {
			return 0;
		}
		const int first = IndexOf( doomed.front() );
		Checkpoint( doomed.size() == 1
			? "delete " + std::string( Find( doomed[0] )->entity.Classname() )
			: "delete " + std::to_string( doomed.size() ) + " entities" );
		for ( const std::uint32_t id : doomed ) {
			items_.erase( items_.begin() + IndexOf( id ) );
		}
		Select( items_[static_cast<std::size_t>( first - 1 )].id );
		return doomed.size();
	}

	/* A key set on every selected entity, as one change; false, untouched,
	 * when the key or value cannot be written or nothing is selected. */
	bool SetValueOnSelection( std::string_view key, std::string_view value )
	{
		if ( selection_.empty() || entities::CheckToken( key, true ) != entities::TextProblem::None
			|| !Writable( value ) ) {
			return false;
		}
		Checkpoint( "set " + std::string( key ) + ( selection_.size() > 1 ? " on " + std::to_string( selection_.size() ) : "" ) );
		for ( const std::uint32_t id : selection_ ) {
			Mutable( id )->entity.Set( key, value );
		}
		return true;
	}

	/* A key removed from every selected entity that has it, as one change;
	 * how many lost it. */
	std::size_t RemoveKeyFromSelection( std::string_view key )
	{
		std::vector<std::uint32_t> holders;
		for ( const std::uint32_t id : Selection() ) {
			if ( Find( id )->entity.IndexOf( key ) >= 0 ) {
				holders.push_back( id );
			}
		}
		if ( holders.empty() ) {
			return 0;
		}
		Checkpoint( "remove " + std::string( key ) );
		for ( const std::uint32_t id : holders ) {
			Mutable( id )->entity.Remove( key );
		}
		return holders.size();
	}

	/* Each selected entity but the worldspawn and those whose "angle" points
	 * up or down turned by `degrees`, as one change (Rotate); how many
	 * turned. */
	std::size_t RotateSelection( float degrees )
	{
		std::vector<std::uint32_t> turning;
		for ( const std::uint32_t id : Selection() ) {
			if ( IndexOf( id ) > 0 && Turns( Find( id )->entity ) ) {
				turning.push_back( id );
			}
		}
		if ( turning.empty() ) {
			return 0;
		}
		Checkpoint( "rotate" );
		for ( const std::uint32_t id : turning ) {
			Turn( *Mutable( id ), degrees );
		}
		return turning.size();
	}

	/* The origin key, without a checkpoint: the caller took one, as a drag
	 * takes one when it starts. */
	bool PlaceWithoutCheckpoint( std::uint32_t id, const float origin[3] )
	{
		Item *item = Mutable( id );
		if ( !item ) {
			return false;
		}
		item->entity.Set( "origin", entities::FormatVector( origin ) );
		Touch();
		return true;
	}

	bool Place( std::uint32_t id, const float origin[3] )
	{
		if ( !Find( id ) ) {
			return false;
		}
		Checkpoint( "move" );
		return PlaceWithoutCheckpoint( id, origin );
	}

	/* Turns by `degrees` about the vertical: the "angles" key's yaw where it
	 * has one, else its "angle" key, made when absent; an "angle" that
	 * points up or down stays. */
	bool Rotate( std::uint32_t id, float degrees )
	{
		if ( !Find( id ) || IndexOf( id ) == 0 || !Turns( Find( id )->entity ) ) {
			return false;
		}
		Checkpoint( "rotate" );
		Turn( *Mutable( id ), degrees );
		return true;
	}

	/* The entities whose targetname one of `id`'s target keys names, and
	 * those whose target keys name `id`'s targetname. */
	void Links( std::uint32_t id, std::vector<std::uint32_t> &targets, std::vector<std::uint32_t> &sources ) const
	{
		targets.clear();
		sources.clear();
		const Item *item = Find( id );
		if ( !item ) {
			return;
		}
		const std::string_view name = item->entity.Value( "targetname" );
		for ( const Item &other : items_ ) {
			if ( other.id == id ) {
				continue;
			}
			const std::string_view otherName = other.entity.Value( "targetname" );
			bool targeted = false, targeting = false;
			if ( !otherName.empty() ) {
				for ( const KeyValue &pair : item->entity.pairs ) {
					if ( IsTargetKey( pair.key ) && pair.value == otherName ) {
						targeted = true;
						break;
					}
				}
			}
			if ( !name.empty() ) {
				for ( const KeyValue &pair : other.entity.pairs ) {
					if ( IsTargetKey( pair.key ) && pair.value == name ) {
						targeting = true;
						break;
					}
				}
			}
			if ( targeted ) targets.push_back( other.id );
			if ( targeting ) sources.push_back( other.id );
		}
	}

	/* Whether an entity matches a filter: each of its words appears in some
	 * key or value, ignoring case; a word "key=value" needs one pair whose
	 * key holds the one and whose value holds the other. */
	[[nodiscard]] static bool Matches( const Entity &entity, std::string_view filter ) noexcept
	{
		std::size_t start = 0;
		while ( start < filter.size() ) {
			while ( start < filter.size() && filter[start] == ' ' ) start++;
			std::size_t end = start;
			while ( end < filter.size() && filter[end] != ' ' ) end++;
			if ( end > start ) {
				const std::string_view word = filter.substr( start, end - start );
				const std::size_t equals = word.find( '=' );
				bool found = false;
				for ( const KeyValue &pair : entity.pairs ) {
					if ( equals != std::string_view::npos
						? ContainsNoCase( pair.key, word.substr( 0, equals ) )
							&& ContainsNoCase( pair.value, word.substr( equals + 1 ) )
						: ContainsNoCase( pair.key, word ) || ContainsNoCase( pair.value, word ) ) {
						found = true;
						break;
					}
				}
				if ( !found ) {
					return false;
				}
			}
			start = end;
		}
		return true;
	}

	/* The classnames listed, each once, the most used first. */
	[[nodiscard]] std::vector<std::string> Classnames() const
	{
		std::vector<std::pair<std::string, int>> counts;
		for ( const Item &item : items_ ) {
			const std::string_view name = item.entity.Classname();
			if ( name.empty() ) {
				continue;
			}
			auto found = std::find_if( counts.begin(), counts.end(),
				[name]( const std::pair<std::string, int> &entry ) { return entities::EqualsNoCase( entry.first, name ); } );
			if ( found != counts.end() ) {
				found->second++;
			} else {
				counts.emplace_back( std::string( name ), 1 );
			}
		}
		std::stable_sort( counts.begin(), counts.end(),
			[]( const auto &a, const auto &b ) { return a.second > b.second; } );
		std::vector<std::string> names;
		for ( auto &entry : counts ) {
			names.push_back( std::move( entry.first ) );
		}
		return names;
	}

private:
	struct Snapshot {
		std::vector<Item> items;
		std::uint32_t selected = 0;
		std::vector<std::uint32_t> selection;
		std::uint64_t revision = 0;
		std::string label;
	};

	[[nodiscard]] static bool Writable( std::string_view value ) noexcept
	{
		const entities::TextProblem problem = entities::CheckToken( value, false );
		return problem == entities::TextProblem::None || problem == entities::TextProblem::TooLong;
	}

	Item *Mutable( std::uint32_t id ) noexcept
	{
		const int index = IndexOf( id );
		return index >= 0 ? &items_[static_cast<std::size_t>( index )] : nullptr;
	}

	void Restore( const Snapshot &snapshot )
	{
		items_ = snapshot.items;
		SetSelection( snapshot.selection, snapshot.selected );
		revision_ = snapshot.revision;
	}

	/* Whether turning changes an entity's facing: not when it has no
	 * "angles" key and its "angle" key points up or down -- a turned -1
	 * would no longer be up. */
	[[nodiscard]] static bool Turns( const Entity &entity ) noexcept
	{
		if ( entity.Has( "angles" ) ) {
			return true;
		}
		const int angleIndex = entity.IndexOf( "angle" );
		if ( angleIndex < 0 ) {
			return true;
		}
		float angle[3] = { 0.0f, 0.0f, 0.0f };
		entities::ParseVector( entity.pairs[static_cast<std::size_t>( angleIndex )].value, angle );
		return !AngleIsVertical( angle[0] );
	}

	/* The "angles" key's yaw turned where it has one, else its "angle" key,
	 * made when absent. */
	static void Turn( Item &item, float degrees )
	{
		const auto wrap = []( float value ) {
			value = std::fmod( value, 360.0f );
			return value < 0.0f ? value + 360.0f : value;
		};
		const int anglesIndex = item.entity.IndexOf( "angles" );
		if ( anglesIndex >= 0 ) {
			float angles[3] = { 0.0f, 0.0f, 0.0f };
			entities::ParseVector( item.entity.pairs[static_cast<std::size_t>( anglesIndex )].value, angles );
			angles[1] = wrap( angles[1] + degrees );
			item.entity.pairs[static_cast<std::size_t>( anglesIndex )].value = entities::FormatVector( angles );
			return;
		}
		float yaw = 0.0f;
		EntityYaw( item.entity, yaw );
		item.entity.Set( "angle", entities::FormatNumber( wrap( yaw + degrees ) ) );
	}

	/* An entity gone from the list leaves the selection; a primary one gone
	 * hands that role to the last chosen of those left. */
	void Forget( std::uint32_t id )
	{
		const auto found = std::find( selection_.begin(), selection_.end(), id );
		if ( found != selection_.end() ) {
			selection_.erase( found );
		}
		if ( selected_ == id ) {
			selected_ = selection_.empty() ? 0 : selection_.back();
		}
	}

	std::vector<Item> items_;
	std::uint32_t nextId_ = 1;
	std::uint32_t selected_ = 0;
	std::vector<std::uint32_t> selection_;
	std::vector<Snapshot> undo_;
	std::vector<Snapshot> redo_;
	std::uint64_t revision_ = 0;
	std::uint64_t savedRevision_ = 0;
	std::uint64_t nextRevision_ = 1;
};

} // namespace fntech3::entity_editor

#endif // FNTECH3_CLIENT_ENTITY_EDITOR_MODEL_HPP
