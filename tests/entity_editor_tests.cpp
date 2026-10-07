/*
 * The entity editor's engine-independent parts: the entity text reader and
 * writer (code/qcommon/entity_text.hpp), the document with its history and
 * selection (code/client/entity_editor_model.hpp), and the drawing that
 * turns Dear ImGui's draw lists into the renderers' quads and pictures
 * (code/client/entity_editor_ui.hpp), driven by headless ImGui frames.
 */
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "../code/qcommon/entity_text.hpp"
#include "../code/client/entity_editor_model.hpp"
#include "../code/client/entity_editor_ui.hpp"

namespace {

int failures;

void Check( bool condition, const char *expression, int line )
{
	if ( condition ) return;
	std::cerr << "line " << line << ": check failed: " << expression << '\n';
	++failures;
}

#define CHECK(expression) Check( ( expression ), #expression, __LINE__ )

namespace et = fntech3::entities;
namespace ee = fntech3::entity_editor;
namespace ui = fntech3::entity_editor::ui;

std::vector<std::string> Tokens( std::string_view text )
{
	std::vector<std::string> tokens;
	et::Tokenizer tokenizer( text );
	et::Token token;
	while ( tokenizer.Next( token ) ) {
		tokens.push_back( token.text );
	}
	return tokens;
}

/* ---- the tokenizer: COM_ParseExt's rules -------------------------------- */

void TestTokenizer()
{
	CHECK( Tokens( "" ).empty() );
	CHECK( Tokens( "   \n\t " ).empty() );
	CHECK( ( Tokens( "{ \"classname\" \"worldspawn\" }" )
		== std::vector<std::string>{ "{", "classname", "worldspawn", "}" } ) );
	/* a quoted token keeps spaces and line breaks, and has no escapes */
	CHECK( ( Tokens( "\"a b\" \"c\nd\" \"e\\\\\"" ) == std::vector<std::string>{ "a b", "c\nd", "e\\\\" } ) );
	/* an empty quoted token is still a token */
	CHECK( ( Tokens( "\"\" x" ) == std::vector<std::string>{ "", "x" } ) );
	/* comments, but not inside quotes */
	CHECK( ( Tokens( "a // gone\nb /* gone\ntoo */ c \"// kept\" \"/* kept */\"" )
		== std::vector<std::string>{ "a", "b", "c", "// kept", "/* kept */" } ) );
	/* an unquoted token runs to the next whitespace, braces and quotes included */
	CHECK( ( Tokens( "{\"classname\" x}" ) == std::vector<std::string>{ "{\"classname\"", "x}" } ) );
	/* an unterminated quote ends with the text */
	CHECK( ( Tokens( "\"open" ) == std::vector<std::string>{ "open" } ) );
	/* an unterminated block comment swallows the rest */
	CHECK( Tokens( "/* never closed { \"a\" \"b\" }" ).empty() );
	/* the text ends at its first NUL */
	const std::string withNul( "a\0b", 3 );
	CHECK( ( Tokens( withNul ) == std::vector<std::string>{ "a" } ) );

	/* a token keeps 1023 bytes */
	/* the tokenizer reads the text where it lies, so the text outlives it */
	const std::string longText = "\"" + std::string( 2000, 'v' ) + "\" next";
	et::Tokenizer tokenizer( longText );
	et::Token token;
	CHECK( tokenizer.Next( token ) );
	CHECK( token.text.size() == et::kTokenBytes - 1 );
	CHECK( token.truncated );
	CHECK( tokenizer.Next( token ) && token.text == "next" && !token.truncated );

	/* line numbers count every line break, those in comments and quotes too */
	et::Tokenizer lines( "a\n\"b\nc\"\n/* \n */ d" );
	CHECK( lines.Next( token ) && token.line == 1 );
	CHECK( lines.Next( token ) && token.line == 2 );
	CHECK( lines.Next( token ) && token.text == "d" && token.line == 5 );

	/* a byte from 0x80 up is whitespace where char is signed, as it is to
	 * SkipWhitespace there, and part of a word where char is unsigned */
	const std::string high = std::string( "a" ) + static_cast<char>( 0x80 ) + "b";
	if constexpr ( std::numeric_limits<char>::is_signed ) {
		CHECK( ( Tokens( high ) == std::vector<std::string>{ "a", "b" } ) );
	} else {
		CHECK( ( Tokens( high ) == std::vector<std::string>{ high } ) );
	}
	const std::string quotedHigh = std::string( "\"" ) + static_cast<char>( 0xe9 ) + "\"";
	CHECK( ( Tokens( quotedHigh ) == std::vector<std::string>{ std::string( 1, static_cast<char>( 0xe9 ) ) } ) );
}

/* ---- the parser: G_ParseSpawnVars's rules ------------------------------- */

void TestParser()
{
	const char *text =
		"{\n\"classname\" \"worldspawn\"\n\"message\" \"Test Map\"\n}\n"
		"// a comment between entities\n"
		"{\n\"origin\" \"0 0 64\"\n\"classname\" \"info_player_deathmatch\"\n\"angle\" \"90\"\n}\n";
	const et::ParseResult parsed = et::Parse( text );
	CHECK( parsed.ok );
	CHECK( parsed.entities.size() == 2 );
	CHECK( parsed.entities[0].Classname() == "worldspawn" );
	CHECK( parsed.entities[0].line == 1 );
	CHECK( parsed.entities[1].line == 6 );
	CHECK( parsed.entities[1].pairs.size() == 3 );
	CHECK( parsed.entities[1].pairs[0].key == "origin" );	/* order kept */
	CHECK( parsed.entities[1].Value( "ANGLE" ) == "90" );		/* keys ignore case */

	/* errors where a game module would stop the level */
	et::ParseResult bad = et::Parse( "\"classname\" \"x\"" );
	CHECK( !bad.ok && bad.error.find( "should begin with {" ) != std::string::npos );
	bad = et::Parse( "{ \"classname\" \"worldspawn\"" );
	CHECK( !bad.ok && bad.error.find( "no closing brace" ) != std::string::npos && bad.entities.size() == 1 );
	bad = et::Parse( "{ \"classname\" }" );
	CHECK( !bad.ok && bad.error.find( "in place of the value" ) != std::string::npos );
	bad = et::Parse( "{ \"a\" \"b\" }\n{ \"key\"" );
	CHECK( !bad.ok && bad.errorLine == 2 && bad.entities.size() == 2 );
	/* a key that begins with '}', quoted or not, ends the entity */
	const et::ParseResult brace = et::Parse( "{ \"a\" \"b\" \"}x\" }" );
	CHECK( !brace.ok );		/* the trailing '}' is then a stray token */
	CHECK( brace.entities.size() == 1 && brace.entities[0].pairs.size() == 1 );
	/* an empty value is a value */
	const et::ParseResult empty = et::Parse( "{ \"classname\" \"worldspawn\" \"message\" \"\" }" );
	CHECK( empty.ok && empty.entities[0].Value( "message", "unset" ).empty() );
	/* nothing at all is no entities, and no error */
	CHECK( et::Parse( "  // nothing\n" ).ok && et::Parse( "" ).entities.empty() );
	/* a token past 1023 bytes is flagged */
	CHECK( et::Parse( "{ \"k\" \"" + std::string( 1500, 'x' ) + "\" }" ).truncated );
}

void TestSerializer()
{
	et::Entity world;
	world.pairs = { { "classname", "worldspawn" }, { "message", "a // b /* c */" } };
	et::Entity spawn;
	spawn.pairs = { { "classname", "info_player_start" }, { "origin", "-8 16 24" }, { "empty", "" } };
	const std::vector<et::Entity> list = { world, spawn };
	const std::string text = et::Serialize( list );
	CHECK( text == "{\n\"classname\" \"worldspawn\"\n\"message\" \"a // b /* c */\"\n}\n"
		"{\n\"classname\" \"info_player_start\"\n\"origin\" \"-8 16 24\"\n\"empty\" \"\"\n}\n" );
	const et::ParseResult back = et::Parse( text );
	CHECK( back.ok && back.entities == list );
	CHECK( et::Serialize( {} ).empty() );
}

void TestTextHelpers()
{
	CHECK( et::CheckToken( "origin", true ) == et::TextProblem::None );
	CHECK( et::CheckToken( "", true ) == et::TextProblem::Empty );
	CHECK( et::CheckToken( "", false ) == et::TextProblem::None );
	CHECK( et::CheckToken( "a\"b", false ) == et::TextProblem::Quote );
	CHECK( et::CheckToken( std::string( "a\0b", 3 ), false ) == et::TextProblem::Nul );
	CHECK( et::CheckToken( "}x", false ) == et::TextProblem::ClosingBrace );
	CHECK( et::CheckToken( "x}", false ) == et::TextProblem::None );
	CHECK( et::CheckToken( std::string( 1024, 'a' ), false ) == et::TextProblem::TooLong );
	CHECK( et::CheckToken( std::string( 1023, 'a' ), false ) == et::TextProblem::None );

	float v[3] = { 7.0f, 7.0f, 7.0f };
	CHECK( et::ParseVector( "1 2.5 -3", v ) == 3 && v[0] == 1.0f && v[1] == 2.5f && v[2] == -3.0f );
	v[2] = 7.0f;
	CHECK( et::ParseVector( "4 5", v ) == 2 && v[0] == 4.0f && v[2] == 7.0f );
	CHECK( et::ParseVector( "abc", v ) == 0 );
	CHECK( et::ParseVector( "  9 8 7 6", v ) == 3 && v[0] == 9.0f && v[2] == 7.0f );

	CHECK( et::FormatNumber( 0.0 ) == "0" );
	CHECK( et::FormatNumber( -0.0001 ) == "0" );
	CHECK( et::FormatNumber( 1.5 ) == "1.5" );
	CHECK( et::FormatNumber( 12.0 ) == "12" );
	CHECK( et::FormatNumber( -816.0 ) == "-816" );
	CHECK( et::FormatNumber( 1.0 / 3.0 ) == "0.333" );
	const float point[3] = { 64.0f, -32.5f, 0.0f };
	CHECK( et::FormatVector( point ) == "64 -32.5 0" );

	CHECK( et::EqualsNoCase( "ClassName", "classname" ) );
	CHECK( !et::EqualsNoCase( "classname", "classnam" ) );
	CHECK( et::StartsWithNoCase( "Weapon_Rocketlauncher", "weapon_" ) );

	et::Entity entity;
	entity.Set( "classname", "light" );
	entity.Set( "Light", "300" );
	entity.Set( "LIGHT", "400" );			/* the first occurrence takes it */
	CHECK( entity.pairs.size() == 2 && entity.Value( "light" ) == "400" );
	entity.pairs.push_back( { "light", "500" } );
	CHECK( entity.Value( "light" ) == "400" );	/* G_SpawnString reads the first */
	CHECK( entity.Remove( "LIGHT" ) == 2 && !entity.Has( "light" ) );
}

/* ---- the document --------------------------------------------------------- */

std::vector<et::Entity> Sample()
{
	return et::Parse(
		"{ \"classname\" \"worldspawn\" }\n"
		"{ \"classname\" \"info_player_deathmatch\" \"origin\" \"0 0 24\" \"angle\" \"90\" }\n"
		"{ \"classname\" \"trigger_multiple\" \"model\" \"*1\" \"target\" \"t1\" }\n"
		"{ \"classname\" \"target_delay\" \"targetname\" \"t1\" \"target\" \"t2\" \"origin\" \"64 0 0\" }\n"
		"{ \"classname\" \"target_speaker\" \"targetname\" \"t2\" \"origin\" \"128 0 0\" }\n"
		"{ \"classname\" \"func_door\" \"model\" \"*2\" \"killtarget\" \"t2\" }\n"
		"{ \"classname\" \"weapon_rocketlauncher\" \"origin\" \"256 0 8\" }\n"
		"{ \"classname\" \"info_player_deathmatch\" \"origin\" \"512 0 24\" }\n" ).entities;
}

void TestDocumentBasics()
{
	ee::Document doc;
	doc.Load( Sample() );
	CHECK( doc.Count() == 8 );
	CHECK( !doc.Dirty() && !doc.CanUndo() );
	std::vector<std::uint32_t> ids;
	for ( const ee::Item &item : doc.Items() ) {
		CHECK( item.id != 0 );
		for ( const std::uint32_t seen : ids ) CHECK( seen != item.id );
		ids.push_back( item.id );
	}
	doc.Select( ids[1] );
	CHECK( doc.Selected() == ids[1] && doc.SelectedItem()->entity.Classname() == "info_player_deathmatch" );
	doc.Select( 9999 );
	CHECK( doc.Selected() == 0 );
	CHECK( doc.Text() == et::Serialize( Sample() ) );
	CHECK( doc.Holds( Sample() ) );

	/* set, undo, redo and the saved state */
	const std::uint64_t before = doc.Revision();
	CHECK( doc.SetValue( ids[1], "angle", "180" ) );
	CHECK( doc.Revision() != before );
	CHECK( doc.Dirty() && doc.CanUndo() && doc.UndoLabel() == "set angle" );
	CHECK( doc.Find( ids[1] )->entity.Value( "angle" ) == "180" );
	CHECK( doc.SetValue( ids[1], "angle", "180" ) );	/* no change, no checkpoint */
	CHECK( doc.Undo() && doc.Find( ids[1] )->entity.Value( "angle" ) == "90" );
	CHECK( !doc.Dirty() && doc.CanRedo() );
	CHECK( doc.Redo() && doc.Find( ids[1] )->entity.Value( "angle" ) == "180" && doc.Dirty() );
	doc.MarkSaved();
	CHECK( !doc.Dirty() );
	CHECK( doc.Undo() && doc.Dirty() );
	CHECK( doc.Redo() && !doc.Dirty() );

	/* text a game module would misread is refused */
	CHECK( !doc.SetValue( ids[1], "message", "say \"hi\"" ) );
	CHECK( !doc.SetValue( ids[1], "}", "x" ) );
	CHECK( !doc.SetValue( ids[1], "message", "}x" ) );
	CHECK( !doc.AddPair( ids[1], "", "x" ) );
	CHECK( !doc.RenameKey( ids[1], 0, "with\"quote" ) );
	CHECK( !doc.SetValue( 9999, "a", "b" ) );

	/* rename, add and remove by position, so duplicated keys are reachable */
	CHECK( doc.AddPair( ids[1], "angle", "270" ) );
	const std::size_t last = doc.Find( ids[1] )->entity.pairs.size() - 1;
	CHECK( doc.SetPairValue( ids[1], last, "45" ) );
	CHECK( doc.Find( ids[1] )->entity.pairs[last].value == "45" );
	CHECK( doc.RenameKey( ids[1], last, "spawnflags" ) );
	CHECK( doc.Find( ids[1] )->entity.Value( "spawnflags" ) == "45" );
	CHECK( doc.RemovePair( ids[1], last ) && !doc.Find( ids[1] )->entity.Has( "spawnflags" ) );
	CHECK( !doc.RemovePair( ids[1], 99 ) );
}

void TestDocumentEntities()
{
	ee::Document doc;
	doc.Load( Sample() );
	const std::uint32_t world = doc.Items()[0].id;
	const std::uint32_t spawn = doc.Items()[1].id;
	const std::uint32_t trigger = doc.Items()[2].id;

	/* create: listed last, selected, origin written as a mapper would */
	const float at[3] = { 10.0f, -20.5f, 0.0f };
	const std::uint32_t made = doc.Create( "item_health", at );
	CHECK( made && doc.Selected() == made && doc.IndexOf( made ) == 8 );
	CHECK( doc.Find( made )->entity.Value( "origin" ) == "10 -20.5 0" );
	CHECK( !doc.Create( "bad\"name", at ) );

	/* duplicate: after the original, moved, selected; never the world or a
	 * brush entity, whose model is its own */
	const float offset[3] = { 16.0f, 16.0f, 0.0f };
	const std::uint32_t copy = doc.Duplicate( spawn, offset );
	CHECK( copy && doc.IndexOf( copy ) == 2 && doc.Selected() == copy );
	CHECK( doc.Find( copy )->entity.Value( "origin" ) == "16 16 24" );
	CHECK( doc.Find( copy )->entity.Value( "angle" ) == "90" );
	CHECK( !doc.Duplicate( world, offset ) );
	CHECK( !doc.Duplicate( trigger, offset ) );

	/* delete: never the world; the selection moves to the one before */
	CHECK( !doc.Delete( world ) );
	doc.Select( copy );
	CHECK( doc.Delete( copy ) && doc.Selected() == spawn && doc.IndexOf( copy ) < 0 );
	CHECK( doc.Undo() && doc.IndexOf( copy ) == 2 );	/* identities survive undo */

	/* place and rotate */
	const float there[3] = { 1.0f, 2.0f, 3.0f };
	CHECK( doc.Place( spawn, there ) && doc.Find( spawn )->entity.Value( "origin" ) == "1 2 3" );
	CHECK( doc.Rotate( spawn, 300.0f ) && doc.Find( spawn )->entity.Value( "angle" ) == "30" );
	CHECK( doc.Rotate( spawn, -45.0f ) && doc.Find( spawn )->entity.Value( "angle" ) == "345" );
	CHECK( doc.Rotate( trigger, 90.0f ) && doc.Find( trigger )->entity.Value( "angle" ) == "90" );
	CHECK( !doc.Rotate( world, 90.0f ) );
	CHECK( doc.SetValue( made, "angles", "10 20 30" ) );
	CHECK( doc.Rotate( made, 15.0f ) && doc.Find( made )->entity.Value( "angles" ) == "10 35 30" );
	CHECK( !doc.Find( made )->entity.Has( "angle" ) );

	/* a drag: one checkpoint, many moves, and nothing left when it moved
	 * nothing */
	const std::size_t undoBefore = static_cast<std::size_t>( doc.CanUndo() );
	doc.Checkpoint( "move" );
	const float a[3] = { 5.0f, 5.0f, 5.0f }, b[3] = { 6.0f, 6.0f, 6.0f };
	CHECK( doc.PlaceWithoutCheckpoint( spawn, a ) && doc.PlaceWithoutCheckpoint( spawn, b ) );
	doc.DropCheckpointIfUnchanged();
	CHECK( doc.UndoLabel() == "move" );
	CHECK( doc.Undo() && doc.Find( spawn )->entity.Value( "origin" ) == "1 2 3" );
	doc.Checkpoint( "move" );
	doc.DropCheckpointIfUnchanged();
	CHECK( doc.UndoLabel() != "move" || !undoBefore );

	/* paste: entity text from elsewhere, listed last as one change; never a
	 * worldspawn or a brush entity, whose model is its own map's */
	const std::size_t before = doc.Count();
	const auto clipboard = et::Parse(
		"{ \"classname\" \"worldspawn\" }\n"
		"{ \"classname\" \"func_door\" \"model\" \"*4\" }\n"
		"{ \"classname\" \"light\" \"origin\" \"1 2 3\" \"light\" \"300\" }\n" ).entities;
	const float aim[3] = { 64.0f, 64.0f, 16.0f };
	CHECK( doc.Paste( clipboard, aim ) == 1 && doc.Count() == before + 1 );
	CHECK( doc.SelectedItem()->entity.Classname() == "light" );
	CHECK( doc.SelectedItem()->entity.Value( "origin" ) == "64 64 16" );	/* a lone one goes to the aim */
	CHECK( doc.UndoLabel() == "paste light" );
	const auto pair = et::Parse( "{ \"classname\" \"a\" \"origin\" \"1 1 1\" }\n{ \"classname\" \"b\" }" ).entities;
	CHECK( doc.Paste( pair, aim ) == 2 && doc.Count() == before + 3 );
	CHECK( doc.Items()[before + 1].entity.Value( "origin" ) == "1 1 1" );	/* several keep theirs */
	CHECK( doc.Paste( et::Parse( "{ \"classname\" \"worldspawn\" }" ).entities, aim ) == 0 );
	CHECK( doc.Undo() && doc.Undo() && doc.Count() == before );

	/* replace: one change that can be undone */
	doc.Replace( et::Parse( "{ \"classname\" \"worldspawn\" }" ).entities, "revert" );
	CHECK( doc.Count() == 1 && doc.Selected() == 0 );
	CHECK( doc.Undo() && doc.Count() == 10 );
}

void TestDocumentHistoryLimit()
{
	ee::Document doc;
	doc.Load( Sample() );
	const std::uint32_t spawn = doc.Items()[1].id;
	for ( int i = 0; i < static_cast<int>( ee::Document::kHistoryLimit ) + 20; i++ ) {
		CHECK( doc.SetValue( spawn, "angle", std::to_string( i ) ) );
	}
	int undone = 0;
	while ( doc.Undo() ) {
		undone++;
	}
	CHECK( undone == static_cast<int>( ee::Document::kHistoryLimit ) );
	CHECK( doc.Find( spawn )->entity.Value( "angle" ) == "19" );
}

void TestDocumentQueries()
{
	ee::Document doc;
	doc.Load( Sample() );
	const auto &items = doc.Items();
	std::vector<std::uint32_t> targets, sources;
	/* the trigger targets the delay; the delay targets the speaker; the door
	 * kills the speaker */
	doc.Links( items[3].id, targets, sources );
	CHECK( targets.size() == 1 && targets[0] == items[4].id );
	CHECK( sources.size() == 1 && sources[0] == items[2].id );
	doc.Links( items[4].id, targets, sources );
	CHECK( targets.empty() && sources.size() == 2 );

	CHECK( ee::Document::Matches( items[6].entity, "rocket" ) );
	CHECK( ee::Document::Matches( items[6].entity, "WEAPON origin" ) );
	CHECK( !ee::Document::Matches( items[6].entity, "rocket health" ) );
	CHECK( ee::Document::Matches( items[2].entity, "model=*1" ) );
	CHECK( !ee::Document::Matches( items[2].entity, "model=*2" ) );
	CHECK( ee::Document::Matches( items[2].entity, "" ) );

	const std::vector<std::string> names = doc.Classnames();
	CHECK( !names.empty() && names[0] == "info_player_deathmatch" );
	CHECK( names.size() == 7 );

	CHECK( ee::Classify( items[0].entity ) == ee::Category::World );
	CHECK( ee::Classify( items[1].entity ) == ee::Category::Spawn );
	CHECK( ee::Classify( items[2].entity ) == ee::Category::Trigger );
	CHECK( ee::Classify( items[3].entity ) == ee::Category::Logic );
	CHECK( ee::Classify( items[5].entity ) == ee::Category::Brush );
	CHECK( ee::Classify( items[6].entity ) == ee::Category::Item );
	et::Entity light;
	light.Set( "classname", "light" );
	CHECK( ee::Classify( light ) == ee::Category::Light );

	CHECK( ee::IsTargetKey( "target" ) && ee::IsTargetKey( "Target2" ) && ee::IsTargetKey( "killtarget" ) );
	CHECK( ee::IsTargetKey( "deathtarget" ) && !ee::IsTargetKey( "targetname" ) && !ee::IsTargetKey( "target0" ) );
	CHECK( ee::IsBrushModel( items[2].entity ) && ee::BrushModelNumber( items[5].entity ) == 2 );
	CHECK( !ee::IsBrushModel( items[6].entity ) && ee::BrushModelNumber( items[6].entity ) == -1 );

	float origin[3];
	CHECK( ee::EntityOrigin( items[6].entity, origin ) && origin[0] == 256.0f && origin[2] == 8.0f );
	CHECK( !ee::EntityOrigin( items[2].entity, origin ) );
	float yaw = 0.0f;
	CHECK( ee::EntityYaw( items[1].entity, yaw ) && yaw == 90.0f );
	CHECK( !ee::EntityYaw( items[6].entity, yaw ) );

	CHECK( ee::SnapToGrid( 13.0f, 8.0f ) == 16.0f && ee::SnapToGrid( -13.0f, 8.0f ) == -16.0f );
	CHECK( ee::SnapToGrid( 13.0f, 0.0f ) == 13.0f );
}

et::Entity Named( const char *classname, const char *spawnflags = nullptr )
{
	et::Entity entity;
	entity.Set( "classname", classname );
	if ( spawnflags ) {
		entity.Set( "spawnflags", spawnflags );
	}
	return entity;
}

bool ExtentIs( const ee::Extent &extent, float x0, float y0, float z0, float x1, float y1, float z1 )
{
	return extent.mins[0] == x0 && extent.mins[1] == y0 && extent.mins[2] == z0
		&& extent.maxs[0] == x1 && extent.maxs[1] == y1 && extent.maxs[2] == z1;
}

/* Each game's boxes about an origin: the Quake III line's and Wolfenstein's
 * about it, Ritual's and 2015's standing on it. */
void TestPointExtents()
{
	using L = ee::Lineage;
	/* spawn points: the player who stands there */
	CHECK( ExtentIs( ee::PointExtent( L::Quake3, Named( "info_player_deathmatch" ) ), -16, -16, -24, 16, 16, 32 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Quake3, Named( "team_CTF_redspawn" ) ), -16, -16, -24, 16, 16, 32 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Wolfenstein, Named( "info_player_deathmatch" ) ), -18, -18, -24, 18, 18, 48 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "info_player_start" ) ), -16, -16, 0, 16, 16, 96 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "info_player_allied" ) ), -16, -16, 0, 16, 16, 96 ) );
	/* items: Quake III's radius about it; Wolfenstein's set on it unless it
	 * hangs; Ritual's least box, on it */
	CHECK( ExtentIs( ee::PointExtent( L::Quake3, Named( "weapon_rocketlauncher" ) ), -15, -15, -15, 15, 15, 15 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Wolfenstein, Named( "item_health" ) ), -10, -10, 0, 10, 10, 10 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Wolfenstein, Named( "item_health", "1" ) ), -10, -10, -10, 10, 10, 10 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Wolfenstein, Named( "item_health", "2" ) ), -10, -10, 0, 10, 10, 10 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "item_health_25" ) ), -10, -10, 0, 10, 10, 20 ) );
	/* lights and points stay about their origins in every game */
	for ( const L lineage : { L::Quake3, L::Wolfenstein, L::Ritual } ) {
		CHECK( ExtentIs( ee::PointExtent( lineage, Named( "light" ) ), -6, -6, -6, 6, 6, 6 ) );
		CHECK( ExtentIs( ee::PointExtent( lineage, Named( "script_origin" ) ), -8, -8, -8, 8, 8, 8 ) );
		CHECK( ExtentIs( ee::PointExtent( lineage, Named( "info_notnull" ) ), -8, -8, -8, 8, 8, 8 ) );
		CHECK( ExtentIs( ee::PointExtent( lineage, Named( "trigger_relay" ) ), -8, -8, -8, 8, 8, 8 ) );
	}
	/* Ritual's models and path nodes stand on theirs */
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "enemies_boojum" ) ), -8, -8, 0, 8, 8, 16 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "misc_model" ) ), -8, -8, 0, 8, 8, 16 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "script_model" ) ), -8, -8, 0, 8, 8, 16 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "info_pathnode" ) ), -24, -24, 0, 24, 24, 32 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "info_grav_pathnode" ) ), -16, -16, 0, 16, 16, 32 ) );
	CHECK( ExtentIs( ee::PointExtent( L::Quake3, Named( "misc_model" ) ), -8, -8, -8, 8, 8, 8 ) );
	/* func_spawn makes things, and is no player's spawn point */
	CHECK( ee::Classify( Named( "func_spawn" ) ) == ee::Category::Brush );
	CHECK( ee::Classify( Named( "func_spawnchain" ) ) == ee::Category::Brush );
	CHECK( ExtentIs( ee::PointExtent( L::Ritual, Named( "func_spawn" ) ), -8, -8, -8, 8, 8, 8 ) );
	CHECK( ee::Classify( Named( "team_CTF_bluespawn" ) ) == ee::Category::Spawn );
}

/* An "angle" of -1 or -2 points up or down: no yaw, and turning keeps it. */
void TestVerticalAngles()
{
	CHECK( ee::AngleIsVertical( -1.0f ) && ee::AngleIsVertical( -2.0f ) );
	CHECK( !ee::AngleIsVertical( -3.0f ) && !ee::AngleIsVertical( 359.0f ) && !ee::AngleIsVertical( -1.5f ) );
	const et::ParseResult parsed = et::Parse(
		"{\n\"classname\" \"worldspawn\"\n}\n"
		"{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n\"angle\" \"-1\"\n}\n"
		"{\n\"classname\" \"target_push\"\n\"origin\" \"0 0 0\"\n\"angle\" \"-2\"\n}\n"
		"{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 24\"\n\"angle\" \"-1\"\n\"angles\" \"0 -1 0\"\n}\n"
		"{\n\"classname\" \"info_player_start\"\n\"origin\" \"64 0 24\"\n\"angle\" \"90\"\n}\n" );
	CHECK( parsed.ok && parsed.entities.size() == 5 );
	float yaw = 7.0f;
	CHECK( !ee::EntityYaw( parsed.entities[1], yaw ) && yaw == 7.0f );
	CHECK( !ee::EntityYaw( parsed.entities[2], yaw ) );
	CHECK( ee::EntityYaw( parsed.entities[3], yaw ) && yaw == -1.0f );	/* "angles" first: a yaw */

	ee::Document doc;
	doc.Load( parsed.entities );
	std::vector<std::uint32_t> ids;
	for ( const ee::Item &item : doc.Items() ) {
		ids.push_back( item.id );
	}
	CHECK( !doc.Rotate( ids[1], 15.0f ) && !doc.CanUndo() );
	doc.SetSelection( { ids[1], ids[2] }, ids[2] );
	CHECK( doc.RotateSelection( 90.0f ) == 0 && !doc.CanUndo() );
	CHECK( doc.Items()[1].entity.Value( "angle" ) == "-1" && doc.Items()[2].entity.Value( "angle" ) == "-2" );
	/* among others, only the others turn */
	doc.SetSelection( { ids[1], ids[2], ids[3], ids[4] }, ids[4] );
	CHECK( doc.RotateSelection( 90.0f ) == 2 );
	CHECK( doc.Items()[1].entity.Value( "angle" ) == "-1" && doc.Items()[2].entity.Value( "angle" ) == "-2" );
	CHECK( doc.Items()[3].entity.Value( "angles" ) == "0 89 0" && doc.Items()[4].entity.Value( "angle" ) == "180" );
	CHECK( doc.Undo() && doc.Items()[4].entity.Value( "angle" ) == "90" );
}

/* The editor's field of view: as wide as asked on a 4:3 screen, and as a
 * game's is drawn on any other (r_fovCorrection, q3_fov_profile_tests.py). */
void TestFieldOfView()
{
	const auto same = []( float value, float expected ) { return std::fabs( value - expected ) < 0.0002f; };
	float x = 0.0f, y = 0.0f;
	ee::FitFieldOfView( 90.0, 4.0 / 3.0, x, y );
	CHECK( same( x, 90.0f ) && same( y, 73.739795f ) );
	ee::FitFieldOfView( 90.0, 16.0 / 9.0, x, y );
	CHECK( same( x, 106.260205f ) && same( y, 73.739795f ) );
	ee::FitFieldOfView( 90.0, 2560.0 / 1080.0, x, y );
	CHECK( same( x, 121.284493f ) && same( y, 73.739795f ) );
	ee::FitFieldOfView( 90.0, 1080.0 / 1920.0, x, y );
	CHECK( same( x, 45.747330f ) && same( y, 73.739795f ) );
	/* the widest asked of the widest screen stays a field a renderer draws */
	ee::FitFieldOfView( 140.0, 32.0 / 9.0, x, y );
	CHECK( x > 160.0f && x <= 178.0f && y < 130.0f );
	/* nonsense is the default's */
	ee::FitFieldOfView( std::numeric_limits<double>::quiet_NaN(), 0.0, x, y );
	CHECK( same( x, 90.0f ) && same( y, 73.739795f ) );
}

/* Which edits a map_restart can respawn: only lists that spawn alike. */
void TestSpawnsAlike()
{
	CHECK( ee::DecidesSpawn( "classname" ) && ee::DecidesSpawn( "SpawnFlags" ) && ee::DecidesSpawn( "gametype" ) );
	CHECK( ee::DecidesSpawn( "notsingle" ) && ee::DecidesSpawn( "notteam" ) && ee::DecidesSpawn( "not_gametype" ) );
	CHECK( !ee::DecidesSpawn( "origin" ) && !ee::DecidesSpawn( "targetname" ) && !ee::DecidesSpawn( "noise" ) );

	const et::ParseResult running = et::Parse(
		"{\n\"classname\" \"worldspawn\"\n}\n"
		"{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 0\"\n\"spawnflags\" \"1\"\n}\n"
		"{\n\"classname\" \"light\"\n\"origin\" \"8 8 8\"\n}\n" );
	CHECK( running.ok && running.entities.size() == 3 );
	std::vector<et::Entity> edited = running.entities;
	CHECK( ee::SpawnsAlike( running.entities, edited ) );

	/* values that do not decide spawning: moved, keyed, renamed */
	edited[1].Set( "origin", "64 0 0" );
	edited[2].Set( "light", "300" );
	edited[2].Set( "targetname", "t1" );
	CHECK( ee::SpawnsAlike( running.entities, edited ) );
	/* a classname compares in any case, as games compare them */
	edited[2].Set( "classname", "LIGHT" );
	CHECK( ee::SpawnsAlike( running.entities, edited ) );

	/* an entity more, one fewer, another order, another kind */
	std::vector<et::Entity> more = edited;
	more.push_back( edited[2] );
	CHECK( !ee::SpawnsAlike( running.entities, more ) );
	std::vector<et::Entity> fewer( edited.begin(), edited.end() - 1 );
	CHECK( !ee::SpawnsAlike( running.entities, fewer ) );
	std::vector<et::Entity> swapped = edited;
	std::swap( swapped[1], swapped[2] );
	CHECK( !ee::SpawnsAlike( running.entities, swapped ) );
	std::vector<et::Entity> kind = edited;
	kind[2].Set( "classname", "info_notnull" );
	CHECK( !ee::SpawnsAlike( running.entities, kind ) );

	/* spawnflags and the game-type filters, changed, added or removed */
	std::vector<et::Entity> flags = edited;
	flags[1].Set( "spawnflags", "3" );
	CHECK( !ee::SpawnsAlike( running.entities, flags ) );
	std::vector<et::Entity> filtered = edited;
	filtered[2].Set( "notsingle", "1" );
	CHECK( !ee::SpawnsAlike( running.entities, filtered ) );
	std::vector<et::Entity> unflagged = edited;
	unflagged[1].Remove( "spawnflags" );
	CHECK( !ee::SpawnsAlike( running.entities, unflagged ) );
}

void TestFindings()
{
	const ee::SpawnLimits q3 = { 64, 4096 };
	et::Entity world;
	world.Set( "classname", "worldspawn" );
	CHECK( ee::CheckEntity( world, q3, true ).empty() );
	CHECK( !ee::CheckEntity( world, q3, false ).empty() );		/* a second worldspawn */
	et::Entity first;
	first.Set( "classname", "info_null" );
	const auto notWorld = ee::CheckEntity( first, q3, true );
	CHECK( notWorld.size() == 1 && notWorld[0].error );

	et::Entity many;
	many.Set( "classname", "info_null" );
	for ( int i = 0; i < 64; i++ ) {
		many.pairs.push_back( { "k" + std::to_string( i ), "v" } );
	}
	bool tooMany = false;
	for ( const ee::Finding &finding : ee::CheckEntity( many, q3, false ) ) {
		tooMany = tooMany || ( finding.error && finding.text.find( "at most 64" ) != std::string::npos );
	}
	CHECK( tooMany );
	CHECK( ee::CheckEntity( many, {}, false ).empty() );		/* no known limits */

	et::Entity wide;
	wide.Set( "classname", "info_null" );
	wide.Set( "message", std::string( 1000, 'm' ) );
	wide.Set( "message2", std::string( 1000, 'm' ) );
	wide.Set( "message3", std::string( 1000, 'm' ) );
	bool tooWide = false;
	for ( const ee::Finding &finding : ee::CheckEntity( wide, { 64, 2048 }, false ) ) {
		tooWide = tooWide || ( finding.error && finding.text.find( "at most 2048" ) != std::string::npos );
	}
	CHECK( tooWide );

	et::Entity twice;
	twice.pairs = { { "classname", "info_null" }, { "origin", "0 0 0" }, { "ORIGIN", "1 1 1" } };
	const auto duplicate = ee::CheckEntity( twice, q3, false );
	CHECK( duplicate.size() == 1 && !duplicate[0].error );

	et::Entity nameless;
	nameless.Set( "origin", "0 0 0" );
	CHECK( ee::CheckEntity( nameless, q3, false ).size() == 1 );
}

/* ---- the selection ----------------------------------------------------- */

void TestSelection()
{
	ee::Document doc;
	const et::ParseResult parsed = et::Parse(
		"{\n\"classname\" \"worldspawn\"\n}\n"
		"{\n\"classname\" \"light\"\n\"origin\" \"0 0 0\"\n}\n"
		"{\n\"classname\" \"light\"\n\"origin\" \"8 0 0\"\n}\n"
		"{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n}\n"
		"{\n\"classname\" \"info_notnull\"\n\"origin\" \"16 0 0\"\n}\n" );
	CHECK( parsed.ok && parsed.entities.size() == 5 );
	doc.Load( parsed.entities );
	std::vector<std::uint32_t> ids;
	for ( const ee::Item &item : doc.Items() ) {
		ids.push_back( item.id );
	}

	/* one, more, fewer, a range either way, and a list with a stranger */
	doc.Select( ids[1] );
	CHECK( doc.Selected() == ids[1] && doc.SelectionCount() == 1 && doc.IsSelected( ids[1] ) );
	doc.AddToSelection( ids[4] );
	CHECK( doc.Selected() == ids[4] && doc.SelectionCount() == 2 );
	doc.ToggleSelection( ids[4] );
	CHECK( !doc.IsSelected( ids[4] ) && doc.Selected() == ids[1] && doc.SelectionCount() == 1 );
	doc.SelectRange( ids[1], ids[3] );
	CHECK( doc.SelectionCount() == 3 && doc.Selected() == ids[3] );
	const std::vector<std::uint32_t> inOrder = doc.Selection();
	CHECK( inOrder.size() == 3 && inOrder[0] == ids[1] && inOrder[1] == ids[2] && inOrder[2] == ids[3] );
	doc.SelectRange( ids[4], ids[2] );
	CHECK( doc.SelectionCount() == 4 && doc.Selected() == ids[2] );
	doc.SetSelection( { ids[1], ids[2], 9999u }, ids[2] );
	CHECK( doc.SelectionCount() == 2 && doc.Selected() == ids[2] );
	doc.Select( 0 );
	CHECK( doc.SelectionCount() == 0 && doc.Selected() == 0 && !doc.IsSelected( 0 ) );

	/* edits of them all, each one change */
	doc.SetSelection( { ids[1], ids[2] }, ids[2] );
	CHECK( doc.SetValueOnSelection( "spawnflags", "1" ) );
	CHECK( doc.Items()[1].entity.Value( "spawnflags" ) == "1" && doc.Items()[2].entity.Value( "spawnflags" ) == "1" );
	CHECK( doc.UndoLabel() == "set spawnflags on 2" );
	CHECK( !doc.SetValueOnSelection( "bad\"key", "x" ) );
	CHECK( doc.RemoveKeyFromSelection( "spawnflags" ) == 2 && !doc.Items()[1].entity.Has( "spawnflags" ) );
	CHECK( doc.RemoveKeyFromSelection( "spawnflags" ) == 0 );
	doc.Undo();
	CHECK( doc.Items()[1].entity.Has( "spawnflags" ) && doc.SelectionCount() == 2 && doc.Selected() == ids[2] );
	CHECK( doc.RotateSelection( 90.0f ) == 2 );
	float yaw = 0.0f;
	CHECK( ee::EntityYaw( doc.Items()[2].entity, yaw ) && yaw == 90.0f );

	/* copies: not the worldspawn, not a brush entity; the copies selected,
	 * and undo brings the selection back with the entities */
	doc.SetSelection( { ids[0], ids[1], ids[3], ids[4] }, ids[4] );
	const float offset[3] = { 8.0f, 0.0f, 0.0f };
	CHECK( doc.DuplicateSelection( offset ) == 2 );
	CHECK( doc.Count() == 7 && doc.SelectionCount() == 2 );
	CHECK( doc.Items()[2].entity.Value( "origin" ) == "8 0 0" && doc.IsSelected( doc.Items()[2].id ) );
	CHECK( doc.Selected() == doc.Items()[6].id && doc.Items()[6].entity.Value( "origin" ) == "24 0 0" );
	doc.Undo();
	CHECK( doc.Count() == 5 && doc.SelectionCount() == 4 && doc.Selected() == ids[4] );

	/* deletes: the worldspawn stays; the entity before the first deleted is
	 * selected after */
	CHECK( doc.DeleteSelection() == 3 );
	CHECK( doc.Count() == 2 && doc.SelectionCount() == 1 && doc.Selected() == ids[0] );
	CHECK( doc.UndoLabel() == "delete 3 entities" );
	doc.Undo();
	CHECK( doc.Count() == 5 );
	doc.Select( ids[0] );
	CHECK( doc.DeleteSelection() == 0 && doc.Count() == 5 );

	/* an entity deleted leaves the selection; a paste selects what it made */
	doc.SetSelection( { ids[1], ids[2] }, ids[2] );
	CHECK( doc.Delete( ids[2] ) && doc.SelectionCount() == 1 && doc.Selected() == ids[1] );
	const et::ParseResult clip = et::Parse( "{\n\"classname\" \"info_notnull\"\n}\n{\n\"classname\" \"light\"\n}\n" );
	CHECK( doc.Paste( clip.entities, nullptr ) == 2 );
	CHECK( doc.SelectionCount() == 2 && doc.Selected() == doc.Items().back().id );
	doc.Replace( parsed.entities, "replace" );
	CHECK( doc.SelectionCount() == 0 && doc.Selected() == 0 );
}

/* ---- the ImGui bridge ------------------------------------------------------ */

/* A frame of ImGui's with no window, its textures granted an id as the
 * engine's backend grants a shader, and its background list converted. */
class Headless {
public:
	Headless()
	{
		context_ = ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.DisplaySize = ImVec2( 800.0f, 600.0f );
		io.DeltaTime = 1.0f / 60.0f;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
		io.Fonts->AddFontDefaultVector();
		ImGuiStyle &style = ImGui::GetStyle();
		style.AntiAliasedLines = true;
		style.AntiAliasedLinesUseTex = true;
		style.AntiAliasedFill = false;
	}
	~Headless() { ImGui::DestroyContext( context_ ); }
	Headless( const Headless & ) = delete;
	Headless &operator=( const Headless & ) = delete;

	template <typename Draw>
	ui::DrawList Frame( Draw draw )
	{
		ImGui::NewFrame();
		ImDrawList *background = ImGui::GetBackgroundDrawList();
		draw( *background );
		ImGui::Render();
		ImDrawData *data = ImGui::GetDrawData();
		if ( data->Textures ) {
			for ( ImTextureData *texture : *data->Textures ) {
				if ( texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates ) {
					texture->SetTexID( kTexture );
					texture->SetStatus( ImTextureStatus_OK );
				}
			}
		}
		ui::DrawList out;
		for ( const ImDrawList *list : data->CmdLists ) {
			if ( list == background ) {
				ui::AppendDrawList( *list, out );
			}
		}
		return out;
	}

	static constexpr ImTextureID kTexture = 7;

private:
	ImGuiContext *context_ = nullptr;
};

float Shoelace( const ui::DrawCommand &command )
{
	float area = 0.0f;
	for ( int i = 0; i < 4; i++ ) {
		const int next = ( i + 1 ) & 3;
		area += command.points[i][0] * command.points[next][1] - command.points[next][0] * command.points[i][1];
	}
	return area;
}

void TestDrawListQuad()
{
	/* counter-clockwise in, clockwise out, the 0-2 diagonal and each corner's
	 * texture and colour kept with it */
	ui::DrawList list;
	const ImVec2 xy[4] = { ImVec2( 0, 0 ), ImVec2( 0, 10 ), ImVec2( 10, 10 ), ImVec2( 10, 0 ) };
	const ImVec2 uv[4] = { ImVec2( 0.0f, 0.0f ), ImVec2( 0.0f, 1.0f ), ImVec2( 1.0f, 1.0f ), ImVec2( 1.0f, 0.0f ) };
	const ImU32 col[4] = { IM_COL32( 255, 0, 0, 255 ), IM_COL32( 0, 255, 0, 255 ), IM_COL32( 0, 0, 255, 255 ),
		IM_COL32( 255, 255, 255, 255 ) };
	list.Quad( xy, uv, col, 3 );
	CHECK( list.Commands().size() == 1 );
	const ui::DrawCommand &q = list.Commands()[0];
	CHECK( q.kind == ui::DrawKind::Quad && q.texture == 3 && Shoelace( q ) > 0.0f );
	CHECK( q.points[0][0] == 0.0f && q.points[0][1] == 0.0f && q.points[2][0] == 10.0f && q.points[2][1] == 10.0f );
	CHECK( q.points[1][0] == 10.0f && q.points[1][1] == 0.0f && q.st[1][0] == 1.0f && q.st[1][1] == 0.0f );
	CHECK( q.colors[1].r == 1.0f && q.colors[1].g == 1.0f && q.colors[3].g == 1.0f && q.colors[3].r == 0.0f );
	/* clockwise already: unchanged; no area: nothing */
	const ImVec2 cw[4] = { ImVec2( 0, 0 ), ImVec2( 10, 0 ), ImVec2( 10, 10 ), ImVec2( 0, 10 ) };
	list.Quad( cw, uv, col, 3 );
	CHECK( list.Commands().size() == 2 && list.Commands()[1].points[1][0] == 10.0f && list.Commands()[1].points[1][1] == 0.0f );
	const ImVec2 flat[4] = { ImVec2( 0, 0 ), ImVec2( 5, 0 ), ImVec2( 10, 0 ), ImVec2( 10, 0 ) };
	list.Quad( flat, uv, col, 3 );
	CHECK( list.Commands().size() == 2 );
	/* a packed colour and back */
	const ui::Color red = ui::Color::Unpacked( IM_COL32( 255, 0, 0, 128 ) );
	CHECK( red.r == 1.0f && red.g == 0.0f && red.Packed() == IM_COL32( 255, 0, 0, 128 ) );
}

void TestImGuiBridge()
{
	Headless imgui;

	/* an upright rectangle of one colour: one picture */
	ui::DrawList out = imgui.Frame( []( ImDrawList &list ) {
		list.AddRectFilled( ImVec2( 10.0f, 20.0f ), ImVec2( 50.0f, 40.0f ), IM_COL32( 255, 0, 0, 255 ) );
	} );
	CHECK( out.Commands().size() == 1 );
	if ( out.Commands().size() == 1 ) {
		const ui::DrawCommand &pic = out.Commands()[0];
		CHECK( pic.kind == ui::DrawKind::Pic && pic.texture == Headless::kTexture );
		CHECK( pic.points[0][0] == 10.0f && pic.points[0][1] == 20.0f && pic.points[1][0] == 40.0f && pic.points[1][1] == 20.0f );
		CHECK( pic.colors[0].r == 1.0f && pic.colors[0].g == 0.0f && pic.colors[0].a == 1.0f );
	}

	/* cut to its clip rectangle, its texture cut with it */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.PushClipRect( ImVec2( 0.0f, 0.0f ), ImVec2( 30.0f, 600.0f ) );
		list.AddRectFilled( ImVec2( 10.0f, 100.0f ), ImVec2( 50.0f, 120.0f ), IM_COL32( 0, 255, 0, 255 ) );
		list.PopClipRect();
	} );
	CHECK( out.Commands().size() == 1 && out.Commands()[0].kind == ui::DrawKind::Pic
		&& out.Commands()[0].points[0][0] == 10.0f && out.Commands()[0].points[1][0] == 20.0f );

	/* wholly outside it: nothing */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.PushClipRect( ImVec2( 0.0f, 0.0f ), ImVec2( 10.0f, 10.0f ) );
		list.AddRectFilled( ImVec2( 100.0f, 100.0f ), ImVec2( 200.0f, 200.0f ), IM_COL32( 0, 255, 0, 255 ) );
		list.PopClipRect();
	} );
	CHECK( out.Commands().empty() );

	/* text: a picture for each glyph that shows */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.AddText( ImVec2( 100.0f, 100.0f ), IM_COL32( 255, 255, 255, 255 ), "Hi !" );
	} );
	CHECK( out.Commands().size() == 3 );
	for ( const ui::DrawCommand &glyph : out.Commands() ) {
		CHECK( glyph.kind == ui::DrawKind::Pic && glyph.points[0][0] >= 100.0f && glyph.points[1][0] > 0.0f
			&& glyph.st[1][0] > glyph.st[0][0] );
	}

	/* a slanted line: one quad, clockwise */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.AddLine( ImVec2( 200.0f, 200.0f ), ImVec2( 300.0f, 260.0f ), IM_COL32( 255, 255, 0, 255 ), 2.0f );
	} );
	CHECK( out.Commands().size() == 1 && out.Commands()[0].kind == ui::DrawKind::Quad && Shoelace( out.Commands()[0] ) > 0.0f );

	/* a triangle: one quad with a corner doubled, clockwise */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.AddTriangleFilled( ImVec2( 400.0f, 100.0f ), ImVec2( 450.0f, 100.0f ), ImVec2( 425.0f, 150.0f ),
			IM_COL32( 0, 255, 255, 255 ) );
	} );
	CHECK( out.Commands().size() == 1 && out.Commands()[0].kind == ui::DrawKind::Quad && Shoelace( out.Commands()[0] ) > 0.0f );

	/* a line across a clip rectangle: what is left lies within it */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.PushClipRect( ImVec2( 500.0f, 300.0f ), ImVec2( 560.0f, 360.0f ) );
		list.AddLine( ImVec2( 480.0f, 280.0f ), ImVec2( 600.0f, 400.0f ), IM_COL32( 255, 0, 255, 255 ), 3.0f );
		list.PopClipRect();
	} );
	CHECK( !out.Commands().empty() );
	for ( const ui::DrawCommand &command : out.Commands() ) {
		CHECK( command.kind == ui::DrawKind::Quad && Shoelace( command ) > 0.0f );
		for ( int i = 0; i < 4; i++ ) {
			CHECK( command.points[i][0] >= 499.99f && command.points[i][0] <= 560.01f
				&& command.points[i][1] >= 299.99f && command.points[i][1] <= 360.01f );
		}
	}

	/* a rectangle's outline: four lines, a quad each; the window drawn in
	 * the same frame is a list of its own, not the background's */
	out = imgui.Frame( []( ImDrawList &list ) {
		list.AddRect( ImVec2( 600.0f, 400.0f ), ImVec2( 700.0f, 450.0f ), IM_COL32( 200, 200, 200, 255 ) );
		ImGui::SetNextWindowPos( ImVec2( 10.0f, 300.0f ) );
		ImGui::SetNextWindowSize( ImVec2( 200.0f, 100.0f ) );
		ImGui::Begin( "panel" );
		ImGui::TextUnformatted( "worldspawn" );
		ImGui::End();
	} );
	CHECK( out.Commands().size() == 4 );
}

} // namespace

int main()
{
	TestTokenizer();
	TestParser();
	TestSerializer();
	TestTextHelpers();
	TestDocumentBasics();
	TestDocumentEntities();
	TestDocumentHistoryLimit();
	TestDocumentQueries();
	TestPointExtents();
	TestVerticalAngles();
	TestFieldOfView();
	TestSpawnsAlike();
	TestFindings();
	TestSelection();
	TestDrawListQuad();
	TestImGuiBridge();
	if ( failures ) {
		std::cerr << failures << " checks failed\n";
	}
	return failures == 0 ? 0 : 1;
}
