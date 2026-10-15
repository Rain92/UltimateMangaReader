// clang-format off
#include "json.h"
// clang-format on

#include "lualibs.h"

#include <QRegularExpression>
#include <QTextDocument>

#include "greyscaleimage.h"
#include "lauxlib.h"
#include "luaengine.h"

namespace lua
{

QString toQString(lua_State *L, int index)
{
    size_t length = 0;
    const char *chars = lua_tolstring(L, index, &length);
    return chars ? QString::fromUtf8(chars, qsizetype(length)) : QString();
}

void push(lua_State *L, const QString &string)
{
    auto utf8 = string.toUtf8();
    lua_pushlstring(L, utf8.constData(), size_t(utf8.size()));
}

void pushVariant(lua_State *L, const QVariant &value)
{
    luaL_checkstack(L, 3, "too deeply nested");
    switch (value.typeId())
    {
        case QMetaType::QVariantMap:
        {
            auto map = value.toMap();
            lua_createtable(L, 0, int(map.size()));
            for (auto it = map.cbegin(); it != map.cend(); ++it)
            {
                push(L, it.key());
                pushVariant(L, it.value());
                lua_rawset(L, -3);
            }
            break;
        }
        case QMetaType::QVariantList:
        {
            auto list = value.toList();
            lua_createtable(L, int(list.size()), 0);
            lua_Integer i = 0;
            for (const auto &item : std::as_const(list))
            {
                pushVariant(L, item);
                lua_rawseti(L, -2, ++i);
            }
            break;
        }
        case QMetaType::Bool:
            lua_pushboolean(L, value.toBool());
            break;
        case QMetaType::LongLong:
        case QMetaType::Int:
            lua_pushinteger(L, value.toLongLong());
            break;
        case QMetaType::Double:
            lua_pushnumber(L, value.toDouble());
            break;
        default:
            if (value.isValid())
                push(L, value.toString());
            else
                lua_pushnil(L);
    }
}

void pushError(lua_State *L, const Error &error)
{
    lua_createtable(L, 0, 3);
    lua_pushstring(L, LuaEngine::codeName(error.code));
    lua_setfield(L, -2, "code");
    push(L, error.message);
    lua_setfield(L, -2, "message");
    lua_pushinteger(L, error.httpStatus);
    lua_setfield(L, -2, "status");
}

void raise(lua_State *L, const Error &error)
{
    pushError(L, error);
    lua_error(L);
    Q_UNREACHABLE();
}

int rawField(lua_State *L, int index, const char *name)
{
    index = lua_absindex(L, index);
    lua_pushstring(L, name);
    return lua_rawget(L, index);
}

QString stringField(lua_State *L, int index, const char *name)
{
    QString value = rawField(L, index, name) == LUA_TSTRING ? toQString(L, -1) : QString();
    lua_pop(L, 1);
    return value;
}

// --- re

// The positions of a QString and those of its UTF-8 encoding (what the scripts see), from checkpoints.
// (Bytes that weren't UTF-8 became U+FFFD, they shift the positions.)
class PositionMap
{
public:
    explicit PositionMap(const QString *text = nullptr, qsizetype utf8Size = -1) { reset(text, utf8Size); }

    void reset(const QString *text, qsizetype utf8Size)
    {
        this->text = text;
        // each other character takes more bytes in UTF-8 than in UTF-16
        ascii = !text || utf8Size == text->size();
        checkpoints.clear();
    }

    qsizetype toUtf8(qsizetype position)
    {
        if (ascii)
            return position;
        index();
        position = qBound(0, position, text->size());
        qsizetype pos16 = position / interval * interval, pos8 = checkpoints[position / interval];
        auto data = text->utf16();
        while (pos16 < position)
            pos8 += utf8Length(data[pos16++]);
        return pos8;
    }

    // the character that the byte at position belongs to
    qsizetype toUtf16(qsizetype position)
    {
        if (ascii)
            return position;
        index();
        // the last checkpoint at or before position
        auto checkpoint = std::upper_bound(checkpoints.cbegin(), checkpoints.cend(), position) - 1;
        qsizetype pos16 = (checkpoint - checkpoints.cbegin()) * interval, pos8 = *checkpoint;
        auto data = text->utf16();
        while (pos8 < position && pos16 < text->size())
            pos8 += utf8Length(data[pos16++]);
        return pos16;
    }

private:
    static constexpr qsizetype interval = 128;

    const QString *text;
    bool ascii;
    // the UTF-8 positions of every interval-th character
    QList<qsizetype> checkpoints;

    // a surrogate pair takes 4 bytes (without branches, so the loops vectorize)
    static int utf8Length(char16_t c) { return 1 + (c >= 0x80) + (c >= 0x800) - ((c & 0xf800) == 0xd800); }

    void index()
    {
        if (!checkpoints.isEmpty())
            return;
        auto data = text->utf16();
        qsizetype size = text->size();
        checkpoints.resize(size / interval + 1);
        qsizetype pos8 = 0;
        for (qsizetype block = 0; block * interval < size; block++)
        {
            checkpoints[block] = pos8;
            qsizetype end = qMin(size, (block + 1) * interval);
            int length = 0;
            for (qsizetype pos16 = block * interval; pos16 < end; pos16++)
                length += utf8Length(data[pos16]);
            pos8 += length;
        }
        // the end, when it's at a checkpoint
        if (size % interval == 0)
            checkpoints.last() = pos8;
    }
};

// re.text(s): a text that is searched more than once, converted once
struct Text
{
    QString text;
    PositionMap map;
};

struct Regex
{
    QRegularExpression rx;
    // by the number of the group, empty for the unnamed ones
    QStringList names;
};

static const char *textType = "umr.Text";
static const char *regexType = "umr.Regex";

template <typename T>
static int collect(lua_State *L)
{
    static_cast<T *>(lua_touserdata(L, 1))->~T();
    return 0;
}

// The subject of a search: a re.text() or a string (converted now).
class Subject
{
public:
    Subject(lua_State *L, int index)
    {
        if (auto text = static_cast<Text *>(luaL_testudata(L, index, textType)))
        {
            string = &text->text;
            map = &text->map;
            return;
        }
        size_t length = 0;
        const char *chars = luaL_checklstring(L, index, &length);
        owned = QString::fromUtf8(chars, qsizetype(length));
        ownedMap.reset(&owned, qsizetype(length));
        string = &owned;
        map = &ownedMap;
    }

    const QString *string;
    PositionMap *map;

private:
    QString owned;
    PositionMap ownedMap;
};

static Regex *checkRegex(lua_State *L)
{
    return static_cast<Regex *>(luaL_checkudata(L, 1, regexType));
}

// The start of the search (init, from 1) as a position of the string, -1 if it is behind its end.
static qsizetype startPosition(lua_State *L, int index, Subject &subject)
{
    lua_Integer init = luaL_optinteger(L, index, 1);
    if (init < 1)
        init = 1;
    qsizetype position = subject.map->toUtf16(qsizetype(init - 1));
    return position > subject.string->size() ? -1 : position;
}

// The subject ends before limit (a position from 1): the matches lie before it. Without a limit (nil) it's all of
// it.
static QStringView limitedView(lua_State *L, int index, Subject &subject)
{
    QStringView view(*subject.string);
    if (lua_isnoneornil(L, index))
        return view;
    return view.left(subject.map->toUtf16(qsizetype(qMax<lua_Integer>(luaL_checkinteger(L, index) - 1, 0))));
}

// The groups of the match, or the whole match without groups.
static int pushCaptures(lua_State *L, const QRegularExpressionMatch &match)
{
    int groups = match.lastCapturedIndex();
    int count = qMax(1, match.regularExpression().captureCount());
    luaL_checkstack(L, count, "too many captures");
    if (match.regularExpression().captureCount() == 0)
    {
        push(L, match.captured(0));
        return 1;
    }
    for (int i = 1; i <= count; i++)
        push(L, i <= groups ? match.captured(i) : QString());
    return count;
}

// rx:match(s [, init [, limit]]): the captures of the first match, nil without one
static int regexMatch(lua_State *L)
{
    auto regex = checkRegex(L);
    Subject subject(L, 2);
    qsizetype start = startPosition(L, 3, subject);
    auto view = limitedView(L, 4, subject);
    if (start < 0 || start > view.size())
        return 0;

    auto match = regex->rx.matchView(view, start);
    if (!match.hasMatch())
        return 0;
    return pushCaptures(L, match);
}

// rx:find(s [, init [, limit]]): first, last (the bytes of the match, from 1) and the captures, nil without one
static int regexFind(lua_State *L)
{
    auto regex = checkRegex(L);
    Subject subject(L, 2);
    qsizetype start = startPosition(L, 3, subject);
    auto view = limitedView(L, 4, subject);
    if (start < 0 || start > view.size())
        return 0;

    auto match = regex->rx.matchView(view, start);
    if (!match.hasMatch())
        return 0;
    lua_pushinteger(L, subject.map->toUtf8(match.capturedStart()) + 1);
    lua_pushinteger(L, subject.map->toUtf8(match.capturedEnd()));
    return 2 + pushCaptures(L, match);
}

// rx:all(s [, init [, limit]]): all matches, each a table with the groups at 1.. (the whole match at 0), the named
// groups by their names, and first, last (its bytes)
static int regexAll(lua_State *L)
{
    auto regex = checkRegex(L);
    Subject subject(L, 2);
    qsizetype start = startPosition(L, 3, subject);
    auto view = limitedView(L, 4, subject);
    lua_newtable(L);
    if (start < 0 || start > view.size())
        return 1;

    int groups = regex->rx.captureCount();
    lua_Integer n = 0;
    auto it = regex->rx.globalMatchView(view, start);
    while (it.hasNext())
    {
        auto match = it.next();
        lua_createtable(L, groups + 1, 2);
        for (int i = 0; i <= groups; i++)
        {
            push(L, match.captured(i));
            if (i > 0 && !regex->names[i].isEmpty())
            {
                lua_pushvalue(L, -1);
                lua_setfield(L, -3, regex->names[i].toUtf8().constData());
            }
            lua_rawseti(L, -2, i);
        }
        lua_pushinteger(L, subject.map->toUtf8(match.capturedStart()) + 1);
        lua_setfield(L, -2, "first");
        lua_pushinteger(L, subject.map->toUtf8(match.capturedEnd()));
        lua_setfield(L, -2, "last");
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

// the state of rx:gmatch(): the subject and the iterator over it
struct GlobalMatch
{
    QString subject;
    QRegularExpressionMatchIterator it;
};
static const char *globalMatchType = "umr.GlobalMatch";

static int globalMatchNext(lua_State *L)
{
    auto state = static_cast<GlobalMatch *>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!state->it.hasNext())
        return 0;
    return pushCaptures(L, state->it.next());
}

// rx:gmatch(s [, init [, limit]]): an iterator over the captures of the matches, like string.gmatch()
static int regexGmatch(lua_State *L)
{
    auto regex = checkRegex(L);
    Subject subject(L, 2);
    qsizetype start = startPosition(L, 3, subject);
    auto size = limitedView(L, 4, subject).size();

    auto state = static_cast<GlobalMatch *>(lua_newuserdatauv(L, sizeof(GlobalMatch), 0));
    new (state) GlobalMatch{*subject.string, {}};
    luaL_setmetatable(L, globalMatchType);
    if (start >= 0 && start <= size)
        state->it = regex->rx.globalMatchView(QStringView(state->subject).left(size), start);
    lua_pushcclosure(L, globalMatchNext, 1);
    return 1;
}

// rx:groups(): the named groups, {name = number}
static int regexGroups(lua_State *L)
{
    auto regex = checkRegex(L);
    lua_newtable(L);
    for (int i = 1; i < regex->names.size(); i++)
        if (!regex->names[i].isEmpty())
        {
            lua_pushinteger(L, i);
            lua_setfield(L, -2, regex->names[i].toUtf8().constData());
        }
    return 1;
}

// rx:replace(s, replacement): with \1 for the groups (QString::replace())
static int regexReplace(lua_State *L)
{
    auto regex = checkRegex(L);
    auto string = toQString(L, 2);
    auto replacement = QString::fromUtf8(luaL_checkstring(L, 3));
    push(L, string.replace(regex->rx, replacement));
    return 1;
}

// re.compile(pattern [, flags]): flags "i" (ignore case), "s" (. matches newlines), "m" (^ and $ at lines), "x"
static int reCompile(lua_State *L)
{
    auto pattern = QString::fromUtf8(luaL_checkstring(L, 1));
    QByteArray flags = luaL_optstring(L, 2, "");

    QRegularExpression::PatternOptions options;
    for (char flag : flags)
    {
        if (flag == 'i')
            options |= QRegularExpression::CaseInsensitiveOption;
        else if (flag == 's')
            options |= QRegularExpression::DotMatchesEverythingOption;
        else if (flag == 'm')
            options |= QRegularExpression::MultilineOption;
        else if (flag == 'x')
            options |= QRegularExpression::ExtendedPatternSyntaxOption;
        else
            return luaL_error(L, "unknown regex flag '%c'", flag);
    }

    QRegularExpression rx(pattern, options);
    if (!rx.isValid())
        return luaL_error(L, "invalid regex %s: %s", luaL_checkstring(L, 1), qPrintable(rx.errorString()));
    rx.optimize();

    auto regex = static_cast<Regex *>(lua_newuserdatauv(L, sizeof(Regex), 0));
    new (regex) Regex{rx, rx.namedCaptureGroups()};
    luaL_setmetatable(L, regexType);
    return 1;
}

// re.text(s): s for more than one search (converted once)
static int reText(lua_State *L)
{
    size_t length = 0;
    const char *chars = luaL_checklstring(L, 1, &length);
    auto text = static_cast<Text *>(lua_newuserdatauv(L, sizeof(Text), 0));
    new (text) Text{QString::fromUtf8(chars, qsizetype(length)), PositionMap()};
    text->map.reset(&text->text, qsizetype(length));
    luaL_setmetatable(L, textType);
    // The collector only sees the userdata, it should work as if the text was allocated as well. (Without this
    // the texts of a list update with hundreds of pages pile up: 180 MB more on the device for MangaTown.)
    lua_gc(L, LUA_GCSTEP, int(text->text.size() * 2 / 1024));
    return 1;
}

static void openRe(lua_State *L)
{
    luaL_newmetatable(L, regexType);
    lua_pushcfunction(L, collect<Regex>);
    lua_setfield(L, -2, "__gc");
    static const luaL_Reg regexMethods[] = {{"match", regexMatch}, {"find", regexFind},     {"all", regexAll},
                                            {"gmatch", regexGmatch}, {"replace", regexReplace}, {"groups", regexGroups},
                                            {nullptr, nullptr}};
    luaL_newlib(L, regexMethods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newmetatable(L, textType);
    lua_pushcfunction(L, collect<Text>);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    luaL_newmetatable(L, globalMatchType);
    lua_pushcfunction(L, collect<GlobalMatch>);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    static const luaL_Reg functions[] = {{"compile", reCompile}, {"text", reText}, {nullptr, nullptr}};
    luaL_newlib(L, functions);
    lua_setglobal(L, "re");
}

// --- text, html

static int textTrim(lua_State *L)
{
    push(L, toQString(L, 1).trimmed());
    return 1;
}

static int textSimplify(lua_State *L)
{
    push(L, toQString(L, 1).simplified());
    return 1;
}

static int textLower(lua_State *L)
{
    push(L, toQString(L, 1).toLower());
    return 1;
}

static int textUpper(lua_State *L)
{
    push(L, toQString(L, 1).toUpper());
    return 1;
}

// like MangaSourceBase::htmlToPlainText()
static QString htmlToPlainText(const QString &html)
{
    // never destroyed: that would be after the application, which it needs
    static auto converter = new QTextDocument;
    converter->setHtml(html);
    return converter->toPlainText();
}

static int htmlText(lua_State *L)
{
    luaL_checkstring(L, 1);
    push(L, htmlToPlainText(toQString(L, 1)));
    return 1;
}

// like MangaSourceBase::genresFromHtml()
static int htmlGenres(lua_State *L)
{
    luaL_checkstring(L, 1);
    push(L, htmlToPlainText(toQString(L, 1))
                .trimmed()
                .remove('\n')
                .replace(", ", " ")
                .replace("/ ", " ")
                .replace(",", " "));
    return 1;
}

// --- image

// image.size(data): the width and height of the image whose file starts with data, nil if they aren't in it
static int imageSize(lua_State *L)
{
    size_t length;
    auto data = luaL_checklstring(L, 1, &length);
    QSize size;
    if (!readImageSize(QByteArray::fromRawData(data, qsizetype(length)), size))
        return 0;
    lua_pushinteger(L, size.width());
    lua_pushinteger(L, size.height());
    return 2;
}

// --- json

// deeper json fails (the conversion is recursive)
static const int maxJsonDepth = 200;

static void pushJson(lua_State *L, const rapidjson::Value &value, int depth = 0)
{
    if (depth > maxJsonDepth)
        raise(L, {Error::Parse, "The json is nested too deeply.", 0});
    luaL_checkstack(L, 3, "json nested too deeply");
    switch (value.GetType())
    {
        case rapidjson::kNullType:
            lua_pushlightuserdata(L, nullptr);
            break;
        case rapidjson::kFalseType:
        case rapidjson::kTrueType:
            lua_pushboolean(L, value.GetBool());
            break;
        case rapidjson::kStringType:
            lua_pushlstring(L, value.GetString(), value.GetStringLength());
            break;
        case rapidjson::kNumberType:
            if (value.IsInt64())
                lua_pushinteger(L, value.GetInt64());
            else
                lua_pushnumber(L, value.GetDouble());
            break;
        case rapidjson::kArrayType:
        {
            lua_createtable(L, int(value.Size()), 0);
            lua_Integer i = 0;
            for (const auto &element : value.GetArray())
            {
                pushJson(L, element, depth + 1);
                lua_rawseti(L, -2, ++i);
            }
            break;
        }
        case rapidjson::kObjectType:
        {
            lua_createtable(L, 0, int(value.MemberCount()));
            for (const auto &member : value.GetObject())
            {
                lua_pushlstring(L, member.name.GetString(), member.name.GetStringLength());
                pushJson(L, member.value, depth + 1);
                lua_rawset(L, -3);
            }
            break;
        }
    }
}

// json.decode(s): tables, json.null for null
static int jsonDecode(lua_State *L)
{
    size_t length = 0;
    const char *chars = luaL_checklstring(L, 1, &length);
    rapidjson::Document document;
    // not recursive, for any depth
    document.Parse<rapidjson::kParseIterativeFlag>(chars, length);
    if (document.HasParseError())
        raise(L, {Error::Parse, QString("Invalid json (at %1).").arg(document.GetErrorOffset()), 0});
    pushJson(L, document);
    return 1;
}

// --- proto

static bool readVarint(const unsigned char *data, size_t &pos, size_t end, quint64 &value)
{
    value = 0;
    for (int shift = 0; shift < 64 && pos < end; shift += 7)
    {
        unsigned char byte = data[pos++];
        value |= quint64(byte & 0x7f) << shift;
        if (byte < 0x80)
            return true;
    }
    return false;
}

// Appends the value on top of the stack to the list of the field number of the message at index.
static void appendField(lua_State *L, int message, lua_Integer number)
{
    if (lua_rawgeti(L, message, number) == LUA_TNIL)
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawseti(L, message, number);
    }
    lua_insert(L, -2);
    lua_rawseti(L, -2, lua_Integer(lua_rawlen(L, -2)) + 1);
    lua_pop(L, 1);
}

// proto.decode(data): the fields of a protobuf message, {[number] = {values}} in their order: integers (varint,
// and the bits of fixed32 and fixed64) and strings (length-delimited: texts, bytes or messages to decode).
// Invalid or truncated data is an empty message.
static int protoDecode(lua_State *L)
{
    size_t end = 0;
    auto data = reinterpret_cast<const unsigned char *>(luaL_checklstring(L, 1, &end));
    lua_newtable(L);
    int message = lua_gettop(L);
    luaL_checkstack(L, 4, "proto");

    size_t pos = 0;
    bool valid = true;
    while (pos < end && valid)
    {
        valid = false;
        quint64 tag = 0;
        if (!readVarint(data, pos, end, tag) || (tag >> 3) == 0 || (tag >> 3) > 0x1fffffff)
            break;
        auto number = lua_Integer(tag >> 3);
        int wireType = int(tag & 7);

        quint64 value = 0;
        if (wireType == 0)
        {
            if (!readVarint(data, pos, end, value))
                break;
            lua_pushinteger(L, lua_Integer(value));
        }
        else if (wireType == 2)
        {
            if (!readVarint(data, pos, end, value) || value > end - pos)
                break;
            lua_pushlstring(L, reinterpret_cast<const char *>(data + pos), size_t(value));
            pos += size_t(value);
        }
        else if (wireType == 1 || wireType == 5)
        {
            size_t size = wireType == 1 ? 8 : 4;
            if (end - pos < size)
                break;
            for (size_t i = 0; i < size; i++)
                value |= quint64(data[pos + i]) << (8 * i);
            pos += size;
            lua_pushinteger(L, lua_Integer(value));
        }
        else
        {
            break;
        }
        appendField(L, message, number);
        valid = true;
    }

    if (!valid)
        lua_newtable(L);
    return 1;
}

// --- raise

static int raiseError(lua_State *L)
{
    Error error;
    error.code = LuaEngine::codeFromName(QString::fromUtf8(luaL_checkstring(L, 1)));
    error.message = QString::fromUtf8(luaL_optstring(L, 2, ""));
    error.httpStatus = int(luaL_optinteger(L, 3, 0));
    raise(L, error);
}

void openLibraries(lua_State *L)
{
    openRe(L);

    static const luaL_Reg textFunctions[] = {{"trim", textTrim},   {"simplify", textSimplify},
                                             {"lower", textLower}, {"upper", textUpper},
                                             {nullptr, nullptr}};
    luaL_newlib(L, textFunctions);
    lua_setglobal(L, "text");

    static const luaL_Reg htmlFunctions[] = {{"text", htmlText}, {"genres", htmlGenres}, {nullptr, nullptr}};
    luaL_newlib(L, htmlFunctions);
    lua_setglobal(L, "html");

    static const luaL_Reg jsonFunctions[] = {{"decode", jsonDecode}, {nullptr, nullptr}};
    luaL_newlib(L, jsonFunctions);
    lua_pushlightuserdata(L, nullptr);
    lua_setfield(L, -2, "null");
    lua_setglobal(L, "json");

    static const luaL_Reg imageFunctions[] = {{"size", imageSize}, {nullptr, nullptr}};
    luaL_newlib(L, imageFunctions);
    lua_setglobal(L, "image");

    static const luaL_Reg protoFunctions[] = {{"decode", protoDecode}, {nullptr, nullptr}};
    luaL_newlib(L, protoFunctions);
    lua_setglobal(L, "proto");

    lua_register(L, "raise", raiseError);
}

}  // namespace lua
