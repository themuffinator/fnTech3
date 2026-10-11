/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's model definitions and the queries its modules make
 * of them. See alice_tiki.h and docs/fntech3/2026-09-23-alice-tiki.md.
 *
 * Everything here was read out of retail alice.exe. The loading rules were
 * then checked the strongest way available: a compiler written from the same
 * reading reproduces every one of the 797 `.cik` images the installation
 * ships, field for field, from the `.tik` text beside it. */

#include "alice_tiki.h"
#include "qcommon.h"
#include "../compat/alice_skb_format.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace fntech3::alice;
using namespace fntech3::alice::tiki;

namespace {

/* ---- retail's string comparison ---------------------------------------- */

/* 0x0044be40 / 0x0044bda0: signed characters, a-z folded to upper case. This
 * is the collation retail sorts animations with, so it decides which index an
 * animation name gets -- and indices travel in the entity state. */
int FoldUpper( int c ) {
	return ( c >= 'a' && c <= 'z' ) ? c - ( 'a' - 'A' ) : c;
}

int RetailCompareN( const char *a, const char *b, int n ) {
	for ( ;; ) {
		if ( n-- == 0 ) {
			return 0;
		}
		const int ca = static_cast<signed char>( *a++ );
		const int cb = static_cast<signed char>( *b++ );
		if ( ca != cb ) {
			const int fa = FoldUpper( ca ), fb = FoldUpper( cb );
			if ( fa < fb ) return -1;
			if ( fa > fb ) return 1;
		}
		if ( !ca ) {
			return 0;
		}
	}
}

int RetailCompare( const char *a, const char *b ) {
	return RetailCompareN( a, b, 99999 );
}

/* The Visual C++ 6 run-time's qsort, as linked into alice.exe at 0x004e8ae7:
 * a selection sort that moves the maximum to the end for eight elements or
 * fewer, otherwise the middle element as pivot and the smaller side first.
 * Only its treatment of equal keys matters here -- one retail model names two
 * animations alike -- but that is enough to need the real algorithm. */
template<typename T, typename Compare>
void RetailQsort( std::vector<T> &a, Compare compare ) {
	const int n = static_cast<int>( a.size() );
	if ( n < 2 ) {
		return;
	}
	std::vector<std::pair<int, int>> stack;
	int lo = 0, hi = n - 1;
	for ( ;; ) {
		const int size = hi - lo + 1;
		bool pushed = false;
		if ( size <= 8 ) {
			for ( int h = hi; h > lo; --h ) {
				int mx = lo;
				for ( int p = lo + 1; p <= h; ++p ) {
					if ( compare( a[p], a[mx] ) > 0 ) mx = p;
				}
				std::swap( a[mx], a[h] );
			}
		} else {
			const int mid = lo + size / 2;
			std::swap( a[mid], a[lo] );
			int loguy = lo, higuy = hi + 1;
			for ( ;; ) {
				do { ++loguy; } while ( loguy <= hi && compare( a[loguy], a[lo] ) <= 0 );
				do { --higuy; } while ( higuy > lo && compare( a[higuy], a[lo] ) >= 0 );
				if ( higuy < loguy ) break;
				std::swap( a[loguy], a[higuy] );
			}
			std::swap( a[lo], a[higuy] );
			if ( higuy - 1 - lo >= hi - loguy ) {
				if ( lo + 1 < higuy ) stack.emplace_back( lo, higuy - 1 );
				if ( loguy < hi ) { lo = loguy; pushed = true; }
			} else {
				if ( loguy < hi ) stack.emplace_back( loguy, hi );
				if ( lo + 1 < higuy ) { hi = higuy - 1; pushed = true; }
			}
		}
		if ( pushed ) {
			continue;
		}
		if ( stack.empty() ) {
			return;
		}
		lo = stack.back().first;
		hi = stack.back().second;
		stack.pop_back();
	}
}

void TikiError( const char *fmt, ... ) {
	char text[1024];
	va_list argptr;

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	Com_DPrintf( "%s", text );
}

/* ---- the file cache ----------------------------------------------------- */

std::vector<std::unique_ptr<File>> files;
std::vector<std::unique_ptr<Model>> models;

const File *LoadFile( const char *name ) {
	for ( const auto &file : files ) {
		if ( !Q_stricmp( file->name, name ) ) {
			return file.get();
		}
	}
	union {
		unsigned char *b;
		void *v;
	} buffer;
	const int length = FS_ReadFile( name, &buffer.v );
	if ( length <= 0 || !buffer.v ) {
		return nullptr;
	}
	auto file = std::make_unique<File>();
	Q_strncpyz( file->name, name, sizeof( file->name ) );
	file->bytes.assign( buffer.b, buffer.b + length );
	FS_FreeFile( buffer.v );

	const unsigned char *data = file->bytes.data();
	const std::size_t bytes = file->bytes.size();
	if ( pose::OpenSkeleton( data, bytes, file->skeleton ) ) {
		/* The pose view checks the header and the bones; the surfaces and
		 * their vertices, which the renderer walks, are checked by the full
		 * reader before anything trusts them. */
		model::Skb skb;
		const model::SkbError error = model::ParseSkb( data, bytes, skb );
		if ( error != model::SkbError::None ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: skeleton %s is damaged: %s\n", name,
				model::SkbErrorName( error ) );
			return nullptr;
		}
		/* The reader has located every surface; keep them, so the renderer
		 * reaches any one without walking the chain from the first. */
		file->skbSurfaces.assign( skb.surface, skb.surface + skb.surfaces );
		file->kind = File::Kind::Skeleton;
	} else if ( pose::OpenSkeletalAnim( data, bytes, file->anim ) ) {
		file->kind = File::Kind::SkeletalAnim;
	} else if ( model::ParseTan( data, bytes, file->tan ) == model::Error::None
		&& file->tan.frames > 0 ) {
		file->kind = File::Kind::Tan;
		std::memcpy( file->tanDelta, data + 88, sizeof( file->tanDelta ) );
	} else {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s is not a model file Alice's engine accepts\n", name );
		return nullptr;
	}
	files.push_back( std::move( file ) );
	return files.back().get();
}

/* How many frames a file holds and how long the whole animation takes. */
int FileFrames( const File *file ) {
	if ( !file ) return 0;
	return file->kind == File::Kind::Tan ? file->tan.frames
		: file->kind == File::Kind::SkeletalAnim ? file->anim.frames : 0;
}

/* ---- the model cache ---------------------------------------------------- */

Model *ModelAt( int handle ) {
	if ( handle < 0 || handle >= static_cast<int>( models.size() ) || !models[handle] ) {
		return nullptr;
	}
	return models[handle].get();
}

Model *CheckModel( int handle, const char *where ) {
	Model *model = ModelAt( handle );
	if ( !model ) {
		TikiError( "%s: handle %d out of range\n", where, handle );
	}
	return model;
}

const Animation *CheckAnim( const Model *model, int anim, const char *where ) {
	if ( anim < 0 || anim >= static_cast<int>( model->animations.size() ) ) {
		TikiError( "%s: Anim %d out of range for %s.\n", where, anim, model->name );
		return nullptr;
	}
	return &model->animations[anim];
}

const char *Intern( Model &model, const char *text ) {
	model.strings.emplace_back( text );
	return model.strings.back().c_str();
}

/* ---- the compiled image ------------------------------------------------- */

/* A `.cik` is the version number 1 followed by retail's own in-memory record
 * (0x0045c600), so its offsets are relative to the record that holds them
 * and nothing in it can be trusted until it is bounded. */
class ImageReader {
public:
	ImageReader( const unsigned char *data, std::size_t bytes ) : data_( data ), bytes_( bytes ) {}

	bool Fits( std::size_t at, std::size_t length ) const {
		return at <= bytes_ && length <= bytes_ - at;
	}
	std::int32_t Int( std::size_t at ) const {
		std::int32_t v = 0;
		if ( Fits( at, 4 ) ) std::memcpy( &v, data_ + at, 4 );
		return v;
	}
	float Float( std::size_t at ) const {
		float v = 0.0f;
		if ( Fits( at, 4 ) ) std::memcpy( &v, data_ + at, 4 );
		return v;
	}
	/* A string that has to terminate within `limit` bytes of `at`. */
	bool String( std::size_t at, std::size_t limit, char *out, std::size_t outBytes ) const {
		if ( !Fits( at, 1 ) ) return false;
		const std::size_t room = limit < bytes_ - at ? limit : bytes_ - at;
		const void *end = std::memchr( data_ + at, 0, room );
		if ( !end ) return false;
		const std::size_t length = static_cast<const unsigned char *>( end ) - ( data_ + at );
		if ( length >= outBytes ) return false;
		std::memcpy( out, data_ + at, length + 1 );
		return true;
	}

private:
	const unsigned char *data_;
	std::size_t bytes_;
};

/* A command list: the frame, the argument count, and 32 argument offsets,
 * 0x88 bytes a command, each offset relative to its command. */
bool ReadImageCommands( const ImageReader &r, Model &model, std::size_t at, int count,
	int limit, std::vector<Command> &out ) {
	if ( count < 0 || count > limit ) return false;
	for ( int i = 0; i < count; ++i ) {
		const std::size_t base = at + 0x88 * static_cast<std::size_t>( i );
		if ( !r.Fits( base, 0x88 ) ) return false;
		Command command{};
		command.frame = r.Int( base );
		command.numArgs = r.Int( base + 4 );
		if ( command.numArgs < 0 || command.numArgs > MaxCommandArgs ) return false;
		for ( int k = 0; k < command.numArgs; ++k ) {
			char text[256];
			const std::int32_t offset = r.Int( base + 8 + 4 * k );
			if ( offset < 0 || !r.String( base + static_cast<std::size_t>( offset ), 256, text, sizeof( text ) ) ) {
				return false;
			}
			command.args[k] = Intern( model, text );
		}
		out.push_back( command );
	}
	return true;
}

bool LoadImage( const char *cikName, Model &model ) {
	union {
		unsigned char *b;
		void *v;
	} buffer;
	const int length = FS_ReadFile( cikName, &buffer.v );
	if ( length <= 0 || !buffer.v ) {
		return false;
	}
	std::vector<unsigned char> bytes( buffer.b, buffer.b + length );
	FS_FreeFile( buffer.v );
	if ( bytes.size() < 4 + 0x168 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s is truncated\n", cikName );
		return false;
	}
	std::int32_t version;
	std::memcpy( &version, bytes.data(), 4 );
	if ( version != 1 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s has version %d, not 1\n", cikName, version );
		return false;
	}
	const ImageReader r( bytes.data() + 4, bytes.size() - 4 );

	const int numSurfaces = r.Int( 0x80 );
	model.numTags = r.Int( 0x84 );
	const int numAnims = r.Int( 0x88 );
	model.scale = r.Float( 0xa4 );
	model.lodScale = r.Float( 0xa8 );
	model.lodBias = r.Float( 0xac );
	for ( int k = 0; k < 3; ++k ) {
		model.lightOffset[k] = r.Float( 0xb0 + 4 * k );
		model.origin[k] = r.Float( 0xbc + 4 * k );
		model.mins[k] = r.Float( 0xc8 + 4 * k );
		model.maxs[k] = r.Float( 0xd4 + 4 * k );
	}
	model.radius = r.Float( 0xe0 );
	if ( numSurfaces < 0 || numSurfaces > 64 || numAnims <= 0 || numAnims > MaxAnimations
		|| model.numTags < 0 || model.numTags > pose::MaxBones ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s has impossible counts\n", cikName );
		return false;
	}
	if ( !r.String( 0xe8, 128, model.skeletonName, sizeof( model.skeletonName ) ) ) {
		return false;
	}

	if ( !ReadImageCommands( r, model, static_cast<std::size_t>( r.Int( 0x94 ) ), r.Int( 0x90 ),
			MaxInitCommands, model.clientInit )
		|| !ReadImageCommands( r, model, static_cast<std::size_t>( r.Int( 0x9c ) ), r.Int( 0x98 ),
			MaxInitCommands, model.serverInit ) ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s has damaged init commands\n", cikName );
		return false;
	}

	/* Surfaces, 0x15c bytes each: the name, four shader names, four handles
	 * that were retail's and mean nothing here, the skin count, the flags and
	 * the damage multiplier. */
	const std::size_t surfaces = static_cast<std::size_t>( r.Int( 0xa0 ) );
	for ( int i = 0; i < numSurfaces; ++i ) {
		const std::size_t at = surfaces + 0x15c * static_cast<std::size_t>( i );
		if ( !r.Fits( at, 0x15c ) ) return false;
		Surface surface{};
		if ( !r.String( at, 64, surface.name, sizeof( surface.name ) ) ) return false;
		surface.numSkins = r.Int( at + 0x150 );
		if ( surface.numSkins < 0 || surface.numSkins > MaxSkins ) return false;
		for ( int k = 0; k < surface.numSkins; ++k ) {
			if ( !r.String( at + 0x40 + 64 * k, 64, surface.shaders[k], sizeof( surface.shaders[k] ) ) ) {
				return false;
			}
		}
		surface.flags = r.Int( at + 0x154 );
		surface.damage = r.Float( at + 0x158 );
		model.surfaces.push_back( surface );
	}

	/* Animations through the offset table at 0x168, 0xd0 bytes each, already
	 * in retail's sorted order. */
	for ( int i = 0; i < numAnims; ++i ) {
		const std::int32_t at = r.Int( 0x168 + 4 * i );
		if ( at < 0 || !r.Fits( static_cast<std::size_t>( at ), 0xd0 ) ) return false;
		const std::size_t a = static_cast<std::size_t>( at );
		Animation anim{};
		if ( !r.String( a, 48, anim.alias, sizeof( anim.alias ) )
			|| !r.String( a + 0x30, 128, anim.path, sizeof( anim.path ) ) ) {
			return false;
		}
		anim.weight = r.Float( a + 0xb0 );
		anim.crossblendMs = r.Int( a + 0xb4 );
		anim.flags = r.Int( a + 0xbc );
		if ( !ReadImageCommands( r, model, a + static_cast<std::size_t>( r.Int( a + 0xc4 ) ),
				r.Int( a + 0xc0 ), MaxClientFrameCommands, anim.client )
			|| !ReadImageCommands( r, model, a + static_cast<std::size_t>( r.Int( a + 0xcc ) ),
				r.Int( a + 0xc8 ), MaxServerFrameCommands, anim.server ) ) {
			return false;
		}
		/* Retail registers every animation again by the name stored in the
		 * image (0x0045c550). One the installation lacks stays unloaded, and
		 * every query about it answers as retail's does for a bad handle. */
		anim.file = LoadFile( anim.path );
		if ( !anim.file ) {
			TikiError( "TIKI: %s names animation %s, which does not load\n", model.name, anim.path );
		}
		model.animations.push_back( anim );
	}

	if ( model.skeletonName[0] ) {
		model.skeleton = LoadFile( model.skeletonName );
		if ( !model.skeleton || model.skeleton->kind != File::Kind::Skeleton ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: %s's skeleton %s does not load\n", cikName,
				model.skeletonName );
			return false;
		}
		/* A skeletal model's tags are its bones; an image that claims more
		 * would have every tag query read past the posed skeleton. */
		if ( model.numTags > model.skeleton->skeleton.bones ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: %s claims %d tags on a %d-bone skeleton\n", cikName,
				model.numTags, model.skeleton->skeleton.bones );
			return false;
		}
	}
	return true;
}

/* ---- the definition text ------------------------------------------------ */

/* Retail's script reader (0x00465xxx), which differs from Quake III's in ways
 * the installed definitions depend on: braces are not delimiters, `;` and `#`
 * begin comments as `//` does, a block comment ends only at `*` followed by a
 * backslash, `$define` and `$include` are directives, and `$name$` expands. */
/* How deep `$include` may nest before a file is refused. */
constexpr int kMaxIncludeDepth = 16;

class Script {
public:
	Script( std::string text, std::string name, Script *parent )
		: text_( std::move( text ) ), name_( std::move( name ) ), parent_( parent ) {
		if ( parent_ ) {
			defines_ = parent_->defines_;
		}
	}

	bool TokenAvailable( bool crossLine ) {
		if ( child_ ) {
			if ( child_->TokenAvailable( crossLine ) ) return true;
			if ( !crossLine || !child_->AtEnd() ) return false;
			child_.reset();
		}
		if ( unget_ ) return true;
		for ( ;; ) {
			while ( pos_ < text_.size() && Byte( pos_ ) <= ' ' ) {
				if ( text_[pos_] == '\n' ) {
					if ( !crossLine ) return false;
				}
				++pos_;
			}
			if ( pos_ >= text_.size() ) return false;
			if ( !IsComment() ) return true;
			if ( IsBlock() ) SkipBlock();
			if ( SkipToEol() ) return false;
		}
	}

	std::string GetToken( bool crossLine ) {
		for ( ;; ) {
			if ( child_ ) {
				std::string token = child_->GetToken( crossLine );
				if ( !token.empty() || !crossLine || !child_->AtEnd() ) return token;
				child_.reset();
			}
			if ( unget_ ) {
				unget_ = false;
				return token_;
			}
			SkipWhiteAndComments( crossLine );
			if ( child_ ) continue;
			if ( pos_ >= text_.size() ) return std::string();
			return text_[pos_] == '"' ? ReadString() : ReadWord();
		}
	}

	void UngetToken() {
		Script *s = this;
		while ( s->child_ ) s = s->child_.get();
		s->unget_ = true;
	}

	bool AtEnd() const { return pos_ >= text_.size() && !unget_; }

private:
	unsigned Byte( std::size_t at ) const { return static_cast<unsigned char>( text_[at] ); }

	bool IsComment() const {
		if ( pos_ >= text_.size() ) return false;
		const char c = text_[pos_];
		if ( c == ';' || c == '#' ) return true;
		if ( pos_ + 1 >= text_.size() ) return false;
		return c == '/' && ( text_[pos_ + 1] == '/' || text_[pos_ + 1] == '*' );
	}

	bool IsBlock() const {
		return pos_ + 1 < text_.size() && text_[pos_] == '/' && text_[pos_ + 1] == '*';
	}

	/* Retail looks for `*\`, not for the end of a C comment (0x00465444). */
	void SkipBlock() {
		while ( pos_ + 1 < text_.size() && !( text_[pos_] == '*' && text_[pos_ + 1] == '\\' ) ) {
			++pos_;
		}
		if ( pos_ + 1 >= text_.size() ) pos_ = text_.size();
	}

	bool SkipToEol() {
		while ( pos_ < text_.size() && text_[pos_] != '\n' ) ++pos_;
		return pos_ >= text_.size();
	}

	void SkipWhite( bool crossLine ) {
		while ( pos_ < text_.size() && Byte( pos_ ) <= ' ' ) {
			if ( text_[pos_] == '\n' && !crossLine ) return;
			++pos_;
		}
	}

	void SkipWhiteAndComments( bool crossLine ) {
		SkipWhite( crossLine );
		while ( pos_ < text_.size() && IsComment() ) {
			if ( IsBlock() ) {
				SkipBlock();
			} else {
				SkipToEol();
				SkipWhite( crossLine );
			}
		}
		while ( pos_ + 1 < text_.size() && text_[pos_] == '$' && text_[pos_ + 1] != '$' ) {
			if ( !Directive( crossLine ) ) return;
			if ( child_ || pos_ >= text_.size() ) return;
		}
	}

	void AddDefine( const std::string &name, const std::string &value ) {
		for ( Script *s = this; s; s = s->parent_ ) {
			bool found = false;
			for ( const auto &d : s->defines_ ) {
				if ( !RetailCompare( d.first.c_str(), name.c_str() ) ) { found = true; break; }
			}
			if ( !found && s->defines_.size() < 48 ) s->defines_.emplace_back( name, value );
		}
	}

	bool Directive( bool crossLine ) {
		const std::size_t eol = text_.find( '\n', pos_ );
		const std::string line = text_.substr( pos_ + 1, eol == std::string::npos ? std::string::npos : eol - pos_ - 1 );
		char command[64] = "", arg1[256] = "", arg2[256] = "";
		sscanf( line.c_str(), "%63s %255s %255s", command, arg1, arg2 );
		if ( !Q_stricmp( command, "define" ) ) {
			AddDefine( arg1, arg2 );
			SkipToEol();
			SkipWhite( crossLine );
			return true;
		}
		if ( !Q_stricmp( command, "include" ) ) {
			SkipToEol();
			SkipWhite( crossLine );
			/* Retail's limit is not established; a file that includes itself,
			 * directly or through others, would otherwise nest without end. */
			int depth = 0;
			bool cycle = false;
			for ( const Script *s = this; s; s = s->parent_ ) {
				++depth;
				cycle = cycle || !Q_stricmp( s->name_.c_str(), arg1 );
			}
			if ( cycle || depth >= kMaxIncludeDepth ) {
				TikiError( "Tiki:LoadFile refusing to include %s %s\n", arg1,
					cycle ? "inside itself" : "that deep" );
				return true;
			}
			union {
				char *c;
				void *v;
			} buffer;
			std::string text;
			const int length = FS_ReadFile( arg1, &buffer.v );
			if ( length > 0 && buffer.v ) {
				text.assign( buffer.c, buffer.c + length );
				FS_FreeFile( buffer.v );
			} else {
				TikiError( "Tiki:LoadFile Couldn't load %s\n", arg1 );
			}
			child_ = std::make_unique<Script>( std::move( text ), arg1, this );
			return true;
		}
		return false;
	}

	char Escape( char c ) const {
		switch ( c ) {
		case 'n': return '\n';
		default: return c;
		}
	}

	std::string ReadString() {
		++pos_;
		std::string out;
		while ( pos_ < text_.size() ) {
			const char c = text_[pos_];
			if ( c == '"' ) { ++pos_; break; }
			if ( c == '\n' ) break;
			if ( c == '\\' && pos_ + 1 < text_.size() ) {
				++pos_;
				out += Escape( text_[pos_] );
			} else {
				out += c;
			}
			++pos_;
		}
		token_ = out;
		return token_;
	}

	std::string ReadWord() {
		std::string out;
		std::size_t first = std::string::npos, second = std::string::npos;
		while ( pos_ < text_.size() && Byte( pos_ ) > ' ' && !IsComment() ) {
			const char c = text_[pos_];
			if ( c == '\\' && pos_ + 1 < text_.size() ) {
				++pos_;
				out += Escape( text_[pos_] );
			} else {
				if ( c == '$' ) {
					if ( first == std::string::npos ) first = out.size();
					else second = out.size();
				}
				out += c;
			}
			++pos_;
			if ( out.size() >= 255 ) break;
		}
		if ( first != std::string::npos ) {
			if ( second == std::string::npos ) second = out.size();
			if ( second - first - 1 == 0 ) {
				out = out.substr( 0, first ) + "$" + ( second + 1 < out.size() ? out.substr( second + 1 ) : "" );
			} else {
				const std::string name = out.substr( first + 1, second - first - 1 );
				for ( const auto &d : defines_ ) {
					if ( !RetailCompare( d.first.c_str(), name.c_str() ) ) {
						out = out.substr( 0, first ) + d.second
							+ ( second + 1 < out.size() ? out.substr( second + 1 ) : "" );
						break;
					}
				}
			}
		}
		token_ = out;
		return token_;
	}

	std::string text_;
	std::string name_;
	Script *parent_;
	std::unique_ptr<Script> child_;
	std::vector<std::pair<std::string, std::string>> defines_;
	std::size_t pos_ = 0;
	bool unget_ = false;
	std::string token_;
};

/* The pieces a definition's text declares, before they are bound to the
 * model's own surfaces and sorted. */
struct PendingSurface {
	std::string name;
	std::vector<std::string> shaders;
	int flags = 0;
	float damage = 0.0f;
};

struct PendingAnim {
	Animation anim;
	std::vector<std::vector<std::string>> clientArgs, serverArgs;
	std::vector<int> clientFrames, serverFrames;
};

struct PendingModel {
	std::string path;
	std::vector<PendingSurface> surfaces;
	std::vector<PendingAnim> anims;
	std::vector<std::vector<std::string>> clientInit, serverInit;
};

void SkipRestOfLine( Script &s ) {
	while ( s.TokenAvailable( false ) ) s.GetToken( false );
}

float GetFloat( Script &s ) {
	return static_cast<float>( atof( s.GetToken( false ).c_str() ) );
}

bool ParseSetup( Script &s, Model &model, PendingModel &p ) {
	s.GetToken( true );	// the opening brace, unchecked as retail leaves it
	while ( s.TokenAvailable( true ) ) {
		const std::string k = s.GetToken( true );
		if ( !Q_stricmp( k.c_str(), "scale" ) ) {
			const float v = GetFloat( s );
			model.scale = v > 0.0f ? v : 1.0f;
		} else if ( !Q_stricmp( k.c_str(), "lod_scale" ) ) {
			model.lodScale = GetFloat( s ) * 0.2f;
		} else if ( !Q_stricmp( k.c_str(), "lod_bias" ) ) {
			model.lodBias = GetFloat( s );
		} else if ( !Q_stricmp( k.c_str(), "skelmodel" ) ) {
			const std::string n = s.GetToken( false );
			if ( model.skeleton ) {
				TikiError( "TIKI: skelmodel called twice in %s\n", model.name );
				continue;
			}
			Q_strncpyz( model.skeletonName, ( p.path + n ).c_str(), sizeof( model.skeletonName ) );
			model.skeleton = LoadFile( model.skeletonName );
			if ( !model.skeleton || model.skeleton->kind != File::Kind::Skeleton ) {
				return false;
			}
		} else if ( !Q_stricmp( k.c_str(), "path" ) ) {
			p.path = s.GetToken( false );
			if ( !p.path.empty() && p.path.back() != '/' && p.path.back() != '\\' ) p.path += '/';
		} else if ( !Q_stricmp( k.c_str(), "origin" ) ) {
			for ( float &v : model.origin ) v = GetFloat( s );
		} else if ( !Q_stricmp( k.c_str(), "lightoffset" ) ) {
			for ( float &v : model.lightOffset ) v = GetFloat( s );
		} else if ( !Q_stricmp( k.c_str(), "radius" ) ) {
			model.radius = GetFloat( s );
		} else if ( !Q_stricmp( k.c_str(), "surface" ) ) {
			const std::string name = s.GetToken( false );
			PendingSurface *entry = nullptr;
			for ( auto &e : p.surfaces ) {
				if ( !RetailCompare( e.name.c_str(), name.c_str() ) ) { entry = &e; break; }
			}
			if ( !entry ) {
				p.surfaces.emplace_back();
				entry = &p.surfaces.back();
			}
			entry->name = name;
			while ( s.TokenAvailable( false ) ) {
				const std::string kw = s.GetToken( false );
				if ( !Q_stricmp( kw.c_str(), "flags" ) ) {
					static const struct { const char *name; int value; } flags[] = {
						{ "skin1", 1 }, { "skin2", 2 }, { "skin3", 3 }, { "nodraw", SurfaceNoDraw },
						{ "nodamage", SurfaceNoDamage }, { "crossfade", SurfaceCrossfade },
						{ "nomipmaps", SurfaceNoMipmaps }, { "nopicmip", SurfaceNoPicmip }
					};
					const std::string f = s.GetToken( false );
					for ( const auto &flag : flags ) {
						if ( !RetailCompare( f.c_str(), flag.name ) ) { entry->flags |= flag.value; break; }
					}
				} else if ( !Q_stricmp( kw.c_str(), "damage" ) ) {
					entry->damage = GetFloat( s );
				} else if ( !Q_stricmp( kw.c_str(), "shader" ) ) {
					const std::string shader = s.GetToken( false );
					if ( entry->shaders.size() < MaxSkins ) {
						entry->shaders.push_back( shader.find( '.' ) != std::string::npos ? p.path + shader : shader );
					}
				}
			}
		} else if ( k == "}" ) {
			return true;
		} else {
			SkipRestOfLine( s );
		}
	}
	return true;
}

void ParseCommandBlock( Script &s, int room, std::vector<std::vector<std::string>> &out ) {
	s.GetToken( true );
	while ( s.TokenAvailable( true ) ) {
		const std::string token = s.GetToken( true );
		if ( token == "}" ) break;
		if ( room <= 0 ) {
			SkipRestOfLine( s );
			continue;
		}
		s.UngetToken();
		std::vector<std::string> args;
		while ( s.TokenAvailable( false ) ) {
			const std::string a = s.GetToken( false );
			if ( args.size() < MaxCommandArgs ) args.push_back( a );
		}
		out.push_back( std::move( args ) );
		--room;
	}
}

void ParseInit( Script &s, PendingModel &p ) {
	s.GetToken( true );
	while ( s.TokenAvailable( true ) ) {
		const std::string token = s.GetToken( true );
		if ( !Q_stricmp( token.c_str(), "client" ) ) {
			ParseCommandBlock( s, MaxInitCommands - static_cast<int>( p.clientInit.size() ), p.clientInit );
		} else if ( !Q_stricmp( token.c_str(), "server" ) ) {
			ParseCommandBlock( s, MaxInitCommands - static_cast<int>( p.serverInit.size() ), p.serverInit );
		} else if ( token == "}" ) {
			return;
		} else {
			SkipRestOfLine( s );
		}
	}
}

void ParseFrameCommands( Script &s, int limit, int numFrames, std::vector<int> &frames,
	std::vector<std::vector<std::string>> &commands ) {
	int frame = 0;
	bool paren = false;
	s.GetToken( true );
	while ( s.TokenAvailable( true ) ) {
		const std::string k = s.GetToken( true );
		if ( k == "}" ) return;
		if ( static_cast<int>( commands.size() ) >= limit ) {
			SkipRestOfLine( s );
			continue;
		}
		if ( !Q_stricmp( k.c_str(), "start" ) || !Q_stricmp( k.c_str(), "first" ) ) {
			frame = 0;
		} else if ( !Q_stricmp( k.c_str(), "end" ) || !Q_stricmp( k.c_str(), "last" ) ) {
			frame = numFrames - 1;
		} else if ( !Q_stricmp( k.c_str(), "every" ) ) {
			frame = FrameEvery;
		} else if ( !Q_stricmp( k.c_str(), "exit" ) ) {
			frame = FrameExit;
		} else if ( !Q_stricmp( k.c_str(), "entry" ) || !Q_stricmp( k.c_str(), "enter" ) ) {
			frame = FrameEntry;
		} else if ( k == "(" ) {
			/* A bracket is a command of its own, under the previous frame:
			 * the modules read these as the edges of an event group. */
			paren = true;
			s.UngetToken();
		} else if ( k == ")" ) {
			paren = false;
			s.UngetToken();
		} else if ( paren ) {
			s.UngetToken();
		} else {
			frame = atoi( k.c_str() );
		}
		if ( frame <= -4 || frame >= numFrames ) {
			TikiError( "TIKI: illegal frame number %d\n", frame );
			SkipRestOfLine( s );
			continue;
		}
		std::vector<std::string> args;
		while ( s.TokenAvailable( false ) ) {
			const std::string a = s.GetToken( false );
			if ( args.size() < MaxCommandArgs ) args.push_back( a );
		}
		frames.push_back( frame );
		commands.push_back( std::move( args ) );
	}
}

void SkipBraceBlock( Script &s ) {
	int depth = 0;
	while ( s.TokenAvailable( true ) ) {
		const std::string t = s.GetToken( true );
		if ( t == "{" ) {
			++depth;
		} else if ( t == "}" ) {
			if ( --depth == 0 ) return;
		}
	}
}

void ParseAnimations( Script &s, PendingModel &p ) {
	s.GetToken( true );
	while ( s.TokenAvailable( true ) ) {
		const std::string alias = s.GetToken( true );
		if ( alias == "}" ) return;
		if ( p.anims.size() >= MaxAnimations ) continue;
		const std::string path = p.path + s.GetToken( false );
		const File *file = LoadFile( path.c_str() );
		if ( !file || file->kind == File::Kind::Skeleton || FileFrames( file ) <= 0 ) {
			/* A missing animation takes its flags and its command block with
			 * it; 49 retail references name files the installation lacks. */
			SkipRestOfLine( s );
			if ( !s.TokenAvailable( true ) ) continue;
			const std::string t = s.GetToken( true );
			s.UngetToken();
			if ( t == "{" ) SkipBraceBlock( s );
			continue;
		}
		PendingAnim a{};
		Q_strncpyz( a.anim.alias, alias.c_str(), sizeof( a.anim.alias ) );
		Q_strncpyz( a.anim.path, path.c_str(), sizeof( a.anim.path ) );
		a.anim.file = file;
		a.anim.weight = 1.0f;
		a.anim.crossblendMs = 200;
		a.anim.flags = 0;
		while ( s.TokenAvailable( false ) ) {
			const std::string k = s.GetToken( false );
			if ( !Q_stricmp( k.c_str(), "weight" ) ) a.anim.weight = GetFloat( s );
			else if ( !Q_stricmp( k.c_str(), "deltadriven" ) ) a.anim.flags |= AnimDeltaDriven;
			else if ( !Q_stricmp( k.c_str(), "default_angles" ) ) a.anim.flags |= AnimDefaultAngles;
			else if ( !Q_stricmp( k.c_str(), "notimecheck" ) ) a.anim.flags |= AnimNoTimeCheck;
			else if ( !Q_stricmp( k.c_str(), "torso_adjust" ) ) a.anim.flags |= AnimTorsoAdjust;
			else if ( !Q_stricmp( k.c_str(), "crossblend" ) ) a.anim.crossblendMs = atoi( s.GetToken( false ).c_str() );
		}
		if ( s.TokenAvailable( true ) ) {
			const std::string t = s.GetToken( true );
			if ( t == "{" ) {
				s.UngetToken();
				/* Retail validates a version 2 animation's command frames
				 * against the word where version 3 keeps its frame count --
				 * which in version 2 is the bone count (0x0045ad44). */
				int numFrames = FileFrames( file );
				if ( file->kind == File::Kind::SkeletalAnim && file->anim.version == 2 ) {
					numFrames = file->anim.bones;
				}
				s.GetToken( true );
				while ( s.TokenAvailable( true ) ) {
					const std::string side = s.GetToken( true );
					if ( !Q_stricmp( side.c_str(), "client" ) ) {
						ParseFrameCommands( s, MaxClientFrameCommands, numFrames, a.clientFrames, a.clientArgs );
					} else if ( !Q_stricmp( side.c_str(), "server" ) ) {
						ParseFrameCommands( s, MaxServerFrameCommands, numFrames, a.serverFrames, a.serverArgs );
					} else if ( side == "}" ) {
						break;
					} else {
						SkipRestOfLine( s );
					}
				}
			} else {
				s.UngetToken();
			}
		}
		p.anims.push_back( std::move( a ) );
	}
}

std::vector<Command> BuildCommands( Model &model, const std::vector<int> *frames,
	const std::vector<std::vector<std::string>> &args ) {
	std::vector<Command> out;
	for ( std::size_t i = 0; i < args.size(); ++i ) {
		Command c{};
		c.frame = frames ? ( *frames )[i] : 0;
		c.numArgs = static_cast<int>( args[i].size() );
		for ( int k = 0; k < c.numArgs; ++k ) c.args[k] = Intern( model, args[i][k].c_str() );
		out.push_back( c );
	}
	return out;
}

/* A model's own surface names, in file order: the skeleton's, or the first
 * animation's for a vertex-animated model. */
std::vector<std::string> ModelSurfaceNames( const File *file ) {
	std::vector<std::string> names;
	if ( !file ) return names;
	const unsigned char *d = file->bytes.data();
	if ( file->kind == File::Kind::Skeleton ) {
		std::size_t at = file->skeleton.ofsSurfaces;
		for ( int i = 0; i < file->skeleton.surfaces; ++i ) {
			if ( at + 96 > file->bytes.size() ) break;
			char name[64];
			std::memcpy( name, d + at + 4, 64 );
			name[63] = '\0';
			names.push_back( name );
			std::int32_t next;
			std::memcpy( &next, d + at + 92, 4 );
			if ( next <= 0 ) break;
			at += static_cast<std::size_t>( next );
		}
	} else if ( file->kind == File::Kind::Tan ) {
		for ( int i = 0; i < file->tan.surfaces; ++i ) {
			char name[64];
			std::memcpy( name, d + file->tan.surface[i].name, 64 );
			name[63] = '\0';
			names.push_back( name );
		}
	}
	return names;
}

bool CompileText( const char *name, Model &model ) {
	union {
		char *c;
		void *v;
	} buffer;
	const int length = FS_ReadFile( name, &buffer.v );
	if ( length <= 0 || !buffer.v ) {
		/* A definition that is not there is a developer's note, as retail's
		 * definition loader makes it (TIKI_LoadTIKIfile 0x0045bcf7 into
		 * 0x0046637d); its renderer adds nothing (0x0048982e). */
		if ( length < 0 ) {
			TikiError( "Tiki:LoadFile Couldn't load %s\n", name );
		}
		if ( buffer.v ) {
			FS_FreeFile( buffer.v );
		}
		return false;
	}
	std::string text( buffer.c, buffer.c + length );
	FS_FreeFile( buffer.v );

	Script s( std::move( text ), name, nullptr );
	if ( s.GetToken( true ) != "TIKI" ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: %s has no TIKI header\n", name );
		return false;
	}

	PendingModel p;
	const char *slash = strrchr( name, '/' );
	p.path = slash && slash - name + 1 >= 2 ? std::string( name, slash - name + 1 ) : "?model?";
	model.scale = 1.0f;
	model.lodScale = 1.0f;

	while ( s.TokenAvailable( true ) ) {
		const std::string section = s.GetToken( true );
		if ( !Q_stricmp( section.c_str(), "setup" ) ) {
			if ( !ParseSetup( s, model, p ) ) return false;
		} else if ( !Q_stricmp( section.c_str(), "init" ) ) {
			ParseInit( s, p );
		} else if ( !Q_stricmp( section.c_str(), "animations" ) ) {
			ParseAnimations( s, p );
		} else {
			if ( s.GetToken( true ) != "{" ) continue;
			s.UngetToken();
			SkipBraceBlock( s );
		}
	}
	if ( p.anims.empty() ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: No valid animations found in %s\n", name );
		return false;
	}

	/* The model's surfaces and tags come from its skeleton, or from its first
	 * animation in file order when it has none, and every animation has to
	 * agree with them. */
	const File *shape = model.skeleton ? model.skeleton : p.anims.front().anim.file;
	if ( model.skeleton ) {
		model.numTags = model.skeleton->skeleton.bones;
		for ( const auto &a : p.anims ) {
			if ( a.anim.file->kind != File::Kind::SkeletalAnim
				|| a.anim.file->anim.bones != model.skeleton->skeleton.bones ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: %s mixes skeletons\n", name );
				return false;
			}
		}
	} else {
		if ( shape->kind != File::Kind::Tan ) return false;
		model.numTags = shape->tan.tags;
		for ( const auto &a : p.anims ) {
			if ( a.anim.file->kind != File::Kind::Tan || a.anim.file->tan.surfaces != shape->tan.surfaces
				|| a.anim.file->tan.tags != shape->tan.tags ) {
				Com_Printf( S_COLOR_YELLOW "WARNING: %s mixes vertex models\n", name );
				return false;
			}
		}
	}

	/* Bounds: every frame of every animation, with the setup origin, before
	 * the model's scale. */
	float mins[3] = { 99999.0f, 99999.0f, 99999.0f }, maxs[3] = { -99999.0f, -99999.0f, -99999.0f };
	for ( const auto &a : p.anims ) {
		const File *f = a.anim.file;
		for ( int frame = 0; frame < FileFrames( f ); ++frame ) {
			float lo[3], hi[3];
			if ( f->kind == File::Kind::Tan ) {
				const pose::TanFrame tf = pose::ReadTanFrame( f->bytes.data() + f->tan.frameData + pose::TanFrameBytes * frame );
				VectorCopy( tf.mins, lo );
				VectorCopy( tf.maxs, hi );
			} else {
				pose::FrameBounds( f->anim, frame, lo, hi );
			}
			for ( int k = 0; k < 3; ++k ) {
				const float l = lo[k] + model.origin[k], h = hi[k] + model.origin[k];
				if ( l < mins[k] ) mins[k] = l;
				if ( l > maxs[k] ) maxs[k] = l;
				if ( h < mins[k] ) mins[k] = h;
				if ( h > maxs[k] ) maxs[k] = h;
			}
		}
	}
	float extent[3];
	VectorSubtract( maxs, mins, extent );
	if ( VectorLength( extent ) > 100000.0f ) {
		VectorSet( mins, -4, -4, -4 );
		VectorSet( maxs, 4, 4, 4 );
	}
	VectorCopy( mins, model.mins );
	VectorCopy( maxs, model.maxs );
	if ( model.radius == 0.0f ) {
		float radius = 0.0f;
		for ( int c = 0; c < 8; ++c ) {
			vec3_t corner = { ( c & 1 ) ? maxs[0] : mins[0], ( c & 2 ) ? maxs[1] : mins[1],
				( c & 4 ) ? maxs[2] : mins[2] };
			const float r = VectorLength( corner );
			if ( r > radius ) radius = r;
		}
		model.radius = radius * model.scale;
	}

	/* Surfaces bound to the model's own names: a plain name binds the first
	 * surface of that name, `prefix*` every surface it prefixes, and `all`
	 * every surface. A surface no entry binds keeps an empty name. */
	const std::vector<std::string> names = ModelSurfaceNames( shape );
	model.surfaces.assign( names.size(), Surface{} );
	auto bind = []( Surface &out, const std::string &modelName, const PendingSurface &e ) {
		for ( const std::string &shader : e.shaders ) {
			if ( out.numSkins < MaxSkins ) {
				Q_strncpyz( out.name, modelName.c_str(), sizeof( out.name ) );
				Q_strncpyz( out.shaders[out.numSkins++], shader.c_str(), sizeof( out.shaders[0] ) );
			}
		}
		out.flags |= e.flags;
		out.damage = e.damage;
	};
	for ( const auto &e : p.surfaces ) {
		const std::size_t star = e.name.find( '*' );
		const bool all = !Q_stricmp( e.name.c_str(), "all" );
		if ( star == std::string::npos && !all ) {
			for ( std::size_t i = 0; i < names.size(); ++i ) {
				if ( !RetailCompare( e.name.c_str(), names[i].c_str() ) ) {
					bind( model.surfaces[i], names[i], e );
					break;
				}
			}
		} else {
			for ( std::size_t i = 0; i < names.size(); ++i ) {
				if ( all || ( star > 0 && !RetailCompareN( e.name.c_str(), names[i].c_str(), static_cast<int>( star ) ) ) ) {
					bind( model.surfaces[i], names[i], e );
				}
			}
		}
	}

	/* Retail sorts the animations by name before it hands out an index. */
	RetailQsort( p.anims, []( const PendingAnim &a, const PendingAnim &b ) {
		return RetailCompare( a.anim.alias, b.anim.alias );
	} );
	for ( auto &a : p.anims ) {
		Animation anim = a.anim;
		anim.client = BuildCommands( model, &a.clientFrames, a.clientArgs );
		anim.server = BuildCommands( model, &a.serverFrames, a.serverArgs );
		model.animations.push_back( std::move( anim ) );
	}
	model.clientInit = BuildCommands( model, nullptr, p.clientInit );
	model.serverInit = BuildCommands( model, nullptr, p.serverInit );
	model.compiledFromText = true;
	return true;
}

/* ---- tag orientation ---------------------------------------------------- */

void ZeroOrientation( float origin[3], float axis[3][3] ) {
	VectorClear( origin );
	for ( int i = 0; i < 3; ++i ) VectorClear( axis[i] );
}

void IdentityOrientation( float origin[3], float axis[3][3] ) {
	VectorClear( origin );
	for ( int i = 0; i < 3; ++i ) {
		VectorClear( axis[i] );
		axis[i][i] = 1.0f;
	}
}

int ControllersFrom( const fntech3_alice_bonectrl_t *records, pose::Controller out[FNTECH3_ALICE_BONE_CONTROLLERS] ) {
	int n = 0;
	if ( !records ) return 0;
	for ( int i = 0; i < FNTECH3_ALICE_BONE_CONTROLLERS; ++i ) {
		if ( records[i].tag < 0 ) continue;
		/* A record the module cleared and never set is no rotation. */
		if ( records[i].quat[0] == 0.0f && records[i].quat[1] == 0.0f
			&& records[i].quat[2] == 0.0f && records[i].quat[3] == 0.0f ) continue;
		out[n].bone = records[i].tag;
		out[n].q = pose::Quat{ records[i].quat[0], records[i].quat[1], records[i].quat[2], records[i].quat[3] };
		out[n].scale = records[i].scale;
		++n;
	}
	return n;
}

const pose::SkeletalAnim *AnimView( const Model &model, int anim ) {
	if ( anim < 0 || anim >= static_cast<int>( model.animations.size() ) ) return nullptr;
	const File *f = model.animations[anim].file;
	return f && f->kind == File::Kind::SkeletalAnim ? &f->anim : nullptr;
}

/* The skeletal tag path: pose the whole skeleton -- the tag's chain is a
 * subset of it -- and read the tag bone. The model's origin is added to the
 * root, which the tag routine does and the renderer does not. */
void SkeletalTag( const Model &model, const pose::Sample &legs, const pose::Sample *torso,
	const fntech3_alice_bonectrl_t *records, int tag, float origin[3], float axis[3][3] ) {
	const pose::Skeleton &skeleton = model.skeleton->skeleton;
	std::vector<pose::LocalPose> local( skeleton.bones );
	std::vector<pose::BoneTransform> world( skeleton.bones );
	pose::PoseLocal( skeleton, legs, torso, local.data() );
	for ( int b = 0; b < skeleton.bones; ++b ) {
		if ( pose::BoneParent( skeleton, b ) < 0 ) {
			for ( int k = 0; k < 3; ++k ) local[b].t[k] += model.origin[k];
		}
	}
	pose::Controller controllers[FNTECH3_ALICE_BONE_CONTROLLERS];
	const int numControllers = ControllersFrom( records, controllers );
	pose::PoseWorld( skeleton, local.data(), controllers, numControllers, world.data() );
	const pose::BoneTransform &bone = world[tag];
	/* The origin carries the chain's controller scale times the model's. */
	for ( int k = 0; k < 3; ++k ) origin[k] = bone.origin[k] * model.scale;
	pose::ToMatrix( bone.q, 1.0f, axis );
}

void TanTag( const Model &model, const File *file, int frame, int tag, float scale,
	float origin[3], float axis[3][3] ) {
	if ( tag < 0 || tag >= file->tan.tags || frame < 0 || frame >= file->tan.frames ) {
		ZeroOrientation( origin, axis );
		return;
	}
	const unsigned char *d = file->bytes.data() + file->tan.tag[tag].data + 48 * static_cast<std::size_t>( frame );
	float stored[12];
	std::memcpy( stored, d, sizeof( stored ) );
	/* The model's setup origin is added before scaling, as retail's vertex
	 * path draws it (TIKI_Tag_Orientation 0x0045fada..0x0045fb30; the Ex and
	 * lerped paths at 0x0045f350 and 0x0046116c do the same). */
	const double factor = static_cast<double>( scale ) * model.scale;
	for ( int k = 0; k < 3; ++k ) {
		const float placed = model.origin[k] + stored[k];
		origin[k] = static_cast<float>( factor * placed );
	}
	for ( int i = 0; i < 3; ++i ) {
		for ( int k = 0; k < 3; ++k ) axis[i][k] = stored[3 + 3 * i + k];
	}
}

/* ---- aliases ------------------------------------------------------------ */

/* A model's list as retail's TIKI_Alias_* wrappers reach it through
 * TIKI_GetTiki (0x0045c910): none for a handle that names no model, and none
 * until the first addition creates it, named after the model (0x0045cc00). */
alias::List *ModelAliases( int handle, bool create ) {
	Model *model = CheckModel( handle, "TIKI_GetTiki" );
	if ( !model ) return nullptr;
	if ( !model->aliasList && create ) model->aliasList.reset( alias::CreateList( model->name ) );
	return model->aliasList.get();
}

/* -1 is the engine's global list, which each of its wrappers creates on first
 * use (0x00423a10 to 0x00423af0). */
alias::List *Aliases( int handle, bool create ) {
	return handle == -1 ? alias::Global() : ModelAliases( handle, create );
}

} // namespace

/* ======================================================================== */

int AliceTiki_Register( const char *name ) {
	if ( !name || !name[0] ) {
		return -1;
	}
	for ( std::size_t i = 0; i < models.size(); ++i ) {
		if ( models[i] && !Q_stricmp( models[i]->name, name ) ) {
			return static_cast<int>( i );
		}
	}
	/* Retail's cache holds 384 definitions (0x00459615). */
	if ( models.size() >= 384 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: TIKI cache full, %s not loaded\n", name );
		return -1;
	}

	auto model = std::make_unique<Model>();
	Q_strncpyz( model->name, name, sizeof( model->name ) );

	char cik[MAX_QPATH];
	COM_StripExtension( name, cik, sizeof( cik ) );
	Q_strcat( cik, sizeof( cik ), ".cik" );
	if ( !LoadImage( cik, *model ) ) {
		auto fresh = std::make_unique<Model>();
		Q_strncpyz( fresh->name, name, sizeof( fresh->name ) );
		model = std::move( fresh );
		if ( !CompileText( name, *model ) ) {
			return -1;
		}
	}
	models.push_back( std::move( model ) );
	return static_cast<int>( models.size() - 1 );
}

const AliceTikiModel *AliceTiki_Get( int handle ) {
	return ModelAt( handle );
}

int AliceTiki_Count( void ) {
	return static_cast<int>( models.size() );
}

/* Each model's alias list goes with it, as TIKI_FreeTiki frees it
 * (0x0045ca7f); the engine's global list is not the cache's and stays.
 *
 * Where retail empties its cache, read from alice.exe: never all at once --
 * its free-all loop (0x0045cb80) has no caller. The renderer frees
 * definitions instead, through its import slot 29, which is TIKI_FreeTiki
 * (0x0045ca30, written into the import table at 0x0042c5a7): its export +0x20
 * (0x0048a4e0), which the engine calls as it ends a registration (0x0040e705)
 * and the client game reaches as its slot 49, frees the definition behind
 * every model not registered again in the current sequence; its export +0x94
 * (0x0048ab00), the client game's slot 93, frees every model's and loads it
 * again by name. The server's model table (0x006a0070) is only zeroed on a
 * full spawn (0x00453920, from 0x004551de) and frees nothing.
 *
 * fnTech3 empties the whole cache once per level instead, from
 * SV_SpawnServer after the old game, the client game and the renderer have
 * all shut down and before the new game registers a model
 * (SV_AliceFlushModels, server_alice_save.hpp): nothing then holds a handle
 * or a definition pointer into it, and a long session cannot fill its 384
 * slots. A client connected to another process's server has no such flush. */
void AliceTiki_Clear( void ) {
	models.clear();
	files.clear();
}

/* 0x00462460 walks the cache's loaded definitions, stopping once it has seen
 * as many as are loaded, and for each one with an alias list frees every
 * line's actor records (0x004229b0) and sorts the list (0x00422e30, which
 * does nothing to a list already sorted). The engine's global list is not
 * the cache's and keeps its actors. */
void AliceTiki_ResetDialogActors( void ) {
	for ( const auto &model : models ) {
		if ( model && model->aliasList ) {
			alias::ClearActors( model->aliasList.get() );
			alias::Sort( model->aliasList.get() );
		}
	}
}

const fntech3::alice::tiki::File *AliceTiki_FindFile( const char *name ) {
	return name ? LoadFile( name ) : nullptr;
}

const char *AliceTiki_Name( int handle ) {
	const Model *m = CheckModel( handle, "TIKI_NameForNum" );
	return m ? m->name : nullptr;
}

int AliceTiki_NumAnims( int handle ) {
	const Model *m = CheckModel( handle, "TIKI_NumAnims" );
	return m ? static_cast<int>( m->animations.size() ) : 0;
}

int AliceTiki_NumSurfaces( int handle ) {
	const Model *m = CheckModel( handle, "TIKI_NumSurfaces" );
	return m ? static_cast<int>( m->surfaces.size() ) : 0;
}

int AliceTiki_NumTags( int handle ) {
	const Model *m = CheckModel( handle, "TIKI_NumTags" );
	return m ? m->numTags : 0;
}

void AliceTiki_CalculateBounds( int handle, float scale, vec3_t mins, vec3_t maxs ) {
	const Model *m = CheckModel( handle, "TIKI_CalculateBounds" );
	if ( !m ) {
		VectorClear( mins );
		VectorClear( maxs );
		return;
	}
	for ( int k = 0; k < 3; ++k ) {
		mins[k] = m->mins[k] * m->scale * scale;
		maxs[k] = m->maxs[k] * m->scale * scale;
	}
}

static qboolean AliceTiki_FillCommands( const std::vector<Command> &commands, int frame, bool initBlock,
	fntech3_alice_tiki_cmd_t *out ) {
	out->num_cmds = 0;
	for ( const Command &c : commands ) {
		if ( !initBlock && !( c.frame == frame || ( c.frame == FrameEvery && frame >= 0 ) ) ) {
			continue;
		}
		if ( out->num_cmds >= FNTECH3_ALICE_MAX_TIKI_CMDS ) {
			TikiError( "TIKI: too many commands\n" );
			return qfalse;
		}
		fntech3_alice_tiki_singlecmd_t &cmd = out->cmds[out->num_cmds++];
		cmd.num_args = c.numArgs;
		for ( int k = 0; k < c.numArgs; ++k ) {
			/* The module reads these as its own pointers: Win32 x86 only,
			 * which is the only place Alice's modules load. */
			cmd.args[k] = static_cast<uint32_t>( reinterpret_cast<uintptr_t>( c.args[k] ) );
		}
	}
	return qtrue;
}

qboolean AliceTiki_InitCommands( int handle, qboolean client, fntech3_alice_tiki_cmd_t *out ) {
	if ( out ) out->num_cmds = 0;
	const Model *m = CheckModel( handle, "TIKI_InitCommands" );
	if ( !m || !out ) return qfalse;
	return AliceTiki_FillCommands( client ? m->clientInit : m->serverInit, 0, true, out );
}

const char *AliceTiki_AnimName( int handle, int anim ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_NameForNum" );
	if ( !m ) return nullptr;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_NameForNum" );
	return a ? a->alias : nullptr;
}

int AliceTiki_AnimNumForName( int handle, const char *name ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_NumForName" );
	if ( !m || !name ) return -1;
	for ( std::size_t i = 0; i < m->animations.size(); ++i ) {
		if ( !RetailCompare( m->animations[i].alias, name ) ) return static_cast<int>( i );
	}
	return -1;
}

int AliceTiki_AnimRandom( int handle, const char *name ) {
	/* No name, or an empty one, is no match, before the model is looked up
	 * (0x0045d191, 0x0045d1a2..0x0045d1a7). */
	if ( !name || !name[0] ) return -1;
	const Model *m = CheckModel( handle, "TIKI_Anim_Random" );
	if ( !m ) return -1;
	/* Retail walks the sorted list from the first name the prefix matches and
	 * stops at the first that sorts past it, keeping at most sixteen whose next
	 * character is not an underscore, then rolls against their weights. */
	const std::size_t length = strlen( name );
	int candidates[16];
	float weights[16];
	int count = 0;
	float total = 0.0f;
	for ( std::size_t i = 0; i < m->animations.size() && count < 16; ++i ) {
		const Animation &a = m->animations[i];
		const int order = RetailCompareN( a.alias, name, static_cast<int>( length ) );
		if ( order > 0 ) break;
		if ( order < 0 ) continue;
		if ( a.alias[length] == '_' ) continue;
		candidates[count] = static_cast<int>( i );
		weights[count] = a.weight;
		total += a.weight;
		++count;
	}
	if ( !count ) return -1;
	float roll = static_cast<float>( rand() ) * total / 32767.0f;
	for ( int i = 0; i < count; ++i ) {
		if ( roll < weights[i] ) return candidates[i];
		roll -= weights[i];
	}
	return candidates[count - 1];
}

int AliceTiki_AnimNumFrames( int handle, int anim ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_NumFrames" );
	if ( !m ) return 0;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_NumFrames" );
	return a ? FileFrames( a->file ) : 0;
}

float AliceTiki_AnimTime( int handle, int anim ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_Time" );
	if ( !m ) return 0.0f;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_Time" );
	if ( !a || !a->file ) return 0.0f;
	return a->file->kind == File::Kind::Tan ? a->file->tan.totalTime : a->file->anim.totalTime;
}

void AliceTiki_AnimDelta( int handle, int anim, vec3_t delta ) {
	VectorClear( delta );
	const Model *m = CheckModel( handle, "TIKI_Anim_Delta" );
	if ( !m ) return;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_Delta" );
	if ( !a || !a->file ) return;
	const float *stored = a->file->kind == File::Kind::Tan ? a->file->tanDelta : a->file->anim.delta;
	VectorScale( stored, m->scale, delta );
}

void AliceTiki_AnimAbsoluteDelta( int handle, int anim, vec3_t delta ) {
	VectorClear( delta );
	const Model *m = CheckModel( handle, "TIKI_Anim_AbsoluteDelta" );
	if ( !m ) return;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_AbsoluteDelta" );
	if ( !a || !a->file ) return;
	for ( int f = 0; f < FileFrames( a->file ); ++f ) {
		vec3_t d;
		AliceTiki_FrameDelta( handle, anim, f, d );
		for ( int k = 0; k < 3; ++k ) delta[k] += std::fabs( d[k] );
	}
}

int AliceTiki_AnimFlags( int handle, int anim ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_Flags" );
	if ( !m ) return 0;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_Flags" );
	return a ? a->flags : 0;
}

int AliceTiki_AnimCrossblendTime( int handle, int anim ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_CrossblendTime" );
	if ( !m ) return 0;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_CrossblendTime" );
	return a ? a->crossblendMs : 0;
}

qboolean AliceTiki_AnimHasCommands( int handle, int anim, qboolean client ) {
	const Model *m = CheckModel( handle, "TIKI_Anim_HasCommands" );
	if ( !m ) return qfalse;
	const Animation *a = CheckAnim( m, anim, "TIKI_Anim_HasCommands" );
	if ( !a ) return qfalse;
	return ( client ? !a->client.empty() : !a->server.empty() ) ? qtrue : qfalse;
}

qboolean AliceTiki_FrameCommands( int handle, int anim, int frame, qboolean client,
	fntech3_alice_tiki_cmd_t *out ) {
	if ( !out ) return qfalse;
	out->num_cmds = 0;
	const Model *m = CheckModel( handle, "TIKI_Frame_Commands" );
	if ( !m ) return qfalse;
	const Animation *a = CheckAnim( m, anim, "TIKI_Frame_Commands" );
	if ( !a ) return qfalse;
	/* True only when a command was copied (0x0045d780 and 0x0045d8d0 both
	 * start from zero and set one per command), where InitCommands answers
	 * true with none (0x0045cf1a). */
	if ( !AliceTiki_FillCommands( client ? a->client : a->server, frame, false, out ) ) return qfalse;
	return out->num_cmds > 0 ? qtrue : qfalse;
}

void AliceTiki_FrameDelta( int handle, int anim, int frame, vec3_t delta ) {
	VectorClear( delta );
	const Model *m = CheckModel( handle, "TIKI_Frame_Delta" );
	if ( !m ) return;
	const Animation *a = CheckAnim( m, anim, "TIKI_Frame_Delta" );
	if ( !a || !a->file || frame < 0 || frame >= FileFrames( a->file ) ) {
		TikiError( "TIKI_Frame_Delta: Frame %d out of range for %s.\n", frame, m->name );
		return;
	}
	if ( a->file->kind == File::Kind::Tan ) {
		const pose::TanFrame tf = pose::ReadTanFrame( a->file->bytes.data() + a->file->tan.frameData
			+ pose::TanFrameBytes * frame );
		VectorScale( tf.delta, m->scale, delta );
	} else {
		pose::FrameDelta( a->file->anim, frame, delta );
		VectorScale( delta, m->scale, delta );
	}
}

float AliceTiki_FrameTime( int handle, int anim, int frame ) {
	const Model *m = CheckModel( handle, "TIKI_Frame_Time" );
	if ( !m ) return 0.0f;
	const Animation *a = CheckAnim( m, anim, "TIKI_Frame_Time" );
	if ( !a || !a->file ) return 0.0f;
	if ( a->file->kind == File::Kind::Tan ) {
		if ( frame < 0 || frame >= a->file->tan.frames ) {
			TikiError( "TIKI_Frame_Time: Frame %d out of range %s.\n", frame, m->name );
			return 0.0f;
		}
		return pose::ReadTanFrame( a->file->bytes.data() + a->file->tan.frameData
			+ pose::TanFrameBytes * frame ).frameTime;
	}
	/* A skeletal animation's frame time is its header's, with no range check. */
	return a->file->anim.frameTime;
}

void AliceTiki_FrameBounds( int handle, int anim, int frame, float scale, vec3_t mins, vec3_t maxs ) {
	const Model *m = CheckModel( handle, "TIKI_Frame_Bounds" );
	if ( !m ) return;
	const Animation *a = CheckAnim( m, anim, "TIKI_Frame_Bounds" );
	/* On an error retail leaves the caller's buffers as they were. */
	if ( !a || !a->file || frame < 0 || frame >= FileFrames( a->file ) ) return;
	float lo[3], hi[3];
	if ( a->file->kind == File::Kind::Tan ) {
		const pose::TanFrame tf = pose::ReadTanFrame( a->file->bytes.data() + a->file->tan.frameData
			+ pose::TanFrameBytes * frame );
		VectorCopy( tf.mins, lo );
		VectorCopy( tf.maxs, hi );
	} else {
		pose::FrameBounds( a->file->anim, frame, lo, hi );
	}
	for ( int k = 0; k < 3; ++k ) {
		mins[k] = lo[k] * m->scale * scale;
		maxs[k] = hi[k] * m->scale * scale;
	}
}

float AliceTiki_FrameRadius( int handle, int anim, int frame ) {
	const Model *m = CheckModel( handle, "TIKI_Frame_Radius" );
	if ( !m ) return 0.0f;
	const Animation *a = CheckAnim( m, anim, "TIKI_Frame_Radius" );
	if ( !a || !a->file || frame < 0 || frame >= FileFrames( a->file ) ) return 0.0f;
	if ( a->file->kind == File::Kind::Tan ) {
		return pose::ReadTanFrame( a->file->bytes.data() + a->file->tan.frameData
			+ pose::TanFrameBytes * frame ).radius * m->scale;
	}
	return pose::FrameRadius( a->file->anim, frame ) * m->scale;
}

int AliceTiki_SurfaceNameToNum( int handle, const char *name ) {
	const Model *m = CheckModel( handle, "TIKI_Surface_NameToNum" );
	if ( !m || !name ) return -1;
	for ( std::size_t i = 0; i < m->surfaces.size(); ++i ) {
		if ( !Q_stricmp( m->surfaces[i].name, name ) ) return static_cast<int>( i );
	}
	return -1;
}

const char *AliceTiki_SurfaceNumToName( int handle, int surface ) {
	const Model *m = CheckModel( handle, "TIKI_Surface_NumToName" );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return nullptr;
	return m->surfaces[surface].name;
}

int AliceTiki_SurfaceFlags( int handle, int surface ) {
	const Model *m = CheckModel( handle, "TIKI_Surface_Flags" );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return 0;
	return m->surfaces[surface].flags;
}

int AliceTiki_SurfaceNumSkins( int handle, int surface ) {
	const Model *m = CheckModel( handle, "TIKI_Surface_NumSkins" );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return 0;
	return m->surfaces[surface].numSkins;
}

float AliceTiki_SurfaceDamageMultiplier( int handle, int surface ) {
	const Model *m = CheckModel( handle, "TIKI_Surface_DamageMultiplier" );
	if ( !m || surface < 0 || surface >= static_cast<int>( m->surfaces.size() ) ) return 0.0f;
	return m->surfaces[surface].damage;
}

int AliceTiki_TagNumForName( int handle, const char *name ) {
	const Model *m = CheckModel( handle, "TIKI_Tag_NameToNum" );
	if ( !m || !name ) return -1;
	if ( m->skeleton ) {
		const pose::Skeleton &s = m->skeleton->skeleton;
		for ( int b = 0; b < s.bones; ++b ) {
			if ( !Q_stricmp( pose::BoneName( s, b ), name ) ) return b;
		}
		return -1;
	}
	if ( m->animations.empty() || !m->animations.front().file ) return -1;
	const File *f = m->animations.front().file;
	for ( int t = 0; t < f->tan.tags; ++t ) {
		if ( !Q_stricmp( reinterpret_cast<const char *>( f->bytes.data() + f->tan.tag[t].name ), name ) ) return t;
	}
	return -1;
}

const char *AliceTiki_TagNameForNum( int handle, int tag ) {
	const Model *m = CheckModel( handle, "TIKI_Tag_NumToName" );
	if ( !m || tag < 0 || tag >= m->numTags ) return nullptr;
	if ( m->skeleton ) return pose::BoneName( m->skeleton->skeleton, tag );
	if ( m->animations.empty() || !m->animations.front().file ) return nullptr;
	const File *f = m->animations.front().file;
	return tag < f->tan.tags ? reinterpret_cast<const char *>( f->bytes.data() + f->tan.tag[tag].name ) : nullptr;
}

void AliceTiki_TagOrientation( int handle, int anim, int frame, int tag, float scale,
	const fntech3_alice_bonectrl_t *controllers, float origin[3], float axis[3][3] ) {
	ZeroOrientation( origin, axis );
	const Model *m = CheckModel( handle, "TIKI_Tag_Orientation" );
	if ( !m || tag < 0 || tag >= m->numTags ) return;
	const Animation *a = CheckAnim( m, anim, "TIKI_Tag_Orientation" );
	if ( !a || !a->file ) return;
	if ( m->skeleton ) {
		const pose::SkeletalAnim *view = AnimView( *m, anim );
		if ( !view ) return;
		pose::Sample legs;
		legs.anim = view;
		legs.frame = frame;
		SkeletalTag( *m, legs, nullptr, controllers, tag, origin, axis );
		return;
	}
	TanTag( *m, a->file, frame, tag, scale, origin, axis );
}

void AliceTiki_TagOrientationEx( int handle, int anim, int frame, int tag, float scale,
	const fntech3_alice_bonectrl_t *controllers, int crossblendAnim, int crossblendFrame,
	float crossblendLerp, qboolean useLegs, qboolean useTorso, int torsoAnim, int torsoFrame,
	int torsoCrossblendAnim, int torsoCrossblendFrame, float torsoCrossblendLerp,
	float origin[3], float axis[3][3] ) {
	ZeroOrientation( origin, axis );
	const Model *m = CheckModel( handle, "TIKI_Tag_OrientationEx" );
	if ( !m || tag < 0 || tag >= m->numTags ) return;
	if ( !m->skeleton ) {
		const Animation *a = CheckAnim( m, anim, "TIKI_Tag_OrientationEx" );
		if ( a && a->file ) TanTag( *m, a->file, frame, tag, scale, origin, axis );
		return;
	}
	pose::Sample legs, torso;
	legs.anim = AnimView( *m, anim );
	legs.frame = frame;
	legs.crossAnim = AnimView( *m, crossblendAnim );
	legs.crossFrame = crossblendFrame;
	legs.crossWeight = legs.crossAnim ? crossblendLerp : -1.0f;
	torso.anim = AnimView( *m, torsoAnim );
	torso.frame = torsoFrame;
	torso.crossAnim = AnimView( *m, torsoCrossblendAnim );
	torso.crossFrame = torsoCrossblendFrame;
	torso.crossWeight = torso.crossAnim ? torsoCrossblendLerp : -1.0f;
	const bool legsOn = useLegs && legs.anim, torsoOn = useTorso && torso.anim;
	if ( !legsOn && !torsoOn ) return;
	float o[3], ax[3][3];
	if ( legsOn && torsoOn ) {
		SkeletalTag( *m, legs, &torso, controllers, tag, o, ax );
	} else {
		SkeletalTag( *m, legsOn ? legs : torso, nullptr, controllers, tag, o, ax );
	}
	/* This one does use its scale argument, which the plain query ignores. */
	for ( int k = 0; k < 3; ++k ) origin[k] = o[k] * scale;
	std::memcpy( axis, ax, sizeof( ax ) );
}

void AliceTiki_TagLerpedOrientation( int handle, const fntech3_alice_refEntity_t *e,
	int tag, float origin[3], float axis[3][3] ) {
	IdentityOrientation( origin, axis );
	const Model *m = CheckModel( handle, "TIKI_Tag_LerpedOrientation" );
	if ( !m || !e || tag < 0 || tag >= m->numTags ) return;
	if ( !m->skeleton ) {
		const Animation *a = CheckAnim( m, e->anim, "TIKI_Tag_LerpOrientation" );
		if ( !a || !a->file ) return;
		float o[3], ax[3][3], oo[3], oax[3][3];
		TanTag( *m, a->file, e->frame, tag, e->scale, o, ax );
		const Animation *old = CheckAnim( m, e->oldanim, "TIKI_Tag_LerpOrientation" );
		if ( old && old->file && e->backlerp != 0.0f ) {
			TanTag( *m, old->file, e->oldframe, tag, e->scale, oo, oax );
			const float front = 1.0f - e->backlerp;
			for ( int k = 0; k < 3; ++k ) origin[k] = o[k] * front + oo[k] * e->backlerp;
			for ( int i = 0; i < 3; ++i ) {
				for ( int k = 0; k < 3; ++k ) axis[i][k] = ax[i][k] * front + oax[i][k] * e->backlerp;
				VectorNormalize( axis[i] );
			}
		} else {
			VectorCopy( o, origin );
			std::memcpy( axis, ax, sizeof( ax ) );
		}
		return;
	}
	pose::Sample legs, torso;
	legs.anim = AnimView( *m, e->anim );
	legs.frame = e->frame;
	legs.oldAnim = AnimView( *m, e->oldanim );
	legs.oldFrame = e->oldframe;
	legs.backlerp = e->backlerp;
	legs.crossAnim = AnimView( *m, e->crossblend_anim );
	legs.crossFrame = e->crossblend_frame;
	legs.crossWeight = legs.crossAnim ? e->crossblend_lerp : -1.0f;
	torso.anim = AnimView( *m, e->torso_anim );
	torso.frame = e->torso_frame;
	torso.oldAnim = AnimView( *m, e->torso_oldanim );
	torso.oldFrame = e->torso_oldframe;
	torso.backlerp = e->torso_backlerp;
	torso.crossAnim = AnimView( *m, e->torso_crossblend_anim );
	torso.crossFrame = e->torso_crossblend_frame;
	torso.crossWeight = torso.crossAnim ? e->torso_crossblend_lerp : -1.0f;
	const bool legsOn = e->uselegs && legs.anim, torsoOn = e->usetorso && torso.anim;
	if ( !legsOn && !torsoOn ) return;
	float o[3], ax[3][3];
	if ( legsOn && torsoOn ) {
		SkeletalTag( *m, legs, &torso, e->bonectrl, tag, o, ax );
	} else {
		SkeletalTag( *m, legsOn ? legs : torso, nullptr, e->bonectrl, tag, o, ax );
	}
	for ( int k = 0; k < 3; ++k ) origin[k] = o[k] * e->scale;
	std::memcpy( axis, ax, sizeof( ax ) );
}

/* ---- aliases ------------------------------------------------------------ */

/* See alice_alias.cpp for what each list routine does. */

qboolean AliceAlias_Add( int handle, const char *name, const char *realName, const char *parameters ) {
	alias::List *list = Aliases( handle, true );
	return list ? alias::Add( list, name, realName, parameters ) : qfalse;
}

const char *AliceAlias_FindRandom( int handle, const char *name ) {
	alias::List *list = Aliases( handle, false );
	return list ? alias::FindRandom( list, name ) : nullptr;
}

void AliceAlias_Dump( int handle ) {
	alias::Dump( Aliases( handle, false ) );
}

void AliceAlias_Clear( int handle ) {
	alias::ClearList( Aliases( handle, false ) );
}

/* The model-only three answer a negative handle silently, as the game's
 * wrappers do before reaching TIKI_GetTiki: the game module asks for all 512
 * model indices' lists when it saves. */

const char *AliceAlias_FindDialog( int handle, const char *name, int random, int entityNumber ) {
	alias::List *list = handle < 0 ? nullptr : ModelAliases( handle, false );
	return list ? alias::FindDialog( list, name, random, entityNumber ) : nullptr;
}

void AliceAlias_UpdateDialog( int handle, const char *name, int actor, int timesPlayed,
	int played, int lastPlayed ) {
	alias::List *list = handle < 0 ? nullptr : ModelAliases( handle, false );
	if ( !list ) return;
	if ( actor == -1 ) {
		alias::UpdateDialog( list, name, timesPlayed, played, lastPlayed );
	} else {
		alias::UpdateActorDialog( list, name, actor, timesPlayed, played, lastPlayed );
	}
}

fntech3::alice::alias::List *AliceAlias_GetList( int handle ) {
	return handle < 0 ? nullptr : ModelAliases( handle, false );
}
