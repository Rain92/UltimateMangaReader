#pragma once

#include <QString>
#include <QVariant>

#include "lua.h"
#include "result.h"

// The libraries of the source scripts besides Lua's own:
// - re: regular expressions (QRegularExpression, Perl syntax). Positions are byte positions in the UTF-8 text,
//   like the ones of Lua's string functions. The searches take an optional init (where they start) and limit (the
//   text ends before it, the matches lie before it).
// - text: trim, simplify (whitespace like QString), lower, upper (all of Unicode).
// - html: text (the plain text of html), genres.
// - json: decode (into tables; null is json.null).
// - image: size (the width and height of an image from the start of its file, JPEG, PNG or WebP).
// - proto: decode (the fields of a protobuf message by their numbers, see protoDecode()); common.lua adds the
//   accessors of the fields (proto.int, proto.bytes, proto.message, proto.messages).
// - raise(code, message [, status]): fails with an error of the app (code: "failed", "network", "timeout",
//   "cancelled", "http", "parse", "notfound", "storage").
namespace lua
{
void openLibraries(lua_State *L);

QString toQString(lua_State *L, int index);
void push(lua_State *L, const QString &string);
// Maps and lists become tables (e.g. what readYaml() read).
void pushVariant(lua_State *L, const QVariant &value);
void pushError(lua_State *L, const Error &error);
// Fails the running script with the error.
[[noreturn]] void raise(lua_State *L, const Error &error);

// A field of the table at index without metamethods, nil if it's missing (pushed).
int rawField(lua_State *L, int index, const char *name);
QString stringField(lua_State *L, int index, const char *name);
}  // namespace lua
