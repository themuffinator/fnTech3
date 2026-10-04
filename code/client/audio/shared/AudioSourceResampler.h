// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef FNTECH3_AUDIO_SOURCE_RESAMPLER_H
#define FNTECH3_AUDIO_SOURCE_RESAMPLER_H

#include <cstddef>
#include <string>
#include <vector>

namespace fntech3::audio {

inline std::string SourceResamplerLower( const std::string &value ) {
	std::string result = value;
	for ( char &letter : result ) {
		if ( letter >= 'A' && letter <= 'Z' ) {
			letter = static_cast<char>( letter - 'A' + 'a' );
		}
	}
	return result;
}

// AL_SOFT_source_resampler indexes are runtime-defined. Resolve the advertised
// names rather than assuming an OpenAL Soft version's enumeration order.
// Unrecognized policies/names leave the library's source default in place.
inline int SelectSourceResampler( const std::vector<std::string> &names, const char *request ) {
	const std::string policy = SourceResamplerLower( request != nullptr ? request : "balanced" );
	if ( policy != "fast" && policy != "balanced" && policy != "best" ) {
		return -1;
	}
	if ( names.size() > 128 ) {
		return -1;
	}
	int selected = -1, selectedTaps = 0;
	bool selectedFast = false;
	for ( size_t index = 0; index < names.size(); ++index ) {
		const std::string name = SourceResamplerLower( names[index] );
		if ( name.size() > 256 || name.find( "sinc" ) == std::string::npos ) {
			continue;
		}
		int taps = 0;
		for ( const char letter : name ) {
			if ( letter >= '0' && letter <= '9' ) {
				taps = taps * 10 + letter - '0';
				if ( taps > 256 ) {
					break;
				}
			} else if ( taps != 0 ) {
				break;
			}
		}
		if ( name.find( "order" ) != std::string::npos ) {
			++taps;
		}
		if ( taps < 2 || taps > 256 ) {
			continue;
		}
		const bool fastVariant = name.find( "fast" ) != std::string::npos;
		bool prefer = selected < 0;
		if ( !prefer && taps != selectedTaps ) {
			if ( policy == "fast" ) {
				prefer = taps < selectedTaps;
			} else if ( policy == "best" ) {
				prefer = taps > selectedTaps;
			} else {
				// Balanced prefers the widest available filter up to 24 taps,
				// or the smallest above that when no smaller sinc is available.
				prefer = taps <= 24 ? ( selectedTaps > 24 || taps > selectedTaps ) :
					( selectedTaps > 24 && taps < selectedTaps );
			}
		} else if ( !prefer && taps == selectedTaps ) {
			prefer = selectedFast && !fastVariant;
		}
		if ( prefer ) {
			selected = static_cast<int>( index );
			selectedTaps = taps;
			selectedFast = fastVariant;
		}
	}
	return selected;
}

} // namespace fntech3::audio

#endif
