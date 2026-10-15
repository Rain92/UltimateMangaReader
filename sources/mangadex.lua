-- The api of MangaDex. The list is the most followed titles, then the whole catalogue in windows by creation date
-- (an offset can't go beyond 10000). The chapters are the english ones of the feed of a manga, one upload of each
-- chapter. The pages are the data-saver images of the origin server.

local apiUrl = source.baseUrl
local uploadsUrl = "https://uploads.mangadex.org"
local contentRatings =
    "&contentRating[]=safe&contentRating[]=suggestive&contentRating[]=erotica&contentRating[]=pornographic"
local uuidrx = re.compile([=[[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}]=])

-- --- the json

local str, list, toNumber = common.str, common.list, common.number

-- the first non-empty text of a {lang = text} map (the json's order is lost: the lowest language then, the maps
-- rarely have more than one)
local function firstString(map)
    if type(map) ~= "table" then
        return ""
    end
    local first
    for lang, value in pairs(map) do
        if type(value) == "string" and value ~= "" and (not first or lang < first) then
            first = lang
        end
    end
    return first and text.trim(map[first]) or ""
end

local function localized(map, lang)
    local value = str(map, lang)
    return text.trim(value) ~= "" and value or firstString(map)
end

-- the main title of most japanese, korean and chinese series is the romanized original title, the english one
-- comes first: title.en, then the english alt title
local function mangaTitle(attributes)
    if type(attributes) ~= "table" then
        return ""
    end
    local titles = attributes.title
    local title = str(titles, "en")
    if text.trim(title) ~= "" then
        return title
    end
    local fallback = ""
    for _, alt in ipairs(list(attributes, "altTitles")) do
        title = text.trim(str(alt, "en"))
        if title ~= "" then
            return title
        end
        if fallback == "" then
            fallback = firstString(alt)
        end
    end
    title = titles ~= nil and firstString(titles) or ""
    return title ~= "" and title or fallback
end

-- the descriptions are markdown, the result is html
local linksrx = re.compile([=[^[\s*_]*(?:[\w ]{0,20} )?Links[\s*_]*:?[\s*_]*$]=], "mi")
local mdlinkrx = re.compile([=[\[([^\]]*)\]\([^)]*\)]=])
local bbcoderx = re.compile([=[\[/?(?:b|i|u|s|url|spoiler|quote|img|size|color|center)(?:=[^\]]*)?\]]=], "i")
local rulerx = re.compile([=[^\s*(?:-{3,}|_{3,}|\*{3,})\s*$]=], "m")
local emphasisrx = re.compile([=[\*\*|__]=])
local italicrx = re.compile([=[\*([^*\n]+)\*]=])
local blanklinesrx = re.compile([=[\n[ \t]*(?:\n[ \t]*)+]=])

local function markdownToHtml(markdown)
    markdown = markdown:gsub("\r", "")
    -- the list of the official links that many descriptions end with
    local first = linksrx:find(markdown)
    if first and text.trim(markdown:sub(1, first - 1)) ~= "" then
        markdown = markdown:sub(1, first - 1)
    end
    markdown = mdlinkrx:replace(markdown, "\\1")
    markdown = bbcoderx:replace(markdown, "")
    markdown = rulerx:replace(markdown, "")
    markdown = emphasisrx:replace(markdown, "")
    markdown = italicrx:replace(markdown, "\\1")
    markdown = blanklinesrx:replace(markdown, "\n\n")
    return (text.trim(markdown):gsub("\n", "<br>"))
end

-- --- the list

-- The api allows about 5 requests per second. Its edge servers reject a random part of the list requests at once
-- with "400 Bad Request", a retry right away almost always works. Only a request that fails again slows down,
-- e.g. when the edge rejects most requests for a while. The edge throttles sustained crawls that way: after a
-- minute or so 30-40% get rejected and about one page per second gets through, no matter how many requests run at
-- once (measured 2026-10). More only adds rejected requests.
local listDownloads = {parallel = 2, minInterval = 350, maxInterval = 4000, timeout = 20000, retries = 12}

-- The mangas of the query (its {offset}), at most maxResults, go into the state. The pages that a failed attempt
-- got are kept.
local function crawlWindow(query, maxResults, state, update)
    local limit, numPopular = 100, 2000
    local requestedPages

    local function handler(k, response)
        local page = requestedPages[k]
        local data = common.json(response.body)
        if str(data, "result") ~= "ok" or type(data.data) ~= "table" then
            return false, "Couldn't parse manga list."
        end
        if page == 0 and math.type(data.total) == "integer" then
            state.total = data.total
        end

        local entries = {}
        for _, manga in ipairs(data.data) do
            local id = str(manga, "id")
            if id ~= "" and manga.attributes ~= nil then
                entries[#entries + 1] = {id, mangaTitle(manga.attributes)}
                local createdAt = str(manga.attributes, "createdAt")
                if createdAt > state.lastCreatedAt then
                    state.lastCreatedAt = createdAt
                end
            end
        end
        state.pages[page] = entries

        -- the first page of the popular titles has the size of the whole catalogue
        if state.catalogueSize == 0 then
            state.catalogueSize = state.total
        end
        state.processed = state.processed + #entries
        local expected = state.catalogueSize + math.min(state.catalogueSize, numPopular)
        if expected > 0 then
            update.progress(math.max(0, math.min(100 * state.processed // expected, 99)))
        end
        return true
    end

    if not state.pages[0] then
        requestedPages = {0}
        common.downloadAll({common.fill(query, {offset = 0})}, update, handler, listDownloads)
    end

    local numPages = (math.min(state.total, maxResults) + limit - 1) // limit
    requestedPages = {}
    local urls = {}
    for page = 1, numPages - 1 do
        if not state.pages[page] then
            requestedPages[#requestedPages + 1] = page
            urls[#urls + 1] = common.fill(query, {offset = page * limit})
        end
    end
    common.downloadAll(urls, update, handler, listDownloads)

    local pages = {}
    for page in pairs(state.pages) do
        pages[#pages + 1] = page
    end
    table.sort(pages)
    local mangas = state.mangas
    for _, page in ipairs(pages) do
        for _, entry in ipairs(state.pages[page]) do
            local id = entry[1]
            if not state.ids[id] then
                state.ids[id] = true
                mangas.titles[#mangas.titles + 1] = entry[2]
                mangas.urls[#mangas.urls + 1] = "/manga/" .. id
            end
        end
    end
    state.pages = {}
end

function fetchMangaList(update)
    -- a retry goes on with the window and the pages the failed attempt didn't get
    local state = update.state
    if not state.mangas then
        state.mangas, state.ids = {titles = {}, urls = {}}, {}
        -- -1: the popular titles, then the windows of the catalogue by creation date
        state.window, state.since = -1, ""
        -- of the window that is crawled
        state.pages, state.total, state.lastCreatedAt = {}, 0, ""
        state.catalogueSize, state.processed = 0, 0
    end
    update.progress(update.lastProgress)

    local start = clock()
    listDownloads.count, listDownloads.interval = 0, 0
    -- offset + limit must not exceed 10000
    local windowSize, numPopular = 10000, 2000

    -- the chapters are english only, so only the titles that have english chapters
    local query = apiUrl .. "/manga?limit=100&offset={offset}&availableTranslatedLanguage[]=en" ..
                      "&hasAvailableChapters=true" .. contentRatings

    -- the most followed titles first, so that the order of the list means something at its top
    if state.window < 0 then
        crawlWindow(query .. "&order[followedCount]=desc", numPopular, state, update)
        state.window, state.total, state.lastCreatedAt = 0, 0, ""
    end

    -- then the whole catalogue, in windows by creation date
    while state.window < 100 do
        local windowQuery = query .. "&order[createdAt]=asc"
        if state.since ~= "" then
            windowQuery = windowQuery .. "&createdAtSince=" .. state.since
        end
        crawlWindow(windowQuery, windowSize, state, update)
        if state.total <= windowSize then
            break
        end

        -- createdAtSince is inclusive, the overlapping entries are skipped as duplicates
        local since = state.lastCreatedAt:sub(1, 19)
        if since == "" or since <= state.since then
            raise("parse", "Couldn't page through the manga list.")
        end
        state.since, state.total, state.lastCreatedAt = since, 0, ""
        state.window = state.window + 1
    end

    log("mangas:", #state.mangas.urls, "requests:", listDownloads.count, "time:", clock() - start)
    update.progress(100)
    return {titles = state.mangas.titles, urls = state.mangas.urls}
end

-- --- the info and the chapters

local numberrx = re.compile([=[^(\d{1,15})(?:\.(\d{0,15}))?$]=])

-- "014.5" -> 14 and "5", "3.0" -> 3; ranges and text are unnumbered
local function parseNumber(chapter)
    local whole, part = numberrx:match(text.trim(chapter.number))
    if not whole then
        return false
    end
    part = part or ""
    if not part:find("[^0]") then
        part = ""
    end
    chapter.part, chapter.wholeKey = part, math.tointeger(tonumber(whole))
    return true
end

-- The digits after the dot are either a decimal fraction (24.5, 24.75, 24.9) or a part number (14.1, 14.2, ...,
-- 14.9, 14.10, MangaDex sorts them that way). Decimals are not written with trailing zeros, so a part 10, 20 or
-- 100 means parts, as does 11 or 12 when part 10 is missing. The digits are the identity: 42.8 and 42.08 are
-- different chapters.
local function setPartKeys(chapters)
    local numberedParts = {}
    for _, chapter in ipairs(chapters) do
        local part = chapter.part
        if chapter.isNumbered and (part:sub(-1) == "0" or part == "11" or part == "12") then
            numberedParts[chapter.wholeKey] = true
        end
    end
    for _, chapter in ipairs(chapters) do
        if chapter.isNumbered then
            local part = chapter.part
            chapter.key = tostring(chapter.wholeKey) .. (part == "" and "" or "." .. part)
            chapter.partLength = #part
            if numberedParts[chapter.wholeKey] then
                chapter.partKey = math.tointeger(tonumber(part)) or 0
            else
                chapter.partKey = math.tointeger(tonumber((part .. "000000000000000"):sub(1, 15)))
            end
        end
    end
end

-- Some series (e.g. anthologies) restart the numbering every volume without being flagged. A side story numbered
-- from 1 inside one volume of a normal series must not count, so chapter 1 has to be in at least half of the
-- volumes and most numbers in several volumes.
local function detectVolumeReset(chapters)
    local volumes, volumeCount, volumesByNumber = {}, 0, {}
    for _, chapter in ipairs(chapters) do
        local volume = toNumber(chapter.volume)
        if chapter.isNumbered and volume then
            if not volumes[volume] then
                volumes[volume] = true
                volumeCount = volumeCount + 1
            end
            local numberVolumes = volumesByNumber[chapter.key]
            if not numberVolumes then
                numberVolumes = {count = 0}
                volumesByNumber[chapter.key] = numberVolumes
            end
            if not numberVolumes[volume] then
                numberVolumes[volume] = true
                numberVolumes.count = numberVolumes.count + 1
            end
        end
    end

    local numbered, shared = 0, 0
    for _, numberVolumes in pairs(volumesByNumber) do
        numbered = numbered + numberVolumes.count
        if numberVolumes.count > 1 then
            shared = shared + numberVolumes.count
        end
    end
    local withFirstChapter = volumesByNumber["1"] and volumesByNumber["1"].count or 0
    return withFirstChapter >= 2 and withFirstChapter * 2 >= volumeCount and shared * 2 >= numbered
end

-- the chapters in the order of the api (volume, then chapter) -> oldest first, one upload of each chapter
local function chapterList(chapters, resetOnNewVolume)
    for _, chapter in ipairs(chapters) do
        chapter.isNumbered = parseNumber(chapter)
    end
    setPartKeys(chapters)
    if not resetOnNewVolume then
        resetOnNewVolume = detectVolumeReset(chapters)
    end

    local previous
    for _, chapter in ipairs(chapters) do
        local volume = toNumber(chapter.volume)
        if text.trim(chapter.number) ~= "" then
            chapter.title = "Ch. " .. chapter.number
            if resetOnNewVolume and chapter.volume ~= "" then
                chapter.title = "Vol. " .. chapter.volume .. " " .. chapter.title
            end
            if chapter.hasSubtitle then
                chapter.title = chapter.title .. " " .. chapter.subtitle
            end
        elseif text.trim(chapter.subtitle) ~= "" then
            chapter.title = text.trim(chapter.subtitle)
        else
            -- the untitled uploads without a number are oneshots or whole volumes
            chapter.title = chapter.volume == "" and "Oneshot" or "Vol. " .. chapter.volume
        end

        if chapter.isNumbered then
            chapter.volumeKey = resetOnNewVolume and (volume or 1e9) or 0
            if resetOnNewVolume then
                chapter.key = (volume and ("%.12g"):format(volume) or chapter.volume) .. "|" .. chapter.key
            end
            previous = chapter
        else
            -- the unnumbered chapters keep their place behind the chapter before them
            chapter.volumeKey = previous and previous.volumeKey or -1e9
            chapter.wholeKey = previous and previous.wholeKey or -1
            chapter.partKey = previous and previous.partKey or 0
            chapter.partLength = previous and previous.partLength or 0
            chapter.key = "x|" .. (previous and previous.key or "") .. "|" .. text.lower(chapter.title)
        end
    end

    -- several groups often upload the same chapter, one upload of every chapter stays: one with loaded pages
    -- (they and the reading position stay), else the first upload
    local selected, keys = {}, {}
    for _, chapter in ipairs(chapters) do
        local other = selected[chapter.key]
        if not other then
            selected[chapter.key] = chapter
            keys[#keys + 1] = chapter.key
        elseif chapter.isLoaded ~= other.isLoaded and chapter.isLoaded or
            chapter.isLoaded == other.isLoaded and chapter.createdAt < other.createdAt then
            selected[chapter.key] = chapter
        end
    end
    local sorted = {}
    for i, key in ipairs(keys) do
        sorted[i] = selected[key]
    end
    table.sort(sorted, function(a, b)
        if a.volumeKey ~= b.volumeKey then
            return a.volumeKey < b.volumeKey
        elseif a.wholeKey ~= b.wholeKey then
            return a.wholeKey < b.wholeKey
        elseif a.partKey ~= b.partKey then
            return a.partKey < b.partKey
        elseif a.partLength ~= b.partLength then
            return a.partLength < b.partLength
        elseif a.isNumbered ~= b.isNumbered then
            return a.isNumbered
        end
        return a.serverIndex < b.serverIndex
    end)

    for i, chapter in ipairs(sorted) do
        sorted[i] = {title = chapter.title, url = chapter.id}
    end
    return sorted
end

-- the chapters of the feed of the manga, oldest first
local function fetchChapters(mangaId, resetOnNewVolume, loaded)
    -- the most the feed gives at once, offset + limit must not exceed 10000
    local limit, maxResults = 500, 10000
    local feedUrl = apiUrl .. "/manga/" .. mangaId .. "/feed?limit=500&offset={offset}&translatedLanguage[]=en" ..
                        contentRatings ..
                        "&includeExternalUrl=0&includeEmptyPages=0&includeFuturePublishAt=0&includeUnavailable=0" ..
                        "&order[volume]=asc&order[chapter]=asc&order[createdAt]=asc"

    local chapters, ids, total, offset = {}, {}, 1, 0
    while offset < total and offset + limit <= maxResults do
        -- the edge rejects requests with 400 while it throttles (e.g. during a list update), a retry mostly works
        local ok, response = pcall(http.get, common.fill(feedUrl, {offset = offset}),
                                   {timeout = 24000, retryDelay = 1000, retryStatuses = {400}})
        -- never a part of the list, merging it would delete the pages of the missing chapters
        if not ok then
            raise(response.code, "Couldn't load chapter list: " .. response.message, response.status)
        end
        local data = common.json(response.body)
        if str(data, "result") ~= "ok" or type(data.data) ~= "table" then
            raise("parse", "Couldn't parse chapter list.")
        end
        total = math.type(data.total) == "integer" and data.total or 0

        for _, upload in ipairs(data.data) do
            local id = str(upload, "id")
            local attributes = upload.attributes
            if id ~= "" and not ids[id] and type(attributes) == "table" then
                ids[id] = true
                chapters[#chapters + 1] = {
                    id = id,
                    number = str(attributes, "chapter"),
                    volume = text.trim(str(attributes, "volume")),
                    subtitle = str(attributes, "title"),
                    hasSubtitle = type(attributes.title) == "string",
                    createdAt = str(attributes, "createdAt"),
                    isLoaded = loaded[id] or false,
                    serverIndex = #chapters,
                }
            end
        end
        if #data.data == 0 then
            break
        end
        offset = offset + limit
    end
    return chapterList(chapters, resetOnNewVolume)
end

function fetchMangaInfo(url, known)
    -- the list has the urls of the mangas, the info also needs the authors and the cover
    local mangaId = uuidrx:match(url)
    local requestUrl = mangaId and apiUrl .. "/manga/" .. mangaId ..
                           "?includes[]=author&includes[]=artist&includes[]=cover_art" or url
    -- the chapter list is loaded right after with its own timeouts, this allows for slow wifi as well
    local response = http.get(requestUrl, {timeout = 6000})

    local doc = common.json(response.body)
    local data = str(doc, "result") == "ok" and doc.data
    mangaId = str(data, "id")
    if mangaId == "" or type(data.attributes) ~= "table" then
        raise("parse", "Couldn't parse manga infos.")
    end
    local attributes = data.attributes

    local info = {status = common.capitalize(str(attributes, "status"))}
    if math.type(attributes.year) == "integer" then
        info.releaseYear = tostring(attributes.year)
    end

    local genres, themes = {}, {}
    local demographic = str(attributes, "publicationDemographic")
    if demographic ~= "" then
        genres[1] = common.capitalize(demographic)
    end
    for _, tag in ipairs(list(attributes, "tags")) do
        local tagAttributes = type(tag) == "table" and tag.attributes
        if type(tagAttributes) == "table" then
            local name = tagAttributes.name ~= nil and localized(tagAttributes.name, "en") or ""
            if name ~= "" and tagAttributes.group == "genre" then
                genres[#genres + 1] = name
            elseif name ~= "" and tagAttributes.group == "theme" then
                themes[#themes + 1] = name
            end
        end
    end
    table.move(themes, 1, #themes, #genres + 1, genres)
    info.genres = table.concat(genres, ", ")

    if attributes.description ~= nil then
        info.summary = html.text(markdownToHtml(str(attributes.description, "en")))
    end

    local names = {author = {}, artist = {}}
    for _, relation in ipairs(list(data, "relationships")) do
        local kind = str(relation, "type")
        -- (the ones without attributes are skipped, the ones with null aren't)
        local values = type(relation) == "table" and relation.attributes
        if values ~= nil and values ~= false and names[kind] then
            names[kind][#names[kind] + 1] = str(values, "name")
        elseif values ~= nil and values ~= false and kind == "cover_art" then
            local fileName = str(values, "fileName")
            if fileName ~= "" then
                info.cover = uploadsUrl .. "/covers/" .. mangaId .. "/" .. fileName .. ".512.jpg"
            end
        end
    end
    info.author = common.join(names.author)
    info.artist = common.join(names.artist)

    local loaded = {}
    for _, chapter in ipairs(known) do
        if chapter.pagesLoaded then
            loaded[chapter.url] = true
        end
    end
    info.chapters = fetchChapters(mangaId, attributes.chapterNumbersResetOnNewVolume == true, loaded)
    return info
end

-- --- the pages

-- The at-home endpoint allows 40 requests per minute: 8 at once, then one every 2 s (38 per minute at most), so
-- even during a bulk download a chapter is never delayed for long.
local atHomeInterval, atHomeBurst = 2000, 8
local atHomeSchedule = 0

function fetchPageList(chapterUrl)
    local chapterId = uuidrx:match(chapterUrl) or chapterUrl

    local ok, response
    for attempt = 1, 2 do
        if attempt > 1 then
            sleep(1000)
        end
        local now = clock()
        atHomeSchedule = math.max(atHomeSchedule, now) + atHomeInterval
        local wait = atHomeSchedule - atHomeBurst * atHomeInterval - now
        if wait > 0 then
            sleep(wait)
        end

        ok, response = pcall(http.get, apiUrl .. "/at-home/server/" .. chapterId, {timeout = 8000, retries = 0})
        -- only quick rejections are retried (edge servers, gateways), not timeouts or missing chapters
        if ok or (response.status ~= 400 and (response.status or 0) < 500) then
            break
        end
    end
    if not ok then
        if response.status == 404 then
            raise("notfound", "Chapter is not available on MangaDex (removed or licensed).", 404)
        elseif response.status == 429 then
            raise(response.code, "Too many chapters requested from MangaDex, try again in a minute.", 429)
        end
        error(response, 0)
    end

    local doc = common.json(response.body)
    local chapter = str(doc, "result") == "ok" and doc.chapter
    -- the data-saver pages are jpgs of at most 1080 px width: plenty for e-ink screens, a fraction of the size of
    -- the originals and much faster to decode on the device. They are loaded from the origin server, which the api
    -- returns as baseUrl as well: the page urls are stored and loaded much later, a MangaDex@Home baseUrl is only
    -- valid for about 15 minutes, and every image loaded from it would have to be reported to
    -- api.mangadex.network.
    local quality, pages = "data-saver", type(chapter) == "table" and chapter.dataSaver
    if type(pages) ~= "table" or #pages == 0 then
        quality, pages = "data", type(chapter) == "table" and chapter.data
    end
    local hash = str(chapter, "hash")
    if hash == "" or type(pages) ~= "table" then
        raise("parse", "Couldn't parse page list.")
    end

    local urls = {}
    for _, page in ipairs(pages) do
        if type(page) == "string" then
            urls[#urls + 1] = uploadsUrl .. "/" .. quality .. "/" .. hash .. "/" .. page
        end
    end
    return urls
end
