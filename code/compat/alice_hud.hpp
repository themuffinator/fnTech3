/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_ALICE_HUD_HPP
#define FNTECH3_COMPAT_ALICE_HUD_HPP

/* American McGee's Alice's HUD, AliceHud (constructor 0x00411080, children
 * built at 0x004110d0): the pure part -- its widget table, the gauges' eased
 * riser, and the two foldouts' state machines. code/client/cl_alice_hud.cpp
 * reads the player state and the shared block, feeds them here and draws what
 * comes back; nothing here touches an engine.
 *
 * AliceHud's own Draw (0x00411bf0) is a bare `ret`: everything the HUD shows
 * is one of the eight children below, each a UIWidget3D subclass that renders
 * one TIKI model into a 2D view of its own.
 *
 * Every constant is read from retail alice.exe (image base 0x400000) or
 * base/fgamex86.dll (0x10000000) at the address beside it. */

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>

namespace fntech3 {
namespace alice {
namespace hud {

/* ---- the widgets (0x004110d0) -------------------------------------------- */

enum class WidgetKind {
	Still,          /* UIWidget3D, Draw 0x004d2b10: frame 0 */
	Riser,          /* vtable 0x004ff5d0, Draw 0x00411d50: a stat, eased */
	WeaponFoldout,  /* vtable 0x004ff62c, Draw 0x004121c0 */
	ItemFoldout,    /* vtable 0x004ff67c, Draw 0x00412a00 */
};

/* One child: its first model, its offset from the screen's centre in 640x480
 * units (0x004d2670), its scale (0x004d2990), and for a riser the stat it
 * follows (0x004d2970). Every child is given a 640 by 480 frame (0x004d2690)
 * and the animation "idle" (0x004d27c0). */
struct Widget {
	const char *model;
	float offsetX, offsetY;
	float scale;
	WidgetKind kind;
	int stat;
};

/* In creation order, which is their order among AliceHud's children. */
inline constexpr Widget Widgets[8] = {
	{ "models/hud_health_back.tik", -312.0f, -25.0f, 2.45f, WidgetKind::Still, -1 },        /* 0x0041124d */
	{ "models/hud_health_riser.tik", -312.0f, -25.0f, 2.45f, WidgetKind::Riser, 0 },        /* 0x0041139b */
	{ "models/hud_foldout_knife.tik", -275.0f, 182.0f, 6.0f, WidgetKind::WeaponFoldout, -1 }, /* 0x00411508 */
	{ "models/hud_health_bar.tik", -312.0f, 42.0f, 1.2f, WidgetKind::Still, -1 },           /* 0x00411656 */
	{ "models/hud_itemfoldout.tik", 275.0f, 182.0f, 6.0f, WidgetKind::ItemFoldout, -1 },    /* 0x004117a4 */
	{ "models/hud_mana_back.tik", 312.0f, -25.0f, 2.45f, WidgetKind::Still, -1 },           /* 0x004118f2 */
	{ "models/hud_mana_riser.tik", 312.0f, -25.0f, 2.45f, WidgetKind::Riser, 8 },           /* 0x00411a40 */
	{ "models/hud_mana_bar.tik", 312.0f, 42.0f, 1.2f, WidgetKind::Still, -1 },              /* 0x00411bad */
};

/* Every child is drawn facing the camera: angles (0, 180, 0), reset at the
 * top of each Draw (0x004d2b3a, 0x00412200, 0x00412a0f). */
inline constexpr float BaseYaw = 180.0f;

/* ---- the riser (Draw 0x00411d50) ----------------------------------------- */

/* The constructor (0x00411c00) starts the target at 100 and the value shown,
 * the value an ease started from and the ease timer at zero. */
struct Riser {
	int target = 100;   /* +0x238 */
	int timer = 0;      /* +0x23c */
	int from = 0;       /* +0x240 */
	int shown = 0;      /* +0x244 */
};

inline constexpr int RiserEaseMs = 500;         /* 0x1f4 at 0x00411da7 */
inline constexpr float RiserEaseRate = 0.002f;  /* 0x004ff628, one over 500 ms */
inline constexpr double RiserFrameScale = 0.98; /* 0x004ff620 */

/* One frame: a changed stat starts a 500 ms ease from the value shown; the
 * frame is (100 - value) * 0.98, the difference held at zero or more, and the
 * result clamped to 0..100. The timer is written only while time is left, as
 * retail writes it (0x00411dc5). */
inline int RiserFrame( Riser &riser, int value, int frameMs ) {
	if ( value != riser.target ) {
		riser.from = riser.shown;
		riser.target = value;
		riser.timer = RiserEaseMs;
	}
	const int left = riser.timer - frameMs;
	if ( left > 0 ) {
		riser.timer = left;
		const double f = static_cast<double>( left ) * static_cast<double>( RiserEaseRate );
		riser.shown = static_cast<int>( ( 1.0 - f ) * riser.target + riser.from * f );
	} else {
		riser.shown = riser.target;
		riser.from = riser.target;
	}
	const int difference = 100 - riser.shown;
	float frame = static_cast<float>( ( difference < 0 ? 0 : difference ) * RiserFrameScale );
	if ( frame < 0.0f ) frame = 0.0f;
	if ( frame > 100.0f ) frame = 100.0f;
	return static_cast<int>( frame );
}

/* ---- the fold animation ----------------------------------------------------
 *
 * Both foldouts play the "idle" animation of models/ui/hud/foldout/folding.tan
 * or models/ui/hud/itemfoldout/folding.tan, 101 frames each. Frame 0 is the
 * card unfolded, 50 folded shut and 100 unfolded again: unfolding plays 50 to
 * 0, folding shut plays 100 to 50, each over 200 ms at a quarter frame per
 * millisecond (0.005 * 50, 0x004ff678 and 0x004ff4c8). */

inline constexpr int FoldMs = 200;              /* 0xc8 at 0x00412491, 0x00412a83 */
inline constexpr float FoldRate = 0.005f;       /* 0x004ff678 */
inline constexpr float FoldHalf = 50.0f;        /* 0x004ff4c8 */
inline constexpr float FoldMaxFrame = 100.0f;   /* 0x004ff61c */

/* The half of the animation a fold timer is on: the timer times a quarter,
 * held at zero or more (0x0041259a-0x004125c1). */
inline float FoldFrames( int timer ) {
	double v = static_cast<double>( timer ) * static_cast<double>( FoldRate ) * static_cast<double>( FoldHalf );
	if ( v < 0.0 ) v = 0.0;
	return static_cast<float>( v );
}

/* ---- the weapon foldout (Draw 0x004121c0) --------------------------------- */

struct FoldoutWeapon {
	const char *name;
	const char *model;
};

/* The weapons it knows, by item name (table 0x0050c668, count 10 at
 * 0x0050c6b8). */
inline constexpr int FoldoutWeaponCount = 10;
inline constexpr FoldoutWeapon FoldoutWeapons[FoldoutWeaponCount] = {
	{ "knife", "models/hud_foldout_knife.tik" },
	{ "cards", "models/hud_foldout_cards.tik" },
	{ "mallet", "models/hud_foldout_mallet.tik" },
	{ "demondice", "models/hud_foldout_dice1.tik" },
	{ "jackbomb", "models/hud_foldout_jackbomb.tik" },
	{ "icewand", "models/hud_foldout_icewand.tik" },
	{ "jacks", "models/hud_foldout_jacks.tik" },
	{ "eyestaff", "models/hud_foldout_eyestaff.tik" },
	{ "blunderbuss", "models/hud_foldout_blunderbuss.tik" },
	{ "watch", "models/hud_foldout_watchweapon.tik" },
};

/* The demon dice's entry: its card shows how many dice are held, through a
 * model named for the count (`cmp edi, 3` at 0x004122df and 0x00412692; the
 * prefix at 0x0050ca94, the count appended with "%d" by 0x00450180, then
 * ".tik" from 0x0050c098). */
inline constexpr int DemonDiceWeapon = 3;
inline constexpr const char *DiceModelPrefix = "models/hud_foldout_dice";

inline std::string DiceModel( int count ) {
	return std::string( DiceModelPrefix ) + std::to_string( count ) + ".tik";
}

/* The yaw's wobble: sin( realtime * 0.003003 ) degrees, a period of about
 * 2.09 seconds (0x00412838-0x0041285d). */
inline constexpr float WobbleRate = 0.003003003f;   /* 0x004ff468 */

inline float WeaponFoldoutYaw( int realtime ) {
	return static_cast<float>( BaseYaw + std::sin( static_cast<double>( realtime ) * static_cast<double>( WobbleRate ) ) );
}

/* Retail's case-blind compare (0x0044fc10): ASCII letters only. */
inline int CompareNoCase( const char *a, const char *b ) {
	for ( ;; ) {
		int x = static_cast<signed char>( *a++ );
		int y = static_cast<signed char>( *b++ );
		if ( x != y ) {
			if ( x >= 'a' && x <= 'z' ) x -= 'a' - 'A';
			if ( y >= 'a' && y <= 'z' ) y -= 'a' - 'A';
			if ( x < y ) return -1;
			if ( x > y ) return 1;
		}
		if ( !x ) return 0;
	}
}

/* The table entry for an item name, or -1 (0x00412223-0x00412262). */
inline int FoldoutWeaponIndex( const char *name ) {
	if ( !name ) return -1;
	for ( int i = 0; i < FoldoutWeaponCount; ++i ) {
		if ( !CompareNoCase( name, FoldoutWeapons[i].name ) ) return i;
	}
	return -1;
}

/* The weapon's model for a table entry, or null where retail would read past
 * its table. */
inline const char *FoldoutWeaponModel( int index ) {
	return index >= 0 && index < FoldoutWeaponCount ? FoldoutWeapons[index].model : nullptr;
}

/* The foldout's name for the item in the hand: the player state's active item
 * 3 (0x004121dc) is an item index, -1 for none. The engine finds it among the
 * 32 of the inventory array beside it (0x00407500) and takes the item's
 * configstring, CS 0x400 plus the index; an index the inventory does not hold
 * has no name. */
inline bool InventoryHolds( const int inventory[32], int item ) {
	for ( int i = 0; i < 32; ++i ) {
		if ( inventory[i] == item ) return true;
	}
	return false;
}

/* What the foldout is asked to show: a table entry, -1 for none, and whether
 * the item had a name. With no name the item index itself stands in for the
 * entry, as retail's register is left holding it (0x00412274). */
struct WeaponRequest {
	int index = -1;
	bool named = false;
};

inline WeaponRequest RequestWeapon( int activeItem, const char *name ) {
	WeaponRequest request;
	if ( activeItem == -1 ) {
		return request;
	}
	if ( !name ) {
		request.index = activeItem;
		return request;
	}
	request.named = true;
	request.index = FoldoutWeaponIndex( name );
	return request;
}

enum class Fold { Idle = 0, Opening = 1, Closing = 2 };

/* The foldout's members, as the constructor (0x00411ee0) and AliceHud's init
 * leave them: the knife shown, unfolded, at frame 0. */
struct WeaponFoldout {
	int timer = 0;               /* +0x238 */
	int current = 0;             /* +0x23c: the entry shown, -1 for none */
	bool swapped = true;         /* +0x244: the new entry's model is up */
	bool open = true;            /* +0x248 */
	Fold fold = Fold::Idle;      /* +0x24c */
	int lastDice = 0;            /* +0x250 */
	float frame = 0.0f;          /* +0x21c */
	std::string model = FoldoutWeapons[0].model;  /* +0x1d4 */
};

/* One Draw's worth of state (0x00412278-0x00412822). `dice` is the shared
 * block's dice count, or null without a block. Changing the model keeps the
 * frame: retail saves it around its animation reset every time.
 *
 * Idle, a new entry swaps the model at once; losing the weapon folds the card
 * shut (100 to 50), and a named weapon arriving at a shut card unfolds it
 * (50 to 0) with the new model put up as the fold passes below frame 50. A
 * named weapon arriving while the card is open but showing nothing folds it
 * shut instead, as retail does. The item's name is also kept at +0x240, which
 * nothing reads; it is not kept here. */
inline void StepWeaponFoldout( WeaponFoldout &f, const WeaponRequest &request, const int *dice, int frameMs ) {
	const int index = request.index;
	if ( f.fold == Fold::Idle ) {
		if ( f.current != -1 && index != -1 && f.current != index ) {
			f.current = index;
			if ( const char *model = FoldoutWeaponModel( index ) ) {
				f.model = model;
			}
		}
		if ( dice && index == DemonDiceWeapon && *dice ) {
			if ( *dice != f.lastDice ) {
				f.model = DiceModel( *dice );
			}
			f.lastDice = *dice;
		}
		bool close = false;
		if ( f.current != -1 ) {
			close = index == -1 && f.open;
		} else if ( index != -1 && request.named ) {
			if ( f.open ) {
				close = true;
			} else {
				f.timer = FoldMs;
				f.fold = Fold::Opening;
				f.swapped = false;
			}
		}
		if ( close ) {
			f.timer = FoldMs;
			f.fold = Fold::Closing;
		}
		f.current = index;
	}

	if ( f.fold == Fold::Closing ) {
		if ( f.timer > 0 ) {
			f.timer -= frameMs;
			f.frame = static_cast<float>( static_cast<double>( FoldFrames( f.timer ) ) + FoldHalf );
		} else {
			f.fold = Fold::Idle;
			f.open = false;
		}
	} else if ( f.fold == Fold::Opening ) {
		if ( f.timer > 0 ) {
			f.timer -= frameMs;
			f.frame = FoldFrames( f.timer );
		} else {
			f.fold = Fold::Idle;
			f.open = true;
		}
	}

	if ( !f.swapped && f.frame < FoldHalf ) {
		f.swapped = true;
		if ( f.current != -1 ) {
			if ( dice && index == DemonDiceWeapon && *dice ) {
				f.model = DiceModel( *dice );
				f.lastDice = *dice;
			} else if ( const char *model = FoldoutWeaponModel( index ) ) {
				f.model = model;
			}
		}
	}
}

/* ---- the shared block's HUD members --------------------------------------
 *
 * The game module writes these into the 60-byte block import 107 hands it
 * (fgamex86.dll 0x101000f2-0x101001a1), once a player update, from the
 * player's powerup flags at +0x214 and the level's watch time; the engine's
 * item foldout and weapon foldout read them back through the same getter
 * (0x0042c490). */
inline constexpr std::size_t PowerupTimeOffset = 0x20;
inline constexpr std::size_t TeaOffset = 0x28;      /* flags & 1  (0x10100108) */
inline constexpr std::size_t RageBoxOffset = 0x2c;  /* flags & 2  (0x10100114) */
inline constexpr std::size_t GlassOffset = 0x30;    /* flags & 16 (0x10100120) */
inline constexpr std::size_t WatchOffset = 0x34;    /* watch end past the level time (0x10100126) */
inline constexpr std::size_t DiceOffset = 0x38;     /* the demon dice's count, or 0 (0x1010017f) */

/* The time left: the player's powerup time (+0x218, seconds by the values the
 * game writes) while the tea, the rage box or the glass is on; while only the
 * watch flag is, the float at 0x101fc3dc less the one at 0x101fc3cc -- by
 * inference the level's deadtime end less the level time; and zero otherwise
 * (0x1010014e-0x1010017c). */
struct PowerupState {
	float time = 0.0f;
	int tea = 0;
	int rageBox = 0;
	int glass = 0;
	int watch = 0;
	int dice = 0;
};

inline PowerupState ReadPowerupState( const void *block ) {
	PowerupState state;
	if ( !block ) return state;
	const unsigned char *bytes = static_cast<const unsigned char *>( block );
	std::memcpy( &state.time, bytes + PowerupTimeOffset, 4 );
	std::memcpy( &state.tea, bytes + TeaOffset, 4 );
	std::memcpy( &state.rageBox, bytes + RageBoxOffset, 4 );
	std::memcpy( &state.glass, bytes + GlassOffset, 4 );
	std::memcpy( &state.watch, bytes + WatchOffset, 4 );
	std::memcpy( &state.dice, bytes + DiceOffset, 4 );
	return state;
}

/* ---- the item foldout (Draw 0x00412a00) ----------------------------------- */

/* The powerup a card shows, in the order the foldout asks for them: the rage
 * box first, then the glass, the tea and the watch (0x00412a34-0x00412bb0). */
enum class Powerup { None = 0, Watch = 1, RageBox = 2, Tea = 3, Glass = 4 };

inline const char *PowerupModel( Powerup powerup ) {
	switch ( powerup ) {
	case Powerup::RageBox: return "models/hud_foldout_ragebox.tik";  /* 0x0050c760 */
	case Powerup::Glass: return "models/hud_foldout_glass.tik";      /* 0x0050c730 */
	case Powerup::Tea: return "models/hud_foldout_tea.tik";          /* 0x0050c704 */
	case Powerup::Watch: return "models/hud_foldout_watch.tik";      /* 0x0050c6e4 */
	default: return nullptr;
	}
}

inline Powerup ActivePowerup( const PowerupState &state ) {
	if ( state.rageBox ) return Powerup::RageBox;
	if ( state.glass ) return Powerup::Glass;
	if ( state.tea ) return Powerup::Tea;
	if ( state.watch ) return Powerup::Watch;
	return Powerup::None;
}

enum class ItemFold { Hidden = 0, Opening = 1, Timing = 2 };

/* The timer's span of the animation: the time left over the time at pickup,
 * times 35, plus 15, held at zero or more, on the second half -- a full timer
 * at frame 100, an empty one at 65 (0x004ff6cc, 0x004ff6c8). */
inline constexpr float TimerFrames = 35.0f;
inline constexpr float TimerFloor = 15.0f;

/* The constructor (0x004128a0) leaves the card hidden; its model is the
 * placeholder AliceHud's init gives it, which is never drawn. */
struct ItemFoldout {
	int timer = 0;                  /* +0x238 */
	ItemFold state = ItemFold::Hidden;   /* +0x244 */
	Powerup kind = Powerup::None;   /* +0x248 */
	int total = 0;                  /* +0x24c: the time left at pickup, truncated */
	float frame = 0.0f;             /* +0x21c */
	const char *model = Widgets[4].model;  /* +0x1d4 */
};

/* The time left over the time at pickup, as the x87 divide gives it: a zero
 * pickup time divides to an infinity, or to NaN when nothing is left. */
inline double TimerFraction( float time, int total ) {
	if ( total == 0 ) {
		if ( time > 0.0f ) return std::numeric_limits<double>::infinity();
		if ( time < 0.0f ) return -std::numeric_limits<double>::infinity();
		return std::numeric_limits<double>::quiet_NaN();
	}
	return static_cast<double>( time ) / static_cast<double>( total );
}

/* One Draw (0x00412a09-0x00412cf2): returns whether the card is drawn this
 * frame, at `f.frame`. A new powerup, or a different one, puts its model up
 * and unfolds the card over 200 ms; the card then shows the time left, and
 * goes as soon as the time left reads zero, which it does the moment every
 * powerup is off. The timer is never counted down while timing -- it only
 * says the card is up. */
inline bool StepItemFoldout( ItemFoldout &f, const PowerupState *block, int frameMs ) {
	f.frame = 0.0f;
	if ( !block ) {
		return false;
	}
	const Powerup active = ActivePowerup( *block );
	if ( active != Powerup::None ) {
		if ( f.state == ItemFold::Hidden || f.kind != active ) {
			f.model = PowerupModel( active );
			f.total = static_cast<int>( block->time );
			f.state = ItemFold::Opening;
			f.timer = FoldMs;
			f.kind = active;
		}
	} else if ( f.state == ItemFold::Opening ) {
		f.state = ItemFold::Timing;
		f.timer = FoldMs;
	} else if ( f.state == ItemFold::Hidden ) {
		return false;
	}

	if ( f.state == ItemFold::Timing ) {
		if ( f.timer <= 0 ) {
			f.state = ItemFold::Hidden;
			f.kind = Powerup::None;
			return false;
		}
		const double x = TimerFraction( block->time, f.total );
		double v = x * TimerFrames + TimerFloor;
		if ( v < 0.0 ) v = 0.0;
		f.frame = static_cast<float>( v + FoldHalf );
		if ( x == 0.0 || std::isnan( x ) ) {
			f.state = ItemFold::Hidden;
			return false;
		}
	} else if ( f.state == ItemFold::Opening ) {
		if ( f.timer > 0 ) {
			f.timer -= frameMs;
			f.frame = FoldFrames( f.timer );
		} else {
			f.state = ItemFold::Timing;
			f.timer = 1;
		}
	}

	// The frame is held to 0..100, a NaN reading as below (0x00412c8c).
	if ( !( f.frame >= 0.0f ) ) f.frame = 0.0f;
	if ( f.frame > FoldMaxFrame ) f.frame = FoldMaxFrame;
	return true;
}

} // namespace hud
} // namespace alice
} // namespace fntech3

#endif
