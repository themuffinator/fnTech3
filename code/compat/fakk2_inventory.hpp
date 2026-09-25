/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_FAKK2_INVENTORY_HPP
#define FNTECH3_COMPAT_FAKK2_INVENTORY_HPP

/* Heavy Metal: F.A.K.K.2's inventory: the script its engine's UI reads at
 * start-up (`ui_inventoryfile`, default global/inventory.scr; inv_restart
 * 0x0040eeb0 -> Inventory_Load 0x00402b00), the pop-up that `inventory`,
 * `warpinv`, `nextinv` and `previnv` open, and the weapon groups `saveweaps`
 * keeps. Alice's engine carries all of it and ships an empty script, so only
 * F.A.K.K.2 has an inventory to show.
 *
 * This is the pure part: the script's grammar and checks, the selection's
 * steps, and the commands an item or a group sends.
 * code/client/cl_fakk2_inventory.cpp owns the pop-up and draws it.
 *
 * Addresses are retail alice.exe's (image base 0x400000) unless they are
 * marked fakk2.exe 1.02: the official patch's plain executable, readable
 * where the retail one is encrypted. */

#include "fakk2_hud.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fntech3 {
namespace fakk2 {
namespace inventory {

using hud::Move;
using hud::Vec3;
using hud::View;

/* ---- the data (0x00402bd0, 0x00403060, 0x004032a0, 0x004035d0) ---------- */

/* equip's words (the table at 0x0050a580), compared exactly. */
enum Equip {
	EquipLeft = 1,
	EquipRight = 2,
	EquipDual = 4,
	EquipUse = 8,
	EquipNone = 0x10,
};

struct Item {
	std::string name;           /* +0x0c */
	std::string ammo;           /* +0x10 */
	int equip = EquipUse;       /* +0x14, `use` until equip says otherwise (0x004053cf) */
	int width = 0, height = 0;  /* +0x18, +0x1c */
	int barWidth = 0, barHeight = 0;       /* +0x20, +0x24 */
	int barOffsetY = 0, barOffsetX = 0;    /* +0x28, +0x2c */
	float window[4] = { 0.0f, 0.0f, 1.0f, 1.0f };   /* modelwindow, +0x30..+0x3c */
	bool selectedOnTop = false; /* selitem_shaderontop, +0x40 */
	bool checkAmmo = false;     /* +0x41 */
	std::string command;        /* +0x44 */
	View hud;                   /* +0x48, the hud* keywords */
	std::string hudModel;       /* its model, +0x8c */
	std::string hudAnim;        /* +0x90, read by nothing */
	View view;                  /* +0x94, the plain keywords */
	std::string model;          /* +0xd8 */
	std::string anim;           /* +0xdc */
	std::string bgShader;       /* +0xe0 */
	std::string barShader;      /* +0xe4 */
	std::string selectedShader; /* selitem_shader, +0xe8 */
};

struct Type {
	std::string name;
	bool bgTile = false;         /* +0x04 */
	std::string button;          /* button_shader, +0x08 */
	std::string bg;              /* +0x0c */
	std::string hover;           /* hover_shader, +0x10 */
	std::string selected;        /* sel_shader, +0x14 */
	std::vector<Item> items;     /* +0x18 */
};

enum class Align { Left = 1, Right = 2 };
enum class Cascade { Left = 0, Right = 1 };

struct Inventory {
	int typeWidth = 100;         /* inv_width, +0x00 */
	int typeHeight = 100;        /* inv_height, +0x04 */
	int horizontalOffset = 0;    /* +0x08 */
	int verticalOffset = 0;      /* +0x0c */
	Align align = Align::Right;  /* +0x10 */
	Cascade cascade = Cascade::Left;   /* +0x14 */
	std::string selectSound, rejectSound, changeSound;   /* +0x18..+0x20 */
	std::vector<Type> types;     /* +0x24 */
};

/* ---- reading the script (invlistener::Parse 0x00402380) ------------------ */

namespace detail {

inline char Lower( char c ) { return c >= 'A' && c <= 'Z' ? static_cast<char>( c - 'A' + 'a' ) : c; }

inline bool EqualsNoCase( const std::string &a, const char *b ) {
	std::size_t i = 0;
	for ( ; i < a.size() && b[i]; ++i ) {
		if ( Lower( a[i] ) != Lower( b[i] ) ) return false;
	}
	return i == a.size() && !b[i];
}

/* The Script tokenizer's words: whitespace apart, `//` to the end of the
 * line and a block comment skipped, a quoted word kept whole without its
 * quotes. */
class Tokenizer {
public:
	Tokenizer( const char *text, std::size_t length ) : cursor_( text ), end_( text + length ) {}

	/* The next word on any line; false at the end. */
	bool Next( std::string &word, int &line ) {
		SkipSpace( true );
		if ( cursor_ >= end_ ) return false;
		line = line_;
		Read( word );
		return true;
	}

	/* The next word if it is on the current line. */
	bool NextOnLine( std::string &word ) {
		SkipSpace( false );
		if ( cursor_ >= end_ || *cursor_ == '\n' ) return false;
		Read( word );
		return true;
	}

	int Line() const { return line_; }

private:
	void SkipSpace( bool crossLines ) {
		while ( cursor_ < end_ ) {
			const char c = *cursor_;
			if ( c == '\n' ) {
				if ( !crossLines ) return;
				++line_;
				++cursor_;
			} else if ( static_cast<unsigned char>( c ) <= ' ' ) {
				++cursor_;
			} else if ( c == '/' && cursor_ + 1 < end_ && cursor_[1] == '/' ) {
				while ( cursor_ < end_ && *cursor_ != '\n' ) ++cursor_;
			} else if ( c == '/' && cursor_ + 1 < end_ && cursor_[1] == '*' ) {
				cursor_ += 2;
				while ( cursor_ < end_ && !( *cursor_ == '*' && cursor_ + 1 < end_ && cursor_[1] == '/' ) ) {
					if ( *cursor_ == '\n' ) ++line_;
					++cursor_;
				}
				cursor_ = cursor_ + 2 <= end_ ? cursor_ + 2 : end_;
			} else {
				return;
			}
		}
	}

	void Read( std::string &word ) {
		word.clear();
		if ( *cursor_ == '"' ) {
			++cursor_;
			while ( cursor_ < end_ && *cursor_ != '"' && *cursor_ != '\n' ) word.push_back( *cursor_++ );
			if ( cursor_ < end_ && *cursor_ == '"' ) ++cursor_;
			return;
		}
		while ( cursor_ < end_ && static_cast<unsigned char>( *cursor_ ) > ' ' ) word.push_back( *cursor_++ );
	}

	const char *cursor_;
	const char *end_;
	int line_ = 1;
};

inline Vec3 ReadVector( const std::string &text ) {
	Vec3 v;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	const int read = std::sscanf( text.c_str(), "%f %f %f", &x, &y, &z );
	if ( read >= 1 ) v.x = x;
	if ( read >= 2 ) v.y = y;
	if ( read >= 3 ) v.z = z;
	return v;
}

inline int ReadInt( const std::string &text ) { return std::atoi( text.c_str() ); }
inline float ReadFloat( const std::string &text ) { return static_cast<float>( std::atof( text.c_str() ) ); }

/* The listener's state (0x30 bytes on the stack): the current type and item
 * and the six item defaults a type's width keywords set before its first
 * item. Retail keeps the two bar offsets crossed in the listener and crosses
 * them back as an item takes them (0x004053c3..0x004053cc), so each still
 * lands in its own field; they are kept straight here. */
struct Listener {
	int type = -1;   /* indices, as the containers grow under them */
	int item = -1;
	int width = 0, height = 0, barWidth = 0, barHeight = 0, barOffsetY = 0, barOffsetX = 0;
};

} // namespace detail

/* What a read of the script gave. On an error the inventory is emptied and
 * `error` holds retail's message; `warnings` are the Class::warning lines,
 * which retail prints only with developer set. */
struct LoadResult {
	bool ok = false;
	std::string error;
	std::vector<std::string> warnings;
};

namespace detail {

/* One keyword and the words after it on its line. Returns an empty string or
 * the message the handler throws. */
inline std::string Apply( Inventory &inv, Listener &state, const std::string &key, const std::vector<std::string> &args,
	std::vector<std::string> &warnings ) {
	auto is = [&key]( const char *name ) { return EqualsNoCase( key, name ); };
	auto one = [&args]() { return args.size() == 1; };          /* inv_CheckArgs1 0x00402350 */
	const std::string first = args.empty() ? std::string() : args[0];
	Type *type = state.type >= 0 ? &inv.types[state.type] : nullptr;
	Item *item = type && state.item >= 0 ? &type->items[state.item] : nullptr;

	/* Anywhere. */
	if ( is( "{" ) || is( "}" ) ) return std::string();   /* 0x00404290, 0x004042a0 */
	if ( is( "inv_width" ) ) { if ( !one() ) return "bad arg count"; inv.typeWidth = ReadInt( first ); return {}; }
	if ( is( "inv_height" ) ) { if ( !one() ) return "bad arg count"; inv.typeHeight = ReadInt( first ); return {}; }
	if ( is( "inv_horiz_offset" ) ) { if ( !one() ) return "bad arg count"; inv.horizontalOffset = ReadInt( first ); return {}; }
	if ( is( "inv_vert_offset" ) ) { if ( !one() ) return "bad arg count"; inv.verticalOffset = ReadInt( first ); return {}; }
	if ( is( "inv_align" ) ) {
		if ( !one() ) return "bad arg count";
		if ( EqualsNoCase( first, "left" ) ) inv.align = Align::Left;
		else if ( EqualsNoCase( first, "right" ) ) inv.align = Align::Right;
		else warnings.push_back( "invlistener::Align: Invalid align side '" + first + "'" );
		return {};
	}
	if ( is( "inv_cascade" ) ) {
		if ( !one() ) return "bad arg count";
		if ( EqualsNoCase( first, "left" ) ) inv.cascade = Cascade::Left;
		else if ( EqualsNoCase( first, "right" ) ) inv.cascade = Cascade::Right;
		else warnings.push_back( "invlistener::Cascade: Invalid cascade side '" + first + "'" );
		return {};
	}
	if ( is( "inv_selectsound" ) ) { if ( !one() ) return "bad arg count"; inv.selectSound = first; return {}; }
	if ( is( "inv_rejectsound" ) ) { if ( !one() ) return "bad arg count"; inv.rejectSound = first; return {}; }
	if ( is( "inv_changesound" ) ) { if ( !one() ) return "bad arg count"; inv.changeSound = first; return {}; }
	if ( is( "typedef" ) ) {
		/* 0x00403ff0: a new type, current, with no item and the defaults zero. */
		if ( !one() ) return "bad arg count";
		inv.types.emplace_back();
		inv.types.back().name = first;
		state = Listener();
		state.type = static_cast<int>( inv.types.size() ) - 1;
		return {};
	}

	/* A type's. */
	auto typeShader = [&]( std::string Type::*slot, bool tile ) -> std::string {
		if ( !one() ) return "bad arg count";
		if ( !type ) return "no current type";   /* inv_CheckType 0x00402320 */
		type->*slot = first;
		if ( tile ) type->bgTile = true;
		return {};
	};
	if ( is( "button_shader" ) ) return typeShader( &Type::button, false );
	if ( is( "hover_shader" ) ) return typeShader( &Type::hover, false );
	if ( is( "sel_shader" ) ) return typeShader( &Type::selected, false );
	if ( is( "bg" ) ) return typeShader( &Type::bg, false );
	/* Declared with no format, but its handler wants the shader (0x004047f0). */
	if ( is( "bg_tile" ) ) return typeShader( &Type::bg, true );

	/* The sizes: the current item's, or the defaults the next item takes. */
	auto size = [&]( int Item::*field, int Listener::*fallback ) -> std::string {
		if ( !one() ) return "bad arg count";
		const int value = ReadInt( first );
		if ( item ) item->*field = value;
		else state.*fallback = value;
		return {};
	};
	if ( is( "width" ) ) return size( &Item::width, &Listener::width );
	if ( is( "height" ) ) return size( &Item::height, &Listener::height );
	if ( is( "barwidth" ) ) return size( &Item::barWidth, &Listener::barWidth );
	if ( is( "barheight" ) ) return size( &Item::barHeight, &Listener::barHeight );
	if ( is( "baroffsety" ) ) return size( &Item::barOffsetY, &Listener::barOffsetY );
	if ( is( "baroffsetx" ) ) return size( &Item::barOffsetX, &Listener::barOffsetX );

	if ( is( "item" ) ) {
		/* 0x00405220 */
		if ( !one() ) return "bad arg count";
		if ( !type ) return "no current type";
		type->items.emplace_back();
		Item &added = type->items.back();
		added.name = first;
		added.width = state.width;
		added.height = state.height;
		added.barWidth = state.barWidth;
		added.barHeight = state.barHeight;
		added.barOffsetX = state.barOffsetX;
		added.barOffsetY = state.barOffsetY;
		added.equip = EquipUse;
		state.item = static_cast<int>( type->items.size() ) - 1;
		return {};
	}

	/* An item's. */
	auto itemString = [&]( std::string Item::*field ) -> std::string {
		if ( !one() ) return "bad arg count";
		if ( !item ) return "no current item";   /* inv_CheckItem 0x004022f0 */
		item->*field = first;
		return {};
	};
	auto viewFloat = [&]( View Item::*block ) -> std::string {
		if ( !one() ) return "bad arg count";
		if ( !item ) return "no current item";
		( item->*block ).scale = ReadFloat( first );
		return {};
	};
	auto viewVector = [&]( View Item::*block, Vec3 View::*field ) -> std::string {
		if ( !one() ) return "bad arg count";
		if ( !item ) return "no current item";
		( item->*block ).*field = ReadVector( first );
		return {};
	};
	auto viewMove = [&]( View Item::*block ) -> std::string {
		if ( !one() ) return "bad arg count";
		if ( !item ) return "no current item";
		/* Exact words (0x00406be0, 0x00406f20). */
		if ( first == "bob" ) ( item->*block ).move = Move::Bob;
		else if ( first == "spin" ) ( item->*block ).move = Move::Spin;
		else return "bad move arguments";
		return {};
	};

	if ( is( "ammo" ) ) return itemString( &Item::ammo );
	if ( is( "equip" ) ) {
		/* 0x00405b80: the bits of every word, exactly as written. */
		if ( args.empty() ) return "equip needs one or more args";
		if ( !item ) return "no current item";
		int bits = 0;
		for ( const std::string &word : args ) {
			if ( word == "left" ) bits |= EquipLeft;
			else if ( word == "right" ) bits |= EquipRight;
			else if ( word == "dual" ) bits |= EquipDual;
			else if ( word == "use" ) bits |= EquipUse;
			else if ( word == "none" ) bits |= EquipNone;
			else return "bad equip arguments";
		}
		item->equip = bits;
		return {};
	}
	if ( is( "checkammo" ) ) {
		/* 0x00405b10: a missing number is Event::Error's and reads as 0. */
		if ( !item ) return "no current item";
		if ( args.empty() ) warnings.push_back( "Client: 'checkammo' : Index 1 out of range." );
		item->checkAmmo = ReadInt( first ) != 0;
		return {};
	}
	if ( is( "command" ) ) {
		/* 0x00405950: the first word only. */
		if ( args.empty() ) return "command needs one or more args";
		if ( !item ) return "no current item";
		item->command = first;
		return {};
	}
	if ( is( "model" ) ) return itemString( &Item::model );
	if ( is( "anim" ) ) return itemString( &Item::anim );
	if ( is( "scale" ) ) return viewFloat( &Item::view );
	if ( is( "angles" ) ) return viewVector( &Item::view, &View::angles );
	if ( is( "angledeltas" ) ) return viewVector( &Item::view, &View::angleDeltas );
	if ( is( "rotateoffset" ) ) return viewVector( &Item::view, &View::rotateOffset );
	if ( is( "offset" ) ) return viewVector( &Item::view, &View::offset );
	if ( is( "move" ) ) return viewMove( &Item::view );
	if ( is( "hudmodel" ) ) return itemString( &Item::hudModel );
	if ( is( "hudanim" ) ) return itemString( &Item::hudAnim );
	if ( is( "hudscale" ) ) return viewFloat( &Item::hud );
	if ( is( "hudangles" ) ) return viewVector( &Item::hud, &View::angles );
	if ( is( "hudangledeltas" ) ) return viewVector( &Item::hud, &View::angleDeltas );
	if ( is( "hudrotateoffset" ) ) return viewVector( &Item::hud, &View::rotateOffset );
	if ( is( "hudoffset" ) ) return viewVector( &Item::hud, &View::offset );
	if ( is( "hudmove" ) ) return viewMove( &Item::hud );
	if ( is( "bgshader" ) ) return itemString( &Item::bgShader );
	if ( is( "barshader" ) ) return itemString( &Item::barShader );
	if ( is( "selitem_shader" ) ) return itemString( &Item::selectedShader );
	if ( is( "selitem_shaderontop" ) ) {
		if ( !item ) return "no current item";
		item->selectedOnTop = true;
		return {};
	}
	if ( is( "modelwindow" ) ) {
		/* 0x00406030 */
		if ( args.size() != 4 ) return "ModelWindow: bad arg count";
		if ( !item ) return "no current item";
		for ( int i = 0; i < 4; ++i ) item->window[i] = ReadFloat( args[i] );
		return {};
	}
	return "invalid token";
}

/* The checks after the last line (0x00402658..0x004029ff): an empty string or
 * the message, and the fill-in of each hudmodel from its model. */
inline std::string Validate( Inventory &inv ) {
	if ( inv.typeHeight == 0 || inv.typeWidth == 0 ) return "type width/height not specified";
	for ( Type &type : inv.types ) {
		const std::string where = "type " + type.name + ": ";
		if ( type.hover.empty() ) return where + "no hover shader";
		if ( type.selected.empty() ) return where + "no selected shader";
		if ( type.button.empty() ) return where + "no button shader";
		if ( type.name.empty() ) return where + "no name";
		if ( type.items.empty() ) return where + "no items";
		for ( Item &item : type.items ) {
			const std::string itemWhere = where + "item " + item.name + ": ";
			if ( item.equip == 0 ) return itemWhere + "no equip types";
			if ( item.height == 0 || item.width == 0 ) return itemWhere + "bad item width/height";
			if ( item.model.empty() ) return itemWhere + "no model";
			if ( item.hudModel.empty() ) item.hudModel = item.model;   /* 0x00402827..0x00402845 */
			if ( item.name.empty() ) return itemWhere + "no name";
		}
	}
	return std::string();
}

} // namespace detail

/* Inventory_Load's parse of a script already read. Any error empties the
 * inventory, as retail's does (0x00402ab2, 0x00402a6a); a script with no
 * type loads nothing and says nothing (0x00402670). */
inline LoadResult Load( const char *text, std::size_t length, Inventory &inv ) {
	LoadResult result;
	inv = Inventory();
	detail::Tokenizer tokens( text, length );
	detail::Listener state;
	std::string key;
	int line = 0;
	while ( tokens.Next( key, line ) ) {
		std::vector<std::string> args;
		std::string word;
		while ( tokens.NextOnLine( word ) ) args.push_back( word );
		const std::string error = detail::Apply( inv, state, key, args, result.warnings );
		if ( !error.empty() ) {
			inv = Inventory();
			result.error = "inv: error on line " + std::to_string( line ) + ": " + error;
			return result;
		}
	}
	if ( inv.types.empty() ) {
		inv = Inventory();
		return result;
	}
	const std::string error = detail::Validate( inv );
	if ( !error.empty() ) {
		inv = Inventory();
		result.error = "inv: error after load: " + error;
		return result;
	}
	result.ok = true;
	return result;
}

/* The first item of any type named so, without regard to case (0x00407130). */
inline const Item *FindItem( const Inventory &inv, const char *name ) {
	if ( !name ) return nullptr;
	for ( const Type &type : inv.types ) {
		for ( const Item &item : type.items ) {
			if ( detail::EqualsNoCase( item.name, name ) ) return &item;
		}
	}
	return nullptr;
}

/* The first type named so (WarpTo 0x004090e0), 1-based; 0 for none. */
inline int FindType( const Inventory &inv, const char *name ) {
	if ( !name ) return 0;
	for ( std::size_t i = 0; i < inv.types.size(); ++i ) {
		if ( detail::EqualsNoCase( inv.types[i].name, name ) ) return static_cast<int>( i ) + 1;
	}
	return 0;
}

/* ---- the selection -------------------------------------------------------- */

/* The pop-up's selection: a type and an item in it, both 1-based, 0 for
 * none. With a type and no item the type's list is shown with nothing picked
 * in it. */
struct Selection {
	int type = 0;
	int item = 0;
};

/* FindUsable (0x00408ab0): from `from`, step while inside the type and the
 * item is not held. Returns where it stopped, which may be outside. */
template <typename Owned>
int FindUsable( const Type &type, int from, int direction, Owned owned ) {
	const int count = static_cast<int>( type.items.size() );
	int n = from;
	while ( n >= 1 && n <= count && !owned( type.items[n - 1] ) ) n += direction;
	return n;
}

/* ChangeItem (0x00408b20): the next or previous held item, crossing into the
 * next or previous type -- only one a step, wrapping at the ends -- and
 * landing on its first item when it holds nothing. Without a type the first
 * type's first item is chosen. Returns whether the type changed. */
template <typename Owned>
bool ChangeItem( const Inventory &inv, Selection &sel, int direction, Owned owned ) {
	const int types = static_cast<int>( inv.types.size() );
	if ( types == 0 ) return false;
	bool changed = false;
	if ( sel.type < 1 || sel.type > types ) {
		sel.type = 1;
		sel.item = 0;
		changed = true;
	}
	const Type *type = &inv.types[sel.type - 1];
	if ( sel.item == 0 ) {
		sel.item = 1;
		return changed;
	}
	int n = FindUsable( *type, sel.item + direction, direction, owned );
	if ( n >= 1 && n <= static_cast<int>( type->items.size() ) ) {
		sel.item = n;
		return changed;
	}
	int t = sel.type + direction;
	if ( t > types ) t = 1;
	if ( t < 1 ) t = types;
	sel.type = t;
	type = &inv.types[t - 1];
	const int count = static_cast<int>( type->items.size() );
	n = direction > 0 ? FindUsable( *type, 1, 1, owned ) : FindUsable( *type, count, -1, owned );
	if ( n < 1 || n > count ) n = 1;
	sel.item = n;
	return true;
}

/* The pressed warpinv key's type again, or the arrows and the wheel with an
 * item picked (0x00409a07): the next or previous item of the type, wrapping,
 * held or not. */
inline int CycleItem( const Type &type, int item, int direction ) {
	const int count = static_cast<int>( type.items.size() );
	if ( count == 0 ) return 0;
	int n = item + direction;
	if ( n > count ) n = 1;
	if ( n < 1 ) n = count;
	return n;
}

/* The arrows and the wheel with no item picked: the next or previous type,
 * wrapping; with none current, the first. */
inline int StepType( const Inventory &inv, int type, int direction ) {
	const int types = static_cast<int>( inv.types.size() );
	if ( types == 0 ) return 0;
	if ( type < 1 || type > types ) return 1;
	int t = type + direction;
	if ( t > types ) t = 1;
	if ( t < 1 ) t = types;
	return t;
}

/* ---- the commands --------------------------------------------------------- */

/* What using an item sends, or an empty string to refuse it (fakk2.exe 1.02's
 * item list: the left hand's 0x00409fc9..0x0040a026, the right's
 * 0x0040a209..0x0040a266). Its own command; else a two-handed weapon in both
 * hands; else the hand used, if the item fits it; else `use "<name>"` for an
 * item equipped by `use`; else nothing, so a one-handed weapon used from the
 * other hand is refused. The hand words are the `hand` forms fakk/gamex86.dll
 * accepts beside dual, left and right (0x101784c4..0x101784f4). alice.exe
 * sends only the command or `use "<name>"`, from either button. */
inline std::string UseCommand( const Item &item, bool leftHand ) {
	if ( !item.command.empty() ) return item.command;
	const std::string quoted = "\"" + item.name + "\"";
	if ( item.equip & EquipDual ) return "use " + quoted + " dualhand";
	if ( item.equip & ( leftHand ? EquipLeft : EquipRight ) ) {
		return "use " + quoted + ( leftHand ? " lefthand" : " righthand" );
	}
	if ( item.equip & EquipUse ) return "use " + quoted;
	return std::string();
}

/* The command `saveweaps` keeps (0x0040fed0): the two-handed weapon in both
 * hands; else both hands' weapons, or the one hand's; else `holster`. A name
 * is the item configstring of the slot, empty for a slot the game left at -1.
 * Retail ends every form but the first with a newline; the alias it becomes
 * adds its own when it runs. */
inline std::string WeaponGroupCommand( bool hasDual, const std::string &dual, const std::string &left,
	const std::string &right ) {
	if ( hasDual ) return "use \"" + dual + "\" dual";
	if ( !left.empty() && !right.empty() ) return "dualwield \"" + left + "\" \"" + right + "\"";
	if ( !left.empty() ) return "use \"" + left + "\" left";
	if ( !right.empty() ) return "use \"" + right + "\" right";
	return "holster";
}

/* The pop-up's rectangle in the menus' 640 by 480 units: the type column at
 * the right edge less the width, or at the left, moved by the offsets, one
 * type high per type (fakk2.exe 1.02 UI_DoInventory 0x004081cf..0x0040828b,
 * which measures the video's width in pixels; alice.exe's gives the widget no
 * size, 0x00408649..0x0040868a). */
struct Rect {
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

inline Rect ColumnRect( const Inventory &inv, float screenWidth ) {
	Rect r;
	r.x = inv.align == Align::Left ? static_cast<float>( inv.horizontalOffset )
		: screenWidth - static_cast<float>( inv.typeWidth ) + static_cast<float>( inv.horizontalOffset );
	r.y = static_cast<float>( inv.verticalOffset );
	r.w = static_cast<float>( inv.typeWidth );
	r.h = static_cast<float>( inv.typeHeight ) * static_cast<float>( inv.types.size() );
	return r;
}

/* The type's list (FakkItemList::Create 0x00409cc0): as wide as its widest
 * item and as tall as its items together, from the column's top whichever
 * type it shows, to the column's right or left. */
inline Rect ListRect( const Inventory &inv, const Type &type, const Rect &column ) {
	Rect r;
	int width = 0, height = 0;
	for ( const Item &item : type.items ) {
		if ( item.width > width ) width = item.width;
		height += item.height;
	}
	r.w = static_cast<float>( width );
	r.h = static_cast<float>( height );
	r.y = column.y;
	r.x = inv.cascade == Cascade::Right ? column.x + column.w : column.x - r.w;
	return r;
}

/* An item cell's ammunition gauge, as FakkItemList::Draw draws it (fakk2.exe
 * 1.02 0x00409a22..0x00409b36), before the cell's own picture. With an
 * ammunition kind and a bar picture, the amount held over the kind's maximum,
 * held to 0..1 (none with no maximum), fills from the cell's foot: a
 * `barwidth` by `barheight`-times-the-fraction box when either size is given,
 * else the cell's width by its height times the fraction. The renderer's
 * DrawTilePicOffset (0x00481dd0) draws it a texel a unit, sampling the picture
 * where the box sits in the list and only then moving the box by the bar
 * offsets: the offsets move the box, not the picture. The shipped gauges are
 * 27 by 27 boxes raised by 4 over a 64 by 64 picture that repeats. `rowY` is
 * the row's top below the list's, and the image size the picture's. */
struct Gauge {
	bool draw = false;
	Rect rect;
	float s0 = 0.0f, t0 = 0.0f, s1 = 0.0f, t1 = 0.0f;
};

inline Gauge AmmoGauge( const Item &item, const Rect &cell, float rowY, int amount, int max,
	float imageW, float imageH ) {
	Gauge gauge;
	if ( item.barShader.empty() || item.ammo.empty() || max == 0 || !( imageW > 0.0f ) || !( imageH > 0.0f ) ) {
		return gauge;
	}
	float fraction = static_cast<float>( amount ) / static_cast<float>( max );
	if ( !( fraction > 0.0f ) ) fraction = 0.0f;
	if ( fraction > 1.0f ) fraction = 1.0f;
	const bool sized = item.barWidth != 0 || item.barHeight != 0;
	const float width = sized ? static_cast<float>( item.barWidth ) : cell.w;
	const float height = ( sized ? static_cast<float>( item.barHeight ) : cell.h ) * fraction;
	if ( !( width > 0.0f ) || !( height > 0.0f ) ) {
		return gauge;
	}
	const float place = rowY + cell.h - height;
	gauge.draw = true;
	gauge.rect.x = cell.x + static_cast<float>( item.barOffsetX );
	gauge.rect.y = cell.y + cell.h - height + static_cast<float>( item.barOffsetY );
	gauge.rect.w = width;
	gauge.rect.h = height;
	gauge.s0 = 0.0f;
	gauge.s1 = width / imageW;
	gauge.t0 = place / imageH;
	gauge.t1 = ( place + height ) / imageH;
	return gauge;
}

} // namespace inventory
} // namespace fakk2
} // namespace fntech3

#endif
