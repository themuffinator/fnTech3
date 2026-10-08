"""Elite Force SP's savegames: saves/<name>.sav, and the commands around them.

Retail stvoy.exe writes a save as a run of chunks (SG_Append 0x427a40): each
one's id, its data's length and the data's checksum, the data and the magic
0x1234ABCD. The engine writes a comment, a picture of 256 pixels square, the
map, the userinfo cvars and what the player carries, and for a full save the
level's time, its area portals and its configstrings; the game's WriteLevel
follows. Reading one spawns the map between the engine's chunks and the
game's (SG_ReadSavegame 0x427750). The checks pin the framing, the pictures,
the order of both halves, the commands (map, devmap, maptransition,
loadtransition, save, load and wipe) and the autosave a level is entered
with.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


PROBE = r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "code/compat/stvef_sp_savegame.hpp"

typedef unsigned char byte;
#define ID_INLINE inline
#include "code/renderercommon/tr_stvef_thumbnail.h"

namespace save = fntech3::stvef::savegame;

// A checksum with no meaning beyond being the one both sides agree on.
static std::uint32_t Sum( const unsigned char *bytes, std::uint32_t length ) {
	std::uint32_t value = 0x1234u;
	for ( std::uint32_t i = 0; i < length; ++i ) value = value * 31u + bytes[i];
	return value;
}

static std::size_t Frame( std::vector<unsigned char> &bytes, std::uint32_t id,
	const std::string &data, std::uint32_t checksum, std::uint32_t magic ) {
	unsigned char header[12], trailer[4];
	save::PutFrameHeader( id, static_cast<std::uint32_t>( data.size() ), checksum, header );
	save::PutWord( trailer, magic );
	const std::size_t offset = bytes.size();
	bytes.insert( bytes.end(), header, header + sizeof( header ) );
	bytes.insert( bytes.end(), data.begin(), data.end() );
	bytes.insert( bytes.end(), trailer, trailer + sizeof( trailer ) );
	return offset;
}

static std::size_t Frame( std::vector<unsigned char> &bytes, std::uint32_t id, const std::string &data ) {
	return Frame( bytes, id, data,
		Sum( reinterpret_cast<const unsigned char *>( data.data() ), static_cast<std::uint32_t>( data.size() ) ),
		save::Magic );
}

static save::ReadResult Read( const std::vector<unsigned char> &bytes, std::size_t offset,
	std::uint32_t id, std::uint32_t length, save::Chunk &chunk ) {
	return save::ReadChunk( bytes.data(), bytes.size(), offset, id, length, chunk, Sum );
}

static void Framing() {
	assert( save::Comment == 0x434f4d4du );
	assert( save::Screenshot == 0x53484f54u );
	assert( save::MapName == 0x4d50434du );
	assert( save::Magic == 0x1234abcdu );
	assert( save::FrameBytes == 16 );

	char name[5];
	save::Name( save::ConfigstringData, name );
	assert( std::string( name ) == "CSDA" );

	std::vector<unsigned char> bytes;
	const std::size_t first = Frame( bytes, save::Comment, std::string( "a comment" ) );
	const std::size_t second = Frame( bytes, save::MapName, std::string( "borg1" ) );
	assert( bytes.size() == 9 + 5 + 32 );

	save::Chunk chunk{};
	assert( Read( bytes, first, save::Comment, 9, chunk ) == save::ReadResult::Ok );
	assert( chunk.length == 9 && std::memcmp( chunk.data, "a comment", 9 ) == 0 );
	assert( chunk.next == second );
	// Any length will do for a caller that does not care (retail's zero).
	assert( Read( bytes, first, save::Comment, 0, chunk ) == save::ReadResult::Ok );
	assert( Read( bytes, second, save::MapName, 5, chunk ) == save::ReadResult::Ok );
	assert( chunk.next == bytes.size() );

	// The end of a save answers with no chunk, so an optional read ends there.
	assert( Read( bytes, bytes.size(), save::MapName, 0, chunk ) == save::ReadResult::WrongId );
	assert( chunk.id == 0 && chunk.next == bytes.size() );

	// Another chunk, the wrong length, and every way a frame can be damaged.
	assert( Read( bytes, first, save::MapName, 0, chunk ) == save::ReadResult::WrongId );
	assert( chunk.id == save::Comment );
	assert( Read( bytes, first, save::Comment, 8, chunk ) == save::ReadResult::WrongLength );

	std::vector<unsigned char> damaged = bytes;
	damaged[first + 12] = 'A';
	assert( Read( damaged, first, save::Comment, 9, chunk ) == save::ReadResult::BadChecksum );
	damaged = bytes;
	damaged[first + 12 + 9] ^= 0xff;
	assert( Read( damaged, first, save::Comment, 9, chunk ) == save::ReadResult::BadMagic );
	damaged = bytes;
	damaged.resize( first + 20 );
	assert( Read( damaged, first, save::Comment, 9, chunk ) == save::ReadResult::Truncated );
	// A length that runs past the end of the save is refused before any read.
	damaged = bytes;
	Frame( damaged, save::Portals, std::string(), 0u, save::Magic );
	save::PutWord( &damaged[bytes.size() + 4], 0x7fffffffu );
	assert( Read( damaged, bytes.size(), save::Portals, 0, chunk ) == save::ReadResult::Truncated );
	// A frame whose header is cut short reads as no chunk at all.
	damaged = bytes;
	damaged.resize( first + 3 );
	assert( Read( damaged, first, save::Comment, 0, chunk ) == save::ReadResult::WrongId );
}

static void Names() {
	assert( save::NameAllowed( "quik" ) );
	assert( save::NameAllowed( "hub/tour/deck01" ) == false );
	assert( save::NameAllowed( "../q3config" ) == false );
	assert( save::NameAllowed( "sneak\\path" ) == false );
	assert( save::NameAllowed( nullptr ) == false );

	char comment[save::CommentBytes];
	save::AutosaveComment( "voy1", comment );
	assert( std::string( comment ) == "-------> voy1" );
	save::DatedComment( 9, 5, 3, 7, 4, 26, "borg2", comment );
	assert( std::string( comment ) == "09:05:03  07/04/26    borg2" );
	// A comment is kept within its 128 bytes whatever the level is called.
	save::DatedComment( 23, 59, 59, 12, 31, 99, std::string( 400, 'm' ).c_str(), comment );
	assert( std::strlen( comment ) == save::CommentBytes - 1 );

	// From byte 64 the chunk holds the line the menus list (stvoy.exe
	// 0x427086..0x427115), and every byte after the two strings is zero.
	char chunk[save::CommentBytes]{};
	save::AutosaveComment( "voy1", chunk );
	save::CommentInfo( 9, 5, 3, 7, 4, 26, "voy1", chunk );
	assert( save::CommentInfoOffset == 64 );
	assert( std::string( chunk ) == "-------> voy1" );
	assert( std::string( chunk + 64 ) == "09:05:03 07/04/26   voy1" );
	for ( std::size_t i = 13; i < 64; ++i ) assert( chunk[i] == 0 );
	for ( std::size_t i = 64 + 24; i < save::CommentBytes; ++i ) assert( chunk[i] == 0 );
	// The user's own retail save of scav5 holds its two lines the same way.
	char retail[save::CommentBytes]{};
	save::DatedComment( 16, 58, 59, 7, 15, 20, "scav5", retail );
	save::CommentInfo( 16, 58, 59, 7, 15, 20, "scav5", retail );
	assert( std::string( retail ) == "16:58:59  07/15/20    scav5" );
	assert( std::string( retail + 64 ) == "16:58:59 07/15/20   scav5" );
	// The line is cut to 63 characters, and the chunk's last byte stays clear.
	char longer[save::CommentBytes]{};
	save::CommentInfo( 23, 59, 59, 12, 31, 99, std::string( 400, 'm' ).c_str(), longer );
	assert( std::strlen( longer + 64 ) == 63 && longer[save::CommentBytes - 1] == 0 );
	// A comment longer than 63 characters runs into the line, as in retail.
	char crowded[save::CommentBytes]{};
	std::memcpy( crowded, std::string( 100, 'c' ).c_str(), 100 );
	save::CommentInfo( 1, 2, 3, 4, 5, 6, "dn1", crowded );
	assert( std::string( crowded ) == std::string( 64, 'c' ) + "01:02:03 04/05/06   dn1" + std::string( 100 - 64 - 23, 'c' ) );
}

static void Pictures() {
	// Four pixels averaged into one, and the alpha made opaque.
	const unsigned char source[2 * 2 * 4] = {
		10, 20, 30, 0,   30, 40, 50, 0,
		50, 60, 70, 0,   70, 80, 90, 0 };
	unsigned char one[4] = { 0, 0, 0, 0 };
	save::Resample( source, 2, 2, one, 1, 1 );
	assert( one[0] == 40 && one[1] == 50 && one[2] == 60 && one[3] == 255 );

	// A picture smaller than the savegame's takes each pixel as it is.
	unsigned char larger[2 * 2 * 4];
	const unsigned char single[4] = { 7, 8, 9, 0 };
	save::Resample( single, 1, 1, larger, 2, 2 );
	for ( int i = 0; i < 4; ++i ) {
		assert( larger[i * 4 + 0] == 7 && larger[i * 4 + 1] == 8 && larger[i * 4 + 2] == 9 );
		assert( larger[i * 4 + 3] == 255 );
	}

	// Turning a picture over swaps its rows and keeps each one whole.
	unsigned char rows[3 * 1 * 4] = { 1, 1, 1, 1,  2, 2, 2, 2,  3, 3, 3, 3 };
	save::FlipRows( rows, 1, 3 );
	assert( rows[0] == 3 && rows[4] == 2 && rows[8] == 1 );
}

static void Thumbnail() {
	// Eight by six pixels of screen become two by two: each of them averages
	// four across by three down, which here all hold one value.
	const int width = 8, height = 6, padlen = 3, side = 2;
	const int stride = width * 3 + padlen;
	std::vector<unsigned char> screen( static_cast<std::size_t>( stride ) * height, 0xee );
	for ( int y = 0; y < height; ++y ) {
		for ( int x = 0; x < width; ++x ) {
			unsigned char *pixel = &screen[static_cast<std::size_t>( y ) * stride + x * 3];
			const unsigned char value = static_cast<unsigned char>( 10 * ( y / 3 ) + ( x / 4 ) );
			pixel[0] = value;
			pixel[1] = static_cast<unsigned char>( value + 100 );
			pixel[2] = static_cast<unsigned char>( value + 200 );
		}
	}
	unsigned char picture[2 * 2 * 4] = {};
	R_STVEFThumbnail( screen.data(), width, height, padlen, picture, side );
	for ( int y = 0; y < side; ++y ) {
		for ( int x = 0; x < side; ++x ) {
			const unsigned char *pixel = &picture[( y * side + x ) * 4];
			const unsigned char value = static_cast<unsigned char>( 10 * y + x );
			// The row order is the screen's, its bottom row first.
			assert( pixel[0] == value );
			assert( pixel[1] == value + 100 );
			assert( pixel[2] == value + 200 );
			assert( pixel[3] == 255 );
		}
	}
}

int main() {
	Framing();
	Names();
	Pictures();
	Thumbnail();
	return 0;
}
'''

RETAIL_MESSAGES = (
    '"Attempting write of chunk %s length %d\\n"',
    '"^1Failed to write %s chunk\\n"',
    '"Attempting read of chunk %s length %d\\n"',
    '"Loaded chunk ID (%s) does not match requested chunk ID (%s)"',
    '"Loaded chunk (%s) has different length than requested"',
    '"Failed checksum check for chunk"',
    '"Bad savegame magic for chunk %s"',
    '"Error during loading chunk %s"',
    '"Failed to open savegame file %s\\n"',
    '"^1Failed to create savegame\\n"',
    '"^1Failed to create new savegame file \\"%s\\"\\n"',
    '"^1Error during savegame-rename. Check \\"%s\\" for write-protect!\\n"',
    '"^1Failed to open savegame \\"%s\\"\\n"',
    '"Saving: current (%s)\\n"',
    '"Reading: %s\\n"',
    '"Reading %d configstrings...\\n"',
    '"Cfg str %d = %s\\n"',
    '"^1Server is not running\\n"',
    '"^1You must be in a game to save.\\n"',
    '"^1Cannot save on holodeck or brig.\\n"',
    '"USAGE: \\"save <filename>\\"\\n"',
    '"^1\\nCan\'t savegame while dead!\\n"',
    '"^1Can\'t save to \'current\'\\n"',
    '"^1Bad savegame name.\\n"',
    '"^5Saving game \\"%s\\"...\\n"',
    '"^5Done.\\n"',
    '"USAGE: load <filename>\\n"',
    '"^1Bad loadgame name.\\n"',
    '"^1Can\'t load from \\"current\\"\\n"',
    '"^5Loading game \\"%s\\"...\\n"',
    '"^5Restoring game \\"%s\\"...\\n"',
    '"^1USAGE: wipe <name>\\n"',
    '"^1Can\'t wipe \'auto\' or \'exitholodeck\'\\n"',
    '"Ok\\n"',
    '"Can\'t have mapnames with a \\\\\\n"',
    '"Can\'t find map %s\\n"',
)


class STVEFSPSavegameTests(unittest.TestCase):
    def setUp(self):
        self.save = read('code/server/server_stvef_sp_save.hpp')

    def test_chunks_pictures_and_comments(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-sp-save-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text('#include <vector>\n' + PROBE, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=180, cwd=ROOT)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_retail_messages_are_kept(self):
        text = self.save
        for message in RETAIL_MESSAGES:
            self.assertIn(message, text, message)

    def test_a_save_is_written_in_retail_order(self):
        write = function(self.save, 'qboolean SV_STVEFSPWriteSavegame(')
        # Only a full save is refused where the game says saving is not allowed.
        self.assertIn('if ( !autosave && !SV_STVEFSPGameAllowedToSaveHere( qfalse ) ) {', write)
        order = ['SV_STVEFSPCreateSave( "current" )', 'SV_STVEFSPWriteComment( autosave, map )',
                 'SV_STVEFSPWriteScreenshot( autosave, map )', 'savegame::MapName, map', 'SV_STVEFSPWriteCvars()',
                 'SV_STVEFSPWriteGame( autosave )', 'savegame::Time', 'savegame::TimeResidual',
                 'SV_STVEFSPWritePortals()', 'SV_STVEFSPWriteConfigstrings()',
                 'stvefSpGame->WriteLevel( autosave )', 'SV_STVEFSPCloseWrite()',
                 'SV_STVEFSPMoveSave( "current", name )']
        self.assertEqual(sorted(order, key=write.index), order)
        # The level's own time, portals and configstrings belong to a full save.
        level = write[write.index('if ( !autosave ) {'):write.index('SV_STVEFSPWriteConfigstrings()')]
        for chunk in ('savegame::Time', 'savegame::TimeResidual', 'SV_STVEFSPWritePortals()'):
            self.assertIn(chunk, level)

    def test_a_comment_chunk_is_cleared_and_carries_the_menus_line(self):
        comment = function(self.save, 'void SV_STVEFSPWriteComment( qboolean autosave, const char *map ) {')
        # No byte of the chunk is left as the stack had it.
        self.assertIn('char comment[savegame::CommentBytes]{};', comment)
        order = ['savegame::AutosaveComment( map, comment )', 'Q_strncpyz( comment, stvefSpSaveComment',
                 'savegame::DatedComment(', 'savegame::CommentInfo(',
                 'SV_STVEFSPAppendChunk( savegame::Comment, comment, sizeof( comment ) )']
        self.assertEqual(sorted(order, key=comment.index), order)

    def test_a_save_is_read_with_the_map_spawned_in_the_middle(self):
        load = function(self.save, 'qboolean SV_STVEFSPReadSavegame( const char *name ) {')
        order = ['savegame::Comment', 'savegame::Screenshot', 'CL_STVEFSPSetScreenshot',
                 'savegame::MapName', 'SV_STVEFSPReadCvars( reader )', 'SV_STVEFSPReadGame( reader )',
                 'stvefSpSavedGameJustLoaded = autosave ? FNTECH3_STVEF_SP_SAVED_AUTO : FNTECH3_STVEF_SP_SAVED_FULL;',
                 'SV_SpawnServer( map, qfalse )', 'savegame::Time', 'savegame::TimeResidual',
                 'SV_STVEFSPReadPortals( reader )', 'SV_STVEFSPReadConfigstrings( reader )',
                 'stvefSpGame->ReadLevel( autosave, stvefSpLoadTransition )']
        self.assertEqual(sorted(order, key=load.index), order)
        self.assertGreater(load.rindex('SV_STVEFSPCloseRead( reader )'),
                           load.index('stvefSpGame->ReadLevel( autosave, stvefSpLoadTransition )'))
        # A save that names no map, or one outside the maps, spawns nothing.
        self.assertIn('if ( !map[0] || std::strstr( map, ".." ) || std::strchr( map, \'\\\\\' ) ) {', load)
        self.assertLess(load.index('std::strstr( map, ".." )'), load.index('SV_SpawnServer'))

    def test_the_level_is_read_back_whole(self):
        # The engine's own state: the time, the area portals and every
        # configstring but the system info, which belongs to this server.
        configstrings = function(self.save, 'void SV_STVEFSPWriteConfigstrings() {')
        self.assertIn('i != savegame::SkippedConfigstring && sv.configstrings[i] && sv.configstrings[i][0]',
                      configstrings)
        back = function(self.save, 'void SV_STVEFSPReadConfigstrings( STVEFSPSaveReader &reader ) {')
        self.assertIn('if ( i != savegame::SkippedConfigstring ) {\n\t\t\tSV_SetConfigstring( i, "" );', back)
        # A save cannot reach past the configstrings the profile has.
        self.assertIn('if ( index < 0 || index >= limit ) {', back)
        self.assertIn('Com_Error( ERR_DROP, "Elite Force SP savegame holds configstring %i", index );', back)
        portals = function(self.save, 'void SV_STVEFSPReadPortals( STVEFSPSaveReader &reader ) {')
        self.assertIn('CM_FloodAreaConnections();', portals)
        self.assertLess(portals.index('SV_STVEFSPReadChunk'), portals.index('CM_FloodAreaConnections'))
        self.assertIn('int *CM_AreaPortalCounts( int *count ) {', read('code/qcommon/cm_test.c'))

    def test_what_the_player_carries(self):
        player = function(self.save, 'void SV_STVEFSPPlayerEndOfLevelSave() {')
        self.assertIn('"%i %i %i %i %i %f %f %f"', player)
        fields = ['player.stats[FNTECH3_STVEF_SP_STAT_HEALTH]', 'player.stats[FNTECH3_STVEF_SP_STAT_ARMOR]',
                  'player.stats[FNTECH3_STVEF_SP_STAT_WEAPONS]', 'player.weapon', 'player.weaponstate',
                  'player.viewangles[0], player.viewangles[1], player.viewangles[2]']
        self.assertEqual(sorted(fields, key=player.index), fields)
        self.assertIn('"playerammo%d"', player)
        self.assertIn('"borgadapt%d"', player)
        # Only a save made as a level begins carries them (WriteGame 0x427580).
        game = function(self.save, 'void SV_STVEFSPWriteGame( qboolean autosave ) {')
        self.assertLess(game.index('savegame::Autosave'), game.index('SV_STVEFSPPlayerEndOfLevelSave()'))
        self.assertIn('if ( !flag ) {\n\t\treturn;\n\t}', game)
        for chunk in ('savegame::PlayerSave', 'savegame::PlayerAmmo', 'savegame::BorgAdapt'):
            self.assertIn(chunk, game)

    def test_the_picture_a_save_keeps(self):
        picture = function(self.save, 'void SV_STVEFSPWriteScreenshot( qboolean autosave, const char *map ) {')
        # A level beginning keeps its own picture where it has one.
        self.assertIn('if ( autosave && CL_STVEFSPLevelshot( map, pixels ) ) {', picture)
        self.assertIn('CL_STVEFSPScreenshotPixels()', picture)
        screen = read('code/client/client_stvef_sp_screen.hpp')
        precache = function(screen, 'void CL_STVEFSPPrecacheScreenshot( void ) {')
        # Retail takes the picture only while a level is played with no menu up.
        self.assertIn('cls.state != CA_ACTIVE || Key_GetCatcher() != 0', precache)
        self.assertIn('S_ClearSoundBuffer();', precache)
        order = ['re.CaptureSTVEFThumbnail( screenshot.rgba, FNTECH3_STVEF_SP_SCREENSHOT_SIZE, &taken );',
                 'SCR_UpdateScreen();', 're.CaptureSTVEFThumbnail( nullptr, 0, nullptr );',
                 'if ( taken ) {']
        self.assertEqual(sorted(order, key=precache.index), order)
        levelshot = function(screen, 'qboolean CL_STVEFSPLevelshot( const char *map, byte *pixels ) {')
        self.assertIn('re.LoadImagePixels(', levelshot)
        self.assertIn('fntech3::stvef::savegame::Resample(', levelshot)
        self.assertIn('fntech3::stvef::savegame::FlipRows( pixels', levelshot)
        self.assertIn('"levelshots/%s.tga"', screen)
        set_picture = function(screen, 'void CL_STVEFSPSetScreenshot( const byte *pixels, int width, int height ) {')
        self.assertIn('width != FNTECH3_STVEF_SP_SCREENSHOT_SIZE || height != FNTECH3_STVEF_SP_SCREENSHOT_SIZE',
                      set_picture)

    def test_the_renderers_take_the_picture_when_the_frame_is_drawn(self):
        self.assertIn('void (*CaptureSTVEFThumbnail)( byte *rgba, int side, qboolean *done );',
                      read('code/renderercommon/tr_public.h'))
        self.assertIn('byte *(*LoadImagePixels)( const char *name, int *width, int *height );',
                      read('code/renderercommon/tr_public.h'))
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            backend = read(f'code/{renderer}/tr_backend.c')
            self.assertIn('backEnd.stvefThumbnail )\n\t\t&& ', backend)
            self.assertIn('if ( backEnd.stvefThumbnail ) {\n\t\t\tRB_TakeSTVEFThumbnail();', backend)
            # The backend still runs for a picture that has been asked for.
            self.assertIn('!backEnd.stvefThumbnail', read(f'code/{renderer}/tr_cmds.c'))
            init = read(f'code/{renderer}/tr_init.c')
            take = function(init, 'void RB_TakeSTVEFThumbnail( void ) {')
            self.assertIn('backEnd.stvefThumbnail = NULL;', take)
            self.assertIn('R_STVEFThumbnail( source, gls.captureWidth, gls.captureHeight, padlen, rgba, side );',
                          take)
            self.assertIn('R_GammaCorrect( rgba + i * 4, 3 );', take)
            self.assertIn('*backEnd.stvefThumbnailDone = qtrue;', take)
            self.assertIn('re.CaptureSTVEFThumbnail = RE_CaptureSTVEFThumbnail;', init)
            self.assertIn('re.LoadImagePixels = R_LoadImagePixels;', init)
            self.assertIn('#include "../renderercommon/tr_stvef_thumbnail.h"', init)
        for renderer in ('renderervk', 'rendererrtx'):
            # Vulkan copies the frame for reading only where one is wanted.
            self.assertIn('|| backEnd.stvefThumbnail )\n\t\t\t&& vk.capture.image )', read(f'code/{renderer}/vk.c'))

    def test_the_commands_are_retails(self):
        commands = function(self.save, 'void SV_STVEFSPAddCommands( void ) {')
        for name in ('map', 'devmap', 'maptransition', 'loadtransition', 'save', 'load', 'wipe'):
            self.assertIn(f'Cmd_AddCommand( "{name}", SV_STVEFSP', commands)
        self.assertIn('stvefSpTestSave = Cvar_Get( "sv_testsave", "0", 0 );', commands)
        ccmds = read('code/server/sv_ccmds.cpp')
        # Single player has no spmap, spdevmap or Quake III map command.
        operator_commands = function(ccmds, 'void SV_AddOperatorCommands( void ) {')
        single = operator_commands[operator_commands.index('if ( Com_GameIsSTVEFSP() ) {'):]
        self.assertIn('SV_STVEFSPAddCommands();', single)
        self.assertLess(single.index('SV_STVEFSPAddCommands();'), single.index('return;'))
        self.assertLess(operator_commands.index('if ( Com_GameIsSTVEFSP() ) {'),
                        operator_commands.index('SV_AddCommandSet( mapCommands );'))

    def test_a_level_starts_as_retail_starts_it(self):
        start = function(self.save, 'qboolean SV_STVEFSPStartMap( const char *map ) {')
        order = ['"maps/%s.bsp"', 'Can\'t find map %s', 'if ( map[0] != \'_\' ) {', 'SV_STVEFSPWipeSave( "auto" )',
                 'SV_STVEFSPPlayerModels();', 'SV_SpawnServer( map, qfalse );']
        self.assertEqual(sorted(order, key=start.index), order)
        models = function(self.save, 'void SV_STVEFSPPlayerModels() {')
        for model in ('"hazard/default"', '"hazardfemale/default"', '"munro/default"', '"alexandria/default"'):
            self.assertIn(model, models)
        plain = function(self.save, 'void SV_STVEFSPMap_f() {')
        self.assertIn('Cvar_Set( "playersave", "" );', plain)
        self.assertIn('Cvar_Set( "spawntarget", "" );', plain)
        self.assertIn('CL_STVEFSPUnprecacheScreenshot();', plain)
        self.assertIn('Cvar_Set( "sv_cheats", cheats ? "1" : "0" );', plain)

    def test_the_two_transitions(self):
        walk = function(self.save, 'void SV_STVEFSPMapTransition_f() {')
        order = ['stvefSpLoadTransition = 0;', 'CL_STVEFSPPrecacheScreenshot();',
                 'SV_STVEFSPPlayerEndOfLevelSave();', 'Cvar_Set( "spawntarget", target );',
                 'SV_STVEFSPStartMap( map );']
        self.assertEqual(sorted(order, key=walk.index), order)
        hub = function(self.save, 'void SV_STVEFSPLoadTransition_f() {')
        order = ['stvefSpLoadTransition = 1;', 'CL_STVEFSPPrecacheScreenshot();',
                 'SV_STVEFSPPlayerEndOfLevelSave();', '"hub/%s"',
                 'SV_STVEFSPWriteSavegame( name, qfalse, &writeFailed );', 'Cvar_Set( "spawntarget", target );',
                 'if ( !SV_STVEFSPReadHubSave( map ) ) {', 'SV_STVEFSPStartMap( map );']
        self.assertEqual(sorted(order, key=hub.index), order)
        restore = function(self.save, 'qboolean SV_STVEFSPReadHubSave( const char *map ) {')
        self.assertIn('"hub/%s"', restore)

    def test_the_autosave_a_level_is_entered_with(self):
        entered = function(self.save, 'void SV_STVEFSPEnteredWorld( void ) {')
        # A holodeck or the brig keeps the save of the level it was entered from.
        self.assertIn("if ( mapname[0] != '_' ) {", entered)
        hub = entered[entered.index('if ( stvefSpLoadTransition ) {'):entered.index('} else if')]
        self.assertIn('SV_STVEFSPWriteSavegame( name, qfalse );', hub)
        self.assertIn('SV_STVEFSPWriteSavegame( "auto", qfalse );', hub)
        self.assertIn('Cvar_VariableIntegerValue( "cg_virtualvoyager" )', hub)
        self.assertIn('SV_STVEFSPWriteSavegame( "virtual", qfalse );', hub)
        # A level begun afresh keeps an autosave of its beginning.
        self.assertIn('} else if ( !stvefSpSavedGameJustLoaded ) {\n\t\t\tSV_STVEFSPWriteSavegame( "auto", qtrue );',
                      entered)
        self.assertIn('stvefSpSavedGameJustLoaded = FNTECH3_STVEF_SP_SAVED_NO;', entered)
        self.assertIn('stvefSpLoadTransition = 0;', entered)
        client = read('code/server/sv_client.cpp')
        self.assertIn('SV_ClientEnterWorld( cl, &cmds[0] );\n\t\t// Elite Force SP saves the level the player has '
                      'just entered.\n\t\tif ( Com_GameIsSTVEFSP() ) {\n\t\t\tSV_STVEFSPEnteredWorld();', client)

    def test_the_save_command_follows_retails_checks(self):
        save = function(self.save, 'void SV_STVEFSPSave_f() {')
        order = ['"^1Server is not running\\n"', '"^1You must be in a game to save.\\n"',
                 '"^1Cannot save on holodeck or brig.\\n"', 'USAGE: \\"save <filename>\\"',
                 'SV_STVEFSPPlayerHealth() <= 0', '"^1Can\'t save to \'current\'\\n"',
                 'cg_virtualvoyager', 'CL_STVEFSPPrecacheScreenshot();', "name[4] == '*'",
                 'savegame::NameAllowed( name )', 'if ( SV_STVEFSPWriteSavegame( name, qfalse ) ) {']
        self.assertEqual(sorted(order, key=save.index), order)
        load = function(self.save, 'void SV_STVEFSPLoad_f() {')
        self.assertIn('stvefSpLoadTransition = 0;', load)
        respawn = load[load.index('if ( !Q_stricmp( name, "*respawn" ) ) {'):]
        # A respawn takes the level's own autosave, or the save last loaded
        # where that autosave belongs to another level (0x426a33).
        self.assertIn('Q_strncpyz( name, "auto", sizeof( name ) );', respawn)
        self.assertIn('SV_STVEFSPSaveMapName( "auto" )', respawn)
        self.assertIn('Q_strncpyz( name, stvefSpSaveLastRead, sizeof( name ) );', respawn)
        wipe = function(self.save, 'void SV_STVEFSPWipe_f() {')
        self.assertIn('Q_stricmp( name, "auto" ) || !Q_stricmp( name, "exitholodeck" )', wipe)

    def test_the_game_and_the_menus_reach_the_savegames(self):
        host = read('code/server/server_stvef_sp_game.hpp')
        self.assertIn('import.AppendToSaveGame = SV_STVEFSPAppendToSave;', host)
        self.assertIn('import.ReadFromSaveGame = SV_STVEFSPReadFromSave;', host)
        self.assertIn('import.ReadFromSaveGameOptional = SV_STVEFSPReadFromSaveOptional;', host)
        optional = function(self.save, 'std::int32_t SV_STVEFSPReadFromSaveOptional( std::uint32_t chunk,')
        self.assertIn('true );', optional)
        self.assertIn('#include "server_stvef_sp_save.hpp"', read('code/server/sv_game.cpp'))
        # A client that was already connected when a level spawns is connected
        # afresh, whatever a save being read has set (0x425bba).
        entry = function(host, 'intptr_t QDECL SV_STVEFSPGameEntry( int command, ... ) {')
        self.assertIn('arg[1] ? stvefSpSavedGameJustLoaded : FNTECH3_STVEF_SP_SAVED_NO', entry)
        ui = read('code/client/client_stvef_sp_ui.hpp')
        self.assertIn('return SV_STVEFSPSaveImage( name, pixels );', ui)
        self.assertIn('SV_STVEFSPSaveComment( name )', ui)
        self.assertIn('return SV_STVEFSPValidateSave( name );', ui)
        self.assertIn('void CL_STVEFSPUIStoreSaveComment( const char *comment ) { SV_STVEFSPStoreSaveComment( comment ); }',
                      ui)
        self.assertIn('void CL_STVEFSPUIPrecacheScreenshot() { CL_STVEFSPPrecacheScreenshot(); }', ui)
        # The menus' checks are quiet about a save they cannot read.
        validate = function(self.save, 'qboolean SV_STVEFSPValidateSave( const char *name ) {')
        self.assertIn('reader.checking = true;', validate)
        chunk = function(self.save, 'std::int32_t SV_STVEFSPReadChunk( STVEFSPSaveReader &reader, std::uint32_t id,')
        self.assertIn('if ( reader.checking || ( optional && result == savegame::ReadResult::WrongId ) ) {', chunk)

    def test_an_error_drops_the_savegame_being_written(self):
        common = read('code/qcommon/common.c')
        error = function(common, 'void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {')
        self.assertIn('CL_STVEFSPUnprecacheScreenshot();', error)
        self.assertIn('if ( code == ERR_DROP ) {\n\t\t\tSV_STVEFSPSaveAbort();', error)
        abort = function(self.save, 'void SV_STVEFSPSaveAbort( void ) {')
        self.assertIn('SV_STVEFSPCloseWrite();', abort)
        self.assertIn('SV_STVEFSPCloseRead( stvefSpSaveReader );', abort)
        self.assertIn('stvefSpSavedGameJustLoaded = FNTECH3_STVEF_SP_SAVED_NO;', abort)
        self.assertIn('if ( FS_Initialized() ) {\n\t\tSV_STVEFSPWipeSave( "current" );', abort)

    def test_sv_testsave_runs_the_games_saving_code_without_a_file(self):
        append = function(self.save, 'std::int32_t SV_STVEFSPAppendChunk( std::uint32_t id, const void *data,')
        self.assertIn('if ( !stvefSpSaveWriting && stvefSpTestSave && stvefSpTestSave->value != 0.0f ) {', append)
        test = function(self.save, 'void SV_STVEFSPTestSave( void ) {')
        self.assertIn('sv.state != SS_GAME', test)
        self.assertIn('SV_STVEFSPWriteGame( qfalse );', test)
        self.assertIn('stvefSpGame->WriteLevel( qfalse );', test)
        frame = function(read('code/server/sv_main.cpp'), 'void SV_Frame( int msec, int realMsec ) {')
        self.assertIn('SV_STVEFSPTestSave();', frame)
        self.assertLess(frame.index('SV_STVEFSPTestSave();'), frame.index('SV_CheckTimeouts();'))

    def test_the_engine_helpers_the_savegames_need(self):
        cvar = function(read('code/qcommon/cvar.c'), 'void Cvar_ForEachWithFlags( int flags,')
        self.assertIn('( cvar->flags & flags ) == flags', cvar)
        cvars = function(self.save, 'void SV_STVEFSPWriteCvars() {')
        self.assertIn('Cvar_ForEachWithFlags( CVAR_USERINFO, SV_STVEFSPCountCvar, &count );', cvars)
        self.assertLess(cvars.index('savegame::CvarCount'), cvars.index('SV_STVEFSPWriteCvar, nullptr'))
        rename = function(read('code/qcommon/files.c'), 'qboolean FS_HomeReplaceFile( const char *from, const char *to ) {')
        self.assertIn('FS_WriteQpathIsValid( from ) || !FS_WriteQpathIsValid( to )', rename)
        self.assertIn('FS_CreatePath( destination ) || !Sys_ReplaceFile( source, destination )', rename)
        # A save that cannot be put in place leaves the one that was there.
        move = function(self.save, 'qboolean SV_STVEFSPMoveSave( const char *from, const char *to ) {')
        self.assertIn('FS_HomeReplaceFile( SV_STVEFSPSavePath( from ), SV_STVEFSPSavePath( to ) )', move)


if __name__ == '__main__':
    unittest.main()
