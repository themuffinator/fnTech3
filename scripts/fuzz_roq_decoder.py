#!/usr/bin/env python3
"""Fuzz the shared RoQ decoder with seeded corruptions of installed movies.

The decoder bodies of code/client/cl_cin.cpp (chunk loop, codebook, VQ, quad
set-up and audio decoders, extracted as tests/roq_bounds_tests.py does) are
compiled with AddressSanitizer into a disposable harness. Each installed movie
is read from its pak in memory and sent to the harness over stdin; the harness
derives the variants itself, so no corrupted copy of a retail movie is written
to disk.

Variant n of a run with seed S draws from splitmix64 seeded with (S << 32) | n,
and n % 4 picks the corruption:

  payload    1-32 random bytes inside chunk payloads (framing stays valid)
  anywhere   1-32 random bytes anywhere, file and chunk headers included
  header     one chunk header's id, size, size by -16..16, or flags replaced
  structure  truncation, a chunk removed or duplicated, or up to 4 KiB zeroed

(n / 4) % 2 plays the variant looping, as `cinematic name 2` does. Each variant
is played the way CIN_PlayCinematic and CIN_RunCinematic drive the decoder,
until it is rejected, ends, or has looped twice. The harness fails on a
sanitizer report, a drawable frame outside the fixed frame buffers, a new frame
at neither frame buffer, an audio block larger than the decode buffer, or a
decoder pass that does not end within one call per eight bytes of input. The
unmodified movie must play to its end first.

On Windows run it from a Visual Studio developer environment for x86 to build
the shipping target with MSVC's AddressSanitizer; otherwise Clang is used.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
static long currentVariant = -1;
[[noreturn]] static void Fail( const char *what ) {
 std::printf( "FAIL variant=%ld %s\n", currentVariant, what ); std::fflush( stdout ); std::exit( 3 );
}
struct ScopedFileHandle {
 int handle = 0; int get() const { return handle; } void reset() { handle = 0; }
};
static std::vector<byte> stream;
static size_t position = 0;
static int FileRead( int handle, void *out, size_t bytes ) {
 if ( handle != 1 ) Fail( "read from a closed movie" );
 const size_t count = std::min( bytes, stream.size() - position );
 if ( count ) std::memcpy( out, stream.data() + position, count );
 position += count; return static_cast<int>( count );
}
static int OpenFileRead( const char *, ScopedFileHandle &file, qboolean ) {
 file.handle = 1; position = 0; return static_cast<int>( stream.size() );
}
static int CL_ScaledMilliseconds() { return 100; }
static char lastMessage[1024];
void QDECL Com_Printf( const char *format, ... ) {
 va_list args; va_start( args, format ); std::vsnprintf( lastMessage, sizeof( lastMessage ), format, args ); va_end( args );
}
void QDECL Com_DPrintf( const char *, ... ) {}
static void S_Update( int ) {}
static int s_rawend = 0, s_soundtime = 0;
static cvar_t volume{}; static cvar_t *s_volume = &volume;
static long audioBlocks = 0;
static void S_RawSamples( int count, int, int width, int channels, byte *, float ) {
 if ( count < 0 || static_cast<long long>( count ) * width * channels > 65536 ) Fail( "audio block exceeds the decode buffer" );
 ++audioBlocks;
}
static glconfig_t config{};
static struct { const glconfig_t *GetConfig() { return &config; } } re;
'''

DRIVER = r'''
struct Chunk { size_t offset; std::uint32_t size; };
static std::vector<byte> original;
static std::vector<Chunk> chunks;
static std::uint64_t rng;
static std::uint64_t Next() {
 std::uint64_t z = ( rng += 0x9E3779B97F4A7C15ull );
 z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ull;
 z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBull;
 return z ^ ( z >> 31 );
}
static size_t Below( size_t n ) { return n ? static_cast<size_t>( Next() % n ) : 0; }
static void ParseChunks() {
 for ( size_t at = 8; at + 8 <= original.size(); ) {
  const std::uint32_t size = original[at + 2] | ( original[at + 3] << 8 ) | ( original[at + 4] << 16 ) |
   ( static_cast<std::uint32_t>( original[at + 5] ) << 24 );
  chunks.push_back( { at, size } );
  if ( size > original.size() - at - 8 ) break;
  at += 8 + size;
 }
}
static void PutSize( byte *header, std::uint32_t size ) {
 for ( int i = 0; i < 4; ++i ) header[2 + i] = static_cast<byte>( size >> ( 8 * i ) );
}
static const unsigned kIds[] = { ROQ_QUAD_INFO, ROQ_CODEBOOK, ROQ_QUAD_VQ, ROQ_QUAD_JPEG, ROQ_QUAD_HANG,
 ROQ_PACKET, ZA_SOUND_MONO, ZA_SOUND_STEREO, 0x1084, 0 };
static const char *Mutate( std::vector<byte> &v, unsigned kind ) {
 if ( kind == 0 ) {
  for ( size_t i = 0, n = 1 + Below( 32 ); i < n; ++i ) {
   const Chunk &c = chunks[Below( chunks.size() )];
   if ( c.size && c.offset + 8 + c.size <= v.size() ) v[c.offset + 8 + Below( c.size )] = static_cast<byte>( Next() );
  }
  return "payload";
 }
 if ( kind == 1 ) {
  for ( size_t i = 0, n = 1 + Below( 32 ); i < n; ++i ) v[Below( v.size() )] = static_cast<byte>( Next() );
  return "anywhere";
 }
 const Chunk &c = chunks[Below( chunks.size() )];
 const size_t end = std::min( v.size(), c.offset + 8 + static_cast<size_t>( c.size ) );
 if ( kind == 2 ) {
  byte *header = v.data() + c.offset;
  switch ( Below( 4 ) ) {
  case 0: {
   const unsigned id = Below( 4 ) ? kIds[Below( sizeof( kIds ) / sizeof( kIds[0] ) )] : static_cast<unsigned>( Next() & 0xffff );
   header[0] = static_cast<byte>( id ); header[1] = static_cast<byte>( id >> 8 );
   return "header-id";
  }
  case 1: PutSize( header, static_cast<std::uint32_t>( Next() ) ); return "header-size";
  case 2: PutSize( header, c.size + static_cast<std::uint32_t>( static_cast<int>( Below( 33 ) ) - 16 ) ); return "header-size-delta";
  default: {
   const unsigned flags = static_cast<unsigned>( Next() & 0xffff );
   header[6] = static_cast<byte>( flags ); header[7] = static_cast<byte>( flags >> 8 );
   return "header-flags";
  }
  }
 }
 switch ( Below( 4 ) ) {
 case 0: v.resize( Below( v.size() ) ); return "truncated";
 case 1: v.erase( v.begin() + c.offset, v.begin() + end ); return "chunk-removed";
 case 2: { const std::vector<byte> copy( v.begin() + c.offset, v.begin() + end ); v.insert( v.begin() + end, copy.begin(), copy.end() ); return "chunk-duplicated"; }
 default: { const size_t at = Below( v.size() ); std::fill_n( v.begin() + at, std::min( v.size() - at, 1 + Below( 4096 ) ), byte( 0 ) ); return "zeroed"; }
 }
}
// The renderer reads drawX * drawY RGBA pixels from buf whenever a movie is drawn,
// including a stale frame held across a loop restart.
static void Check( cin_cache &movie, long &frames ) {
 const byte *base = cin.linbuf.data();
 if ( movie.buf ) {
  const long long offset = movie.buf - base, area = static_cast<long long>( movie.drawX ) * movie.drawY * 4;
  if ( offset < 0 || area < 0 || offset + area > static_cast<long long>( cin.linbuf.size() ) ) Fail( "drawn frame outside the frame buffers" );
 }
 if ( movie.dirty ) {
  if ( movie.buf != base && movie.buf != base + movie.screenDelta ) Fail( "new frame at neither frame buffer" );
  if ( static_cast<long long>( movie.drawX ) * movie.drawY * 4 > movie.screenDelta ) Fail( "drawn area exceeds the frame" );
  ++frames; movie.dirty = false;
 }
}
enum Outcome { HeaderRejected, Rejected, Ended, Looped };
static const char *const kOutcomes[] = { "headerRejected", "rejected", "ended", "looped" };
static Outcome Play( bool looping, long &frames, long &calls ) {
 lastMessage[0] = 0;
 std::memset( &cin, 0, sizeof( cin ) ); // as CIN_PlayCinematic resets it
 currentHandle = cin.currentHandle = 0;
 cin_cache &movie = cinTable[0];
 movie = cin_cache{};
 std::snprintf( movie.fileName.data(), movie.fileName.size(), "video/fuzz.roq" );
 movie.ROQSize = OpenFileRead( movie.fileName.data(), movie.iFile, qtrue );
 if ( movie.ROQSize < 16 ) return HeaderRejected;
 movie.CIN_WIDTH = DEFAULT_CIN_WIDTH; movie.CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
 movie.looping = looping;
 initRoQ();
 if ( FileRead( movie.iFile.get(), cin.file.data(), 16 ) != 16 ) return HeaderRejected;
 if ( ( cin.file[0] | ( cin.file[1] << 8 ) ) != 0x1084 || !RoQ_init() ) return HeaderRejected;
 movie.status = FMV_PLAY;
 const long bound = static_cast<long>( stream.size() / 8 ) + 16;
 long pass = 0; int loops = 0;
 while ( movie.status == FMV_PLAY ) {
  RoQInterrupt(); ++calls; ++pass;
  Check( movie, frames );
  if ( movie.status == FMV_LOOPED ) {
   if ( ++loops == 2 ) return Looped;
   movie.status = FMV_PLAY; pass = 0; // as CIN_RunCinematic resumes it
  }
  if ( pass > bound ) Fail( "a decoder pass does not end" );
 }
 if ( movie.status != FMV_EOF ) Fail( "unexpected decoder status" );
 return std::strncmp( lastMessage, "Invalid RoQ movie ", 18 ) == 0 ? Rejected : Ended;
}
static std::string Reason() {
 std::string text = lastMessage;
 const size_t at = text.find( ".roq: " );
 if ( at != std::string::npos ) text = text.substr( at + 6 );
 while ( !text.empty() && ( text.back() == '\n' || text.back() == '\r' ) ) text.pop_back();
 return text;
}
static void PrintMap( const std::map<std::string, long> &values ) {
 std::printf( "{" ); bool first = true;
 for ( const auto &entry : values ) { std::printf( "%s\"%s\": %ld", first ? "" : ", ", entry.first.c_str(), entry.second ); first = false; }
 std::printf( "}" );
}
int main( int argc, char **argv ) {
 if ( argc != 4 ) return 2;
 const unsigned long long seed = std::strtoull( argv[1], nullptr, 10 );
 const long count = std::strtol( argv[2], nullptr, 10 ), first = std::strtol( argv[3], nullptr, 10 );
#ifdef _WIN32
 _setmode( _fileno( stdin ), _O_BINARY );
#endif
 std::array<byte, 65536> block;
 for ( size_t n; ( n = std::fread( block.data(), 1, block.size(), stdin ) ) > 0; ) original.insert( original.end(), block.data(), block.data() + n );
 ParseChunks();
 if ( original.size() < 16 || chunks.empty() ) { std::printf( "FAIL not a RoQ movie\n" ); return 2; }
 config.maxTextureSize = 4096;
 stream = original;
 long baselineFrames = 0, baselineCalls = 0;
 std::fprintf( stderr, "variant -1\n" ); // the unmodified movie
 if ( Play( false, baselineFrames, baselineCalls ) != Ended ) Fail( "the unmodified movie does not play to its end" );
 const long baselineAudio = audioBlocks;
 std::map<std::string, long> outcomes, reasons, kinds;
 std::map<std::string, std::map<std::string, long>> byKind;
 long frames = 0, calls = 0;
 for ( long n = first; n < first + count; ++n ) {
  currentVariant = n;
  std::fprintf( stderr, "variant %ld\n", n ); // names the variant in a sanitizer report
  rng = ( static_cast<std::uint64_t>( seed ) << 32 ) | static_cast<std::uint32_t>( n );
  stream = original;
  const char *kind = Mutate( stream, static_cast<unsigned>( n % 4 ) );
  const Outcome outcome = Play( ( n / 4 ) % 2 != 0, frames, calls );
  ++outcomes[kOutcomes[outcome]]; ++kinds[kind]; ++byKind[kind][kOutcomes[outcome]];
  if ( outcome == Rejected ) ++reasons[Reason()];
 }
 std::printf( "{\"movieBytes\": %zu, \"chunks\": %zu, \"baselineFrames\": %ld, \"baselineAudioBlocks\": %ld, "
  "\"seed\": %llu, \"first\": %ld, \"count\": %ld, \"framesPublished\": %ld, \"decoderCalls\": %ld, \"outcomes\": ",
  original.size(), chunks.size(), baselineFrames, baselineAudio, seed, first, count, frames, calls );
 PrintMap( outcomes ); std::printf( ", \"kinds\": " ); PrintMap( kinds ); std::printf( ", \"reasons\": " ); PrintMap( reasons );
 std::printf( ", \"outcomesByKind\": {" ); bool firstKind = true;
 for ( const auto &entry : byKind ) { std::printf( "%s\"%s\": ", firstKind ? "" : ", ", entry.first.c_str() ); PrintMap( entry.second ); firstKind = false; }
 std::printf( "}}\n" );
 return 0;
}
'''


def extract(decoder):
    source = decoder.read_text()
    constants = source[source.index('namespace {'):source.index('} // namespace') + len('} // namespace')]
    declarations = source[source.index('static std::array<long, 256> ROQ_YY_tab;'):source.index('extern "C" {\nextern int')]
    decode = source[source.index('static void RllSetupTable('):source.index('static void RoQShutdown( void )')]
    return HOST + constants + '\n' + declarations + '\nstatic qboolean RoQ_init();\n' + decode + DRIVER


def build(folder, decoder):
    cpp, exe = folder / 'roq_fuzz.cpp', folder / 'roq_fuzz.exe'
    cpp.write_text(extract(decoder))
    if os.name == 'nt' and shutil.which('cl') and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() == 'x86':
        command = ['cl', '/nologo', '/std:c++17', '/EHsc', '/O1', '/Zi', '/fsanitize=address', '/I', str(ROOT),
                   str(cpp), f'/Fe:{exe}', f'/Fo:{folder}\\', f'/Fd:{folder}\\']
        target = 'Windows x86, MSVC AddressSanitizer'
    else:
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            raise SystemExit('No C++ compiler found')
        flags = ['-fsanitize=address'] if os.name == 'nt' else ['-m32', '-fsanitize=address,undefined',
                                                                  '-fno-sanitize-recover=all']
        command = [compiler, *flags, '-std=c++17', '-O1', '-g', '-I', str(ROOT), str(cpp), '-o', str(exe)]
        target = ('Windows x64' if os.name == 'nt' else 'Linux i686') + ', Clang ' + ' '.join(flags)
    result = subprocess.run(command, cwd=folder, capture_output=True, text=True, timeout=300)
    if result.returncode:
        raise SystemExit(result.stdout + result.stderr)
    return exe, command, target, hashlib.sha256(cpp.read_bytes()).hexdigest()


def movie(spec):
    directory, member = spec.split('::', 1)
    for pak in sorted(Path(directory).glob('*.pk3'), reverse=True):
        with zipfile.ZipFile(pak) as archive:
            for name in archive.namelist():
                if name.lower() == member.lower():
                    return archive.read(name), f'{pak.parent.name}/{pak.name}:{name}'
    raise SystemExit(f'{member} not found in {directory}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--movie', action='append', required=True,
                        help='GAMEDIR::path inside its paks, e.g. ".../baseq3::video/idlogo.RoQ"; repeatable')
    parser.add_argument('--count', type=int, default=1000, help='variants per movie')
    parser.add_argument('--seed', type=int, default=20260918)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--decoder', type=Path, default=ROOT / 'code/client/cl_cin.cpp',
                        help='decoder source to test, e.g. an older revision of cl_cin.cpp')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    decoder = args.decoder.resolve(strict=True)
    exe, command, target, harness = build(output, decoder)
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    report = dict(target=target, buildCommand=' '.join(command), harnessSha256=harness, seed=args.seed,
                  count=args.count, movies={}, passed=True,
                  decoder=str(decoder.relative_to(ROOT) if decoder.is_relative_to(ROOT) else decoder),
                  decoderSourceSha256=hashlib.sha256(decoder.read_bytes()).hexdigest(),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip())
    for spec in args.movie:
        data, source = movie(spec)
        begin = time.monotonic()
        run = subprocess.run([str(exe), str(args.seed), str(args.count), '0'], input=data, capture_output=True,
                             timeout=3600, **options)
        stdout = run.stdout.decode('utf-8', 'replace')
        row = dict(movie=source, sha256=hashlib.sha256(data).hexdigest(), exitCode=run.returncode,
                   seconds=round(time.monotonic() - begin, 1))
        if run.returncode == 0:
            row.update(json.loads(stdout.strip().splitlines()[-1]))
        else:
            report['passed'] = False
            stderr = run.stderr.decode('utf-8', 'replace')
            started = [line for line in stderr.splitlines() if line.startswith('variant ')]
            row['failedVariant'] = int(started[-1].split()[1]) if started else None
            row['failure'] = (stdout + '\n'.join(line for line in stderr.splitlines()
                                                 if not line.startswith('variant ')))[-4000:]
        report['movies'][source] = row
        print(source, run.returncode, row.get('outcomes') or row['failure'][-300:], flush=True)
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
