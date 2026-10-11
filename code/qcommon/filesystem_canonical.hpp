/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_FILESYSTEM_CANONICAL_HPP
#define FNTECH3_FILESYSTEM_CANONICAL_HPP

#include <filesystem>
#include <cstring>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fntech3::filesystem {

/* Resolve the object actually opened, including Windows junctions and file
 * symlinks. MinGW's std::filesystem::canonical can retain a reparse-point
 * spelling, which is insufficient for installation-containment decisions. */
inline std::filesystem::path Canonical( const std::filesystem::path &path,
                                      std::error_code &error ) {
#ifdef _WIN32
    error.clear();
    using FinalPathFunction = DWORD (WINAPI *)( HANDLE, LPWSTR, DWORD, DWORD );
    static const FinalPathFunction finalPath = [] {
        const FARPROC address = GetProcAddress( GetModuleHandleW( L"kernel32.dll" ),
                                               "GetFinalPathNameByHandleW" );
        FinalPathFunction function = nullptr;
        static_assert( sizeof( function ) == sizeof( address ) );
        // Windows procedure addresses share a representation; copy it without
        // asking GCC to cast between two incompatible function signatures.
        if ( address ) std::memcpy( &function, &address, sizeof( function ) );
        return function;
    }();
    if ( !finalPath ) {
        // Do not add a newer Kernel32 import to the shared executable. A
        // host without this resolver cannot establish containment here.
        error = std::error_code( ERROR_CALL_NOT_IMPLEMENTED, std::system_category() );
        return {};
    }
    struct FileHandle {
        HANDLE value;
        ~FileHandle() { if ( value != INVALID_HANDLE_VALUE ) CloseHandle( value ); }
    } handle{ CreateFileW( path.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr ) };
    if ( handle.value == INVALID_HANDLE_VALUE ) {
        error = std::error_code( GetLastError(), std::system_category() );
        return {};
    }
    // Zero flags request the normalized path with a DOS drive/UNC prefix.
    const DWORD capacity = finalPath( handle.value, nullptr, 0, 0 );
    if ( !capacity ) {
        error = std::error_code( GetLastError(), std::system_category() );
        return {};
    }
    std::wstring resolved( capacity, L'\0' );
    const DWORD length = finalPath( handle.value, resolved.data(), capacity, 0 );
    if ( !length || length >= capacity ) {
        error = std::error_code( length ? ERROR_INSUFFICIENT_BUFFER : GetLastError(),
                                 std::system_category() );
        return {};
    }
    resolved.resize( length );
    if ( resolved.compare( 0, 8, L"\\\\?\\UNC\\" ) == 0 )
        resolved = L"\\\\" + resolved.substr( 8 );
    else if ( resolved.compare( 0, 4, L"\\\\?\\" ) == 0 )
        resolved.erase( 0, 4 );
    return std::filesystem::path( resolved );
#else
    return std::filesystem::canonical( path, error );
#endif
}

} // namespace fntech3::filesystem
#endif
