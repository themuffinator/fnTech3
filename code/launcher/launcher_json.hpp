/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/
// A small, strict JSON document model for the launcher's own settings file and
// the store manifests it reads (GOG's goggame-*.info, Heroic's installed.json).
// Input is untrusted: nesting and sizes are bounded, and anything outside RFC
// 8259 is rejected rather than guessed at.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace fntech3::launcher::json {

class Value {
public:
	enum class Type { Null, Bool, Number, String, Array, Object };

	Value() = default;
	static Value Boolean( bool value );
	static Value Number( double value );
	static Value String( std::string value );
	static Value Array();
	static Value Object();

	Type GetType() const { return type_; }
	bool IsNull() const { return type_ == Type::Null; }
	bool IsBool() const { return type_ == Type::Bool; }
	bool IsNumber() const { return type_ == Type::Number; }
	bool IsString() const { return type_ == Type::String; }
	bool IsArray() const { return type_ == Type::Array; }
	bool IsObject() const { return type_ == Type::Object; }

	bool AsBool( bool fallback = false ) const { return type_ == Type::Bool ? bool_ : fallback; }
	double AsNumber( double fallback = 0.0 ) const { return type_ == Type::Number ? number_ : fallback; }
	// Empty for anything that is not a string.
	const std::string &AsString() const;
	std::string AsString( const char *fallback ) const;

	// Array elements; empty for anything that is not an array.
	const std::vector<Value> &Items() const;
	void Push( Value value );

	// Object members in document order; empty for anything that is not an object.
	const std::vector<std::pair<std::string, Value>> &Members() const;
	// The last member with this key, or nullptr.
	const Value *Find( const char *key ) const;
	// Replaces an existing member or appends a new one. Turns a null into an object.
	Value &Set( const std::string &key, Value value );

private:
	Type type_ = Type::Null;
	bool bool_ = false;
	double number_ = 0.0;
	std::string string_;
	std::vector<Value> items_;
	std::vector<std::pair<std::string, Value>> members_;
};

enum { kMaxDepth = 64 };

// Parses one complete document. On failure `out` is left null and `error`
// (when given) names the problem and its byte offset.
bool Parse( const std::string &text, Value &out, std::string *error = nullptr );

// Two-space indented output with a trailing newline, or a compact single line.
std::string Serialize( const Value &value, bool pretty = true );

} // namespace fntech3::launcher::json
