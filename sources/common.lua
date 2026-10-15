-- The shared code of the sources, loaded before <source>.lua. It runs the definition of the source
-- (<source>.yaml, see README.md): fetchMangaList, fetchMangaInfo, fetchPageList and resolveImage do what its
-- sections list, info, pages and image describe. A script can replace any of them, call the ones here
-- (common.fetchMangaInfo, ...) and use the helpers.

common = {}

-- --- helpers

-- The text of html as a browser shows it (entities, tags, whitespace collapsed), trimmed.
function common.text(s)
    if s:find("[&<]") then
        return text.trim(html.text(s))
    end
    return text.simplify(s)
end

-- The text of a title (e.g. of an attribute): like common.text, but a < in it is no tag.
function common.title(s)
    if s:find("[&<]") then
        return text.trim(html.text((s:gsub("<", "&lt;"))))
    end
    return text.simplify(s)
end

-- The path of url (from the /, with its query) without its . and .. segments.
local function withoutDots(url)
    local path, rest = url:match("^([^?#]*)(.*)$")
    if not path:find("/%.%.?/") and not path:find("/%.%.?$") then
        return url
    end
    local segments = {}
    for segment in path:gmatch("/([^/]*)") do
        if segment == ".." then
            segments[#segments] = nil
        elseif segment ~= "." then
            segments[#segments + 1] = segment
        end
    end
    return "/" .. table.concat(segments, "/") .. (path:find("/%.%.?$") and "/" or "") .. rest
end

-- url (e.g. of a link) resolved against base (the url of the page it is on), with the &amp; of html decoded.
function common.url(base, url)
    url = url:gsub("&amp;", "&")
    if url == "" or url:find("^%a[%w+.-]*:") then
        return url
    elseif url:sub(1, 2) == "//" then
        return "https:" .. url
    end
    local origin, path = base:match("^(%a[%w+.-]*://[^/?#]*)([^?#]*)")
    if not origin then
        return url
    elseif url:sub(1, 1) == "/" then
        return origin .. withoutDots(url)
    end
    return origin .. withoutDots((path:match("^.*/") or "/") .. url)
end

-- template with its {name}s replaced by values[name].
function common.fill(template, values)
    return (template:gsub("{(%w+)}", function(name)
        return values[name] ~= nil and tostring(values[name]) or nil
    end))
end

local linkrx = re.compile([[<a[^>]*>([^<]*)</a>]])

-- The names in html: the texts of its links (each once) joined by ", ", without links its text. remove: a regex
-- for what isn't a name (removed from each of them, e.g. placeholders).
function common.names(fragment, remove)
    local names, seen, links = {}, {}, 0
    for name in linkrx:gmatch(fragment) do
        links = links + 1
        name = common.text(name)
        if remove then
            name = text.trim(remove:replace(name, ""))
        end
        if name ~= "" and not seen[name] then
            seen[name] = true
            names[#names + 1] = name
        end
    end
    if links > 0 then
        return table.concat(names, ", ")
    end
    local name = common.text(fragment)
    return remove and text.trim(remove:replace(name, "")) or name
end

local jsDigits = {}
for i, digit in ipairs({("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"):byte(1, -1)}) do
    jsDigits[digit] = i - 1
end

-- The single quoted JavaScript string at pos of s (its quote): its value and the position after it.
local function jsString(s, pos)
    if s:sub(pos, pos) ~= "'" then
        return nil, pos
    end
    local parts, i = {}, pos + 1
    while true do
        local j = s:find("[\\']", i)
        if not j then
            return nil, pos
        end
        parts[#parts + 1] = s:sub(i, j - 1)
        if s:sub(j, j) == "'" then
            return table.concat(parts), j + 1
        end
        parts[#parts + 1] = s:sub(j + 1, j + 1)
        i = j + 2
    end
end

-- The script that packed is the packed form of (eval(function(p,a,c,k,e,d){...}('payload', radix, count,
-- 'k|e|y|s'.split('|'), 0, {})), nil if it isn't one: the words of the payload are numbers (in radix) of the keywords.
function common.unpackJs(packed)
    local start = packed:find("}('", 1, true)
    if not start then
        return nil
    end
    local payload, pos = jsString(packed, start + 2)
    local radix, count, keysPos = packed:match("^,%s*(%d+)%s*,%s*(%d+)%s*,%s*()", pos)
    if not payload or not radix then
        return nil
    end
    radix, count = tonumber(radix), tonumber(count)
    local keywords = {}
    for keyword in ((jsString(packed, keysPos)) or ""):gmatch("([^|]*)|?") do
        keywords[#keywords + 1] = keyword
    end

    return (payload:gsub("%f[%w_][%w_]+", function(word)
        if radix == 0 or (#word > 1 and word:sub(1, 1) == "0") then
            return nil
        end
        local value = 0
        for i = 1, #word do
            local digit = jsDigits[word:byte(i)]
            if not digit or digit >= radix then
                return nil
            end
            value = value * radix + digit
            if value >= count then
                return nil
            end
        end
        local keyword = keywords[value + 1]
        return keyword ~= "" and keyword or nil
    end))
end

-- The response of a request (http.start), waited for in steps to notice a cancel of the list update.
function common.waitFor(request, update)
    while true do
        local response = request:wait(500)
        if update.cancelled() then
            raise("cancelled", "Download cancelled")
        end
        if response then
            return response
        end
    end
end

-- The sizes of the images ({width, height}, false if unknown) from the starts of their files, loaded at the same
-- time for at most timeout ms, and the indices of the ones that didn't answer in time.
function common.imageSizes(urls, timeout)
    local requests = {}
    for i, url in ipairs(urls) do
        -- servers that ignore the range send everything, the start is enough
        requests[i] = http.start(url, {timeout = timeout, retries = 0, maxBytes = 4096,
                                       headers = {Range = "bytes=0-4095"}})
    end
    local sizes, timedOut = {}, {}
    for i, request in ipairs(requests) do
        local ok, response = pcall(request.wait, request)
        local width, height
        if ok then
            width, height = image.size(response.body)
        elseif response.code == "timeout" then
            timedOut[#timedOut + 1] = i
        end
        sizes[i] = width and {width, height} or false
    end
    return sizes, timedOut
end

-- --- the definition: checked and its patterns compiled when the source is loaded

local def = {}
local hooks = {}

local function fail(path, problem)
    error(source.id .. ".yaml: " .. path .. ": " .. problem, 0)
end

local function compile(pattern, path)
    if type(pattern) ~= "string" then
        fail(path, "a pattern (a string) is expected")
    end
    local ok, rx = pcall(re.compile, pattern)
    if not ok then
        fail(path, rx)
    end
    return rx
end

-- a pattern or a list of them (the first that matches counts)
local function compileList(value, path)
    if type(value) ~= "table" then
        return {compile(value, path)}
    end
    local list = {}
    for i, pattern in ipairs(value) do
        list[i] = compile(pattern, path .. "[" .. i .. "]")
    end
    return list
end

-- a field: pattern(s), or {match = pattern(s), remove = pattern, lines = true, lua = function name}
local function compileField(value, path)
    if type(value) ~= "table" or value[1] then
        return {patterns = compileList(value, path)}
    end
    local field = {}
    for key, v in pairs(value) do
        if key == "match" then
            field.patterns = compileList(v, path .. ".match")
        elseif key == "remove" then
            field.remove = compile(v, path .. ".remove")
        elseif key == "lines" then
            if type(v) ~= "boolean" then
                fail(path .. ".lines", "true or false is expected")
            end
            field.lines = v
        elseif key == "lua" then
            if type(v) ~= "string" then
                fail(path .. ".lua", "the name of a function is expected")
            end
            field.lua = v
            hooks[#hooks + 1] = {name = v, path = path .. ".lua"}
        else
            fail(path .. "." .. key, "unknown key")
        end
    end
    return field
end

local function compileWithin(value, path)
    if type(value) ~= "table" or #value < 1 or #value > 2 then
        fail(path, "[start] or [start, end] is expected")
    end
    return {start = compile(value[1], path .. "[1]"), stop = value[2] and compile(value[2], path .. "[2]")}
end

-- The kinds of the values of the sections, by key.
local kinds = {
    timeout = "integer", pageSize = "integer", estimatedPages = "integer", pages = "integer",
    url = "string", page = "string", order = "order",
    expect = "patterns", notFound = "patterns", complete = "patterns", count = "patterns", when = "patterns",
    each = "pattern", within = "within",
}
-- The keys of each section (subsections are tables), fields: the fields in the order they are read.
local schema = {
    list = {
        url = true, pageSize = true, estimatedPages = true, timeout = true, complete = true,
        mangas = {fields = {"url", "title", "popularity"}},
        popular = {url = true, pages = true, timeout = true, mangas = {fields = {"url", "title"}}},
    },
    info = {
        timeout = true, expect = true, notFound = true, complete = true,
        fields = {"author", "artist", "status", "genres", "summary", "cover", "releaseYear"},
        chapters = {order = true, fields = {"url", "title"}},
    },
    pages = {
        timeout = true, expect = true, notFound = true, complete = true, count = true, page = true,
        images = {fields = {"url"}},
    },
    image = {when = true, timeout = true, within = true, fields = {"url"}},
}
-- the item lists: each, within and fields
local itemLists = {mangas = true, chapters = true, images = true}

local function compileSection(value, shape, path, isItems)
    if type(value) ~= "table" then
        fail(path, "a mapping is expected")
    end
    local fieldSet = {}
    for _, name in ipairs(shape.fields or {}) do
        fieldSet[name] = true
    end

    local section = {fields = {}}
    for key, v in pairs(value) do
        local keyPath = path .. "." .. key
        if key ~= "fields" and type(shape[key]) == "table" then
            section[key] = compileSection(v, shape[key], keyPath, itemLists[key])
        elseif isItems and key == "each" then
            section.each = compile(v, keyPath)
        elseif isItems and key == "within" then
            section.within = compileWithin(v, keyPath)
        elseif fieldSet[key] then
            section.fields[key] = compileField(v, keyPath)
        elseif shape[key] then
            local kind = kinds[key]
            if kind == "patterns" then
                section[key] = compileList(v, keyPath)
            elseif kind == "within" then
                section[key] = compileWithin(v, keyPath)
            elseif kind == "integer" then
                if math.type(v) ~= "integer" then
                    fail(keyPath, "a number is expected")
                end
                section[key] = v
            elseif kind == "order" then
                if v ~= "newestFirst" and v ~= "oldestFirst" then
                    fail(keyPath, "newestFirst or oldestFirst is expected")
                end
                section[key] = v
            else
                if type(v) ~= "string" then
                    fail(keyPath, "a string is expected")
                end
                section[key] = v
            end
        else
            fail(keyPath, "unknown key")
        end
    end

    -- the fields in their order, with the named groups of each that are fields
    section.fieldOrder = {}
    for _, name in ipairs(shape.fields or {}) do
        if section.fields[name] or (section.each and section.each:groups()[name]) then
            section.fieldOrder[#section.fieldOrder + 1] = name
        end
    end
    if isItems then
        if not section.each then
            fail(path .. ".each", "missing")
        end
        for name in pairs(section.each:groups()) do
            if not fieldSet[name] then
                fail(path .. ".each", "the group " .. name .. " isn't a field (" .. table.concat(shape.fields, ", ") .. ")")
            end
        end
    end
    return section
end

-- the app reads these
local topLevel = {name = true, baseUrl = true, headers = true, cookies = true, images = true}
for key, value in pairs(source.definition or {}) do
    if schema[key] then
        def[key] = compileSection(value, schema[key], key)
    elseif not topLevel[key] then
        fail(key, "unknown key")
    end
end
if def.list and not def.list.url then
    fail("list.url", "missing")
end
if def.list and not def.list.mangas then
    fail("list.mangas", "missing")
end
if def.pages and not def.pages.images and not def.pages.count then
    fail("pages", "images or count (with page) is expected")
end
if def.pages and def.pages.count and not def.pages.page then
    fail("pages.page", "missing (the url of a page, with {n})")
end
if def.image and not def.image.fields.url then
    fail("image.url", "missing")
end

-- --- pages and their parts

-- A page of the site: {url (after redirects), body, subject (re.text of the body)}. spec: a section with
-- timeout, expect, notFound, complete. what: for the messages.
local function loadPage(url, spec, what)
    local response = http.get(url, {timeout = spec.timeout})
    local page = {url = response.url, body = response.body}
    page.subject = re.text(page.body)
    if spec.expect and not common.find(spec.expect, page) then
        common.notFound(spec, page)
        raise("parse", "Couldn't read " .. what .. ".")
    end
    if spec.complete and not common.find(spec.complete, page) then
        raise("parse", "The page of " .. what .. " is incomplete.")
    end
    return page
end

-- The captures of the first of the patterns that matches in page, between init and limit.
function common.find(patterns, page, init, limit)
    for _, rx in ipairs(patterns) do
        local value = rx:match(page.subject, init, limit)
        if value then
            return value
        end
    end
end

-- Fails with the message of notFound if it is in the page (e.g. "has been licensed").
function common.notFound(spec, page)
    local message = spec.notFound and common.find(spec.notFound, page)
    if message then
        raise("notfound", common.text(message))
    end
end

-- The part of the page in within: first, limit (nil: to its end). nil if its start isn't there.
local function region(within, page)
    if not within then
        return 1, nil
    end
    local first, last = within.start:find(page.subject)
    if not first or not within.stop then
        return first, nil
    end
    local limit = within.stop:find(page.subject, last + 1)
    if not limit then
        -- e.g. a page that was cut off would have some of the chapters only
        raise("parse", "The page is incomplete.")
    end
    return first, limit
end

local conversions = {
    url = function(value, page) return common.url(page.url, value) end,
    cover = function(value, page) return common.url(page.url, value) end,
    author = function(value, _, field) return common.names(value, field.remove) end,
    artist = function(value, _, field) return common.names(value, field.remove) end,
    genres = function(value, _, field) return common.names(value, field.remove) end,
    popularity = function(value) return tonumber(value) or 0 end,
}
-- toText: common.text unless given
local function textOf(value, _, field, toText)
    -- the line breaks of the html (e.g. of a plain text) stay
    if field.lines then
        value = value:gsub("\r", ""):gsub("\n", "<br>")
    end
    value = (toText or common.text)(value)
    return field.remove and text.trim(field.remove:replace(value, "")) or value
end
conversions.title = function(value, page, field) return textOf(value, page, field, common.title) end

-- The value of the field name of item: of its pattern between init and limit, else of the named group of the
-- item (item[name], raw), converted, then of its hook.
local function fieldValue(section, name, item, page, init, limit)
    local field = section.fields[name] or {}
    local value = item[name]
    if field.patterns then
        value = common.find(field.patterns, page, init, limit)
    end
    if value ~= nil then
        value = (conversions[name] or textOf)(value, page, field)
    end
    if field.lua then
        item[name] = value
        value = _G[field.lua](item, page)
    end
    return value
end

-- The items of an item list (each, within, fields) in the page: a table per match of each with its fields,
-- searched in its text (from its match to the next one). nil if the start of within isn't there. progress(done,
-- count), optional, is called now and then with the bytes of the region done and all of them (e.g. for
-- update.progress, which lets the GUI go on in a long list).
local function findItems(spec, page, progress)
    local first, limit = region(spec.within, page)
    if not first then
        return nil
    end
    local needsText = false
    for _, field in pairs(spec.fields) do
        needsText = needsText or field.lua ~= nil
    end

    -- one match after the other (a long list has tens of thousands), the next one is where the item ends
    local nextMatch = spec.each:matches(page.subject, first, limit)
    local size = (limit or #page.body + 1) - first
    local items = {}
    local match = nextMatch()
    while match do
        local following = nextMatch()
        local itemLimit = following and following.first or limit
        local item = {}
        if needsText then
            item.text = page.body:sub(match.first, itemLimit and itemLimit - 1 or -1)
        end
        for _, name in ipairs(spec.fieldOrder) do
            item[name] = match[name]
            item[name] = fieldValue(spec, name, item, page, match.first, itemLimit)
        end
        items[#items + 1] = item
        if progress and #items % 500 == 0 then
            progress(match.first - first, size)
        end
        match = following
    end
    return items
end

-- --- the list of the mangas

-- The mangas of a page of the list (spec: an item list) that aren't known yet go into mangas ({titles, urls,
-- popularity}). Returns the number of matches and of the new mangas. progress: see findItems.
local function addMangas(spec, page, mangas, known, progress)
    local items = findItems(spec, page, progress) or {}
    local added = 0
    for _, item in ipairs(items) do
        local url, title = item.url, item.title
        if url and url ~= "" and title and not known[url] then
            known[url] = true
            added = added + 1
            local n = #mangas.urls + 1
            mangas.titles[n] = title
            mangas.urls[n] = url
            mangas.popularity[n] = item.popularity or 0
        end
    end
    return #items, added
end

local function pageOf(response)
    local page = {url = response.url, body = response.body}
    page.subject = re.text(page.body)
    return page
end

local function newMangas()
    return {titles = {}, urls = {}, popularity = {}}
end

-- The pages of the list until one doesn't bring new mangas (the number of pages isn't known).
local function pagedList(spec, update, state)
    local pageSize = spec.pageSize or 1
    local estimatedPages = math.max(spec.estimatedPages or 100, (update.currentSize + pageSize - 1) // pageSize)
    local maxPages = 3 * estimatedPages

    -- a retry continues with the pages the failed attempt didn't get
    if not state.nextPage then
        state.nextPage = 1
        state.mangas, state.known = newMangas(), {}
        state.queued, state.shortPages, state.refetched = {}, {}, {}
        state.processed = 0
    end

    -- parallel to state.queued, taken in order: first to last
    local requests, first, last = {}, 1, 0
    local function requestPage(url)
        state.queued[#state.queued + 1] = url
        last = last + 1
        requests[last] = http.start(url, {timeout = spec.timeout or 30000})
    end
    local function requestNextPage()
        if state.nextPage <= maxPages then
            requestPage(common.url(source.baseUrl, common.fill(spec.url, {page = state.nextPage})))
            state.nextPage = state.nextPage + 1
        end
    end

    local queued = state.queued
    state.queued = {}
    for _, url in ipairs(queued) do
        requestPage(url)
    end
    if state.nextPage == 1 then
        for _ = 1, http.parallel do
            requestNextPage()
        end
    end

    -- a page stays queued until it's done
    while first <= last do
        local request = requests[first]
        if request:failed() then
            request:restart()
        end
        local response = request:wait()
        if update.cancelled() then
            raise("cancelled", "Download cancelled")
        end

        requests[first] = nil
        first = first + 1
        local pageUrl = table.remove(state.queued, 1)

        local page = pageOf(response)
        -- a page that was cut off is requested once more
        if spec.complete and not common.find(spec.complete, page) then
            page = pageOf(http.get(pageUrl, {timeout = spec.timeout or 30000}))
            if not common.find(spec.complete, page) then
                raise("parse", "A page of the manga list is incomplete.")
            end
        end

        local matches, added = addMangas(spec.mangas, page, state.mangas, state.known)
        if matches >= pageSize and added > 0 then
            requestNextPage()
            -- the short pages before this one weren't the end: they are fetched once more
            for _, url in ipairs(state.shortPages) do
                state.refetched[url] = true
                requestPage(url)
            end
            state.shortPages = {}
        else
            log("Incomplete page:", pageUrl, matches, added)
            if not state.refetched[pageUrl] then
                state.shortPages[#state.shortPages + 1] = pageUrl
            end
        end

        state.processed = state.processed + 1
        update.progress(math.min(99, 10 + 90 * state.processed // estimatedPages))
    end
    log("pages:", state.nextPage - 1)
    return state.mangas
end

-- The list on one page, after the mangas of the popular pages (in their order).
local function singleList(spec, update, state)
    update.progress(state.popular and 10 or 0)
    local url = common.url(source.baseUrl, spec.url)
    local request = http.start(url, {timeout = spec.timeout or 30000})

    -- the popular pages are only loaded once, the ones that fail only cost the order
    if spec.popular and not state.popular then
        local popular = spec.popular
        local requests = {}
        for i = 1, popular.pages or 1 do
            requests[i] = http.start(common.url(source.baseUrl, common.fill(popular.url, {page = i})),
                                     {timeout = popular.timeout or 30000})
        end
        local mangas, known = newMangas(), {}
        for _, popularRequest in ipairs(requests) do
            local ok, response = pcall(popularRequest.wait, popularRequest)
            if update.cancelled() then
                raise("cancelled", "Download cancelled")
            end
            if ok then
                addMangas(popular.mangas, pageOf(response), mangas, known)
            else
                log("Popular page failed:", response.message)
            end
        end
        state.popular = mangas
    end
    update.progress(10)

    -- a list that failed or was cut off is requested once more
    local page
    for attempt = 1, 2 do
        -- the download can take a while, a cancel is noticed meanwhile
        local ok, response
        repeat
            ok, response = pcall(request.wait, request, 500)
            if update.cancelled() then
                raise("cancelled", "Download cancelled")
            end
        until not ok or response
        if ok then
            page = pageOf(response)
            if not spec.complete or common.find(spec.complete, page) then
                break
            end
            page = nil
        end
        if attempt == 2 then
            if not ok then
                error(response, 0)
            end
            raise("parse", "The manga list is incomplete.")
        end
        log("The manga list failed or is incomplete, once more")
        request = http.start(url, {timeout = spec.timeout or 30000})
    end
    update.progress(80)

    local mangas, known = newMangas(), {}
    for i, mangaUrl in ipairs(state.popular and state.popular.urls or {}) do
        local n = #mangas.urls + 1
        known[mangaUrl] = true
        mangas.titles[n], mangas.urls[n], mangas.popularity[n] = state.popular.titles[i], mangaUrl, 0
    end
    local popularCount = #mangas.urls
    local matches = addMangas(spec.mangas, page, mangas, known,
                              function(done, count) update.progress(80 + 19 * done // count) end)
    log("popular:", popularCount, "matches:", matches)
    if matches == 0 then
        update.clearState()
        raise("parse", "Couldn't read the manga list.")
    end
    -- the order of the list
    mangas.popularity = nil
    return mangas
end

function common.fetchMangaList(update)
    local spec = def.list
    local state = update.state
    update.progress(update.lastProgress)

    local mangas
    if spec.url:find("{page}", 1, true) then
        mangas = pagedList(spec, update, state)
    else
        mangas = singleList(spec, update, state)
    end
    if #mangas.urls == 0 then
        -- nothing worth resuming, a retry starts over
        update.clearState()
        raise("parse", "Couldn't read the manga list.")
    end
    log("mangas:", #mangas.urls)
    update.progress(100)

    -- relative to the site if they all are on it (the lists are large)
    local prefix = source.baseUrl .. "/"
    local relative = true
    for _, url in ipairs(mangas.urls) do
        if url:sub(1, #prefix) ~= prefix then
            relative = false
            break
        end
    end
    if relative then
        for i, url in ipairs(mangas.urls) do
            mangas.urls[i] = url:sub(#source.baseUrl + 1)
        end
    end
    return {titles = mangas.titles, urls = mangas.urls, popularity = mangas.popularity, absoluteUrls = not relative}
end

-- --- the info of a manga

function common.fetchMangaInfo(url)
    local spec = def.info
    local page = loadPage(url, spec, "the manga info")

    -- the hooks see the url
    local info = {url = url}
    for _, name in ipairs(schema.info.fields) do
        if spec.fields[name] then
            info[name] = fieldValue(spec, name, info, page)
        end
    end

    if spec.chapters then
        local items = findItems(spec.chapters, page)
        if not items then
            -- no chapter list, e.g. licensed
            common.notFound(spec, page)
            items = {}
        end
        -- each chapter once (the first of the same url on the page), oldest first
        local chapters, known = {}, {}
        for _, chapter in ipairs(items) do
            if chapter.url and chapter.url ~= "" and not known[chapter.url] then
                known[chapter.url] = true
                chapters[#chapters + 1] = {title = chapter.title or "", url = chapter.url}
            end
        end
        if spec.chapters.order == "newestFirst" then
            local n = #chapters
            for i = 1, n // 2 do
                chapters[i], chapters[n + 1 - i] = chapters[n + 1 - i], chapters[i]
            end
        end
        info.chapters = chapters
    end
    -- the page for a script that wants more of it
    return info, page
end

-- --- the pages of a chapter

function common.fetchPageList(chapterUrl)
    local spec = def.pages
    local page = loadPage(chapterUrl, spec, "the pages")

    -- e.g. the images of a long strip
    if spec.images then
        local images = {}
        for _, item in ipairs(findItems(spec.images, page) or {}) do
            if item.url and item.url ~= "" then
                images[#images + 1] = item.url
            end
        end
        if #images > 0 then
            return images
        end
    end

    -- else the pages of the site: the first count above 0
    local count = 0
    for _, rx in ipairs(spec.count or {}) do
        count = tonumber(rx:match(page.subject) or "") or 0
        if count > 0 then
            break
        end
    end
    if count <= 0 then
        common.notFound(spec, page)
        raise("parse", "Couldn't read the pages.")
    end
    local chapter = page.url:gsub("/$", "")
    local pages = {}
    for n = 1, count do
        pages[n] = common.url(page.url, common.fill(spec.page, {chapter = chapter, n = n}))
    end
    return pages
end

-- --- the image of a page

function common.resolveImage(link)
    local spec = def.image
    if spec.when and not common.find(spec.when, {subject = link.url}) then
        return link
    end
    local page = loadPage(link.url, spec, "the image")
    local first, limit = region(spec.within, page)
    local url = first and fieldValue(spec, "url", {}, page, first, limit)
    if not url or url == "" then
        raise("parse", "Couldn't find the image.")
    end
    return url
end

-- --- the calls of the app: the ones of the definition, unless the script has its own

fetchMangaList = def.list and common.fetchMangaList
fetchMangaInfo = def.info and common.fetchMangaInfo
fetchPageList = def.pages and common.fetchPageList
resolveImage = def.image and common.resolveImage

-- Called by the app after the script was loaded.
function prepare()
    for _, hook in ipairs(hooks) do
        if type(_G[hook.name]) ~= "function" then
            fail(hook.path, source.id .. ".lua has no function " .. hook.name)
        end
    end
    for _, call in ipairs({"fetchMangaList", "fetchMangaInfo", "fetchPageList"}) do
        if type(_G[call]) ~= "function" then
            error(source.id .. ": neither the definition nor the script has " .. call, 0)
        end
    end
end
