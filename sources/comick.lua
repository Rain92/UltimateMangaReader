-- The api of comick.art. The list is the most followed and the newest titles of the search, then the sitemaps
-- (only slugs). A chapter is one upload, chosen from the uploads of all groups.

local apiUrl = source.baseUrl .. "/api"
local userAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"
local statuses = {"Ongoing", "Completed", "Cancelled", "Hiatus"}

-- the search api returns 50 titles per request and can only be paged with a cursor
local popularPages = 20
-- the sitemaps are only regenerated once a day, the newest titles are missing there
local latestPages = 2
-- series that need all uploads (60 per page of the chapter list) are only handled up to this size
local maxListPages = 5
-- pages that are warmed up in the background ahead of the requested one
local prefetchPages = 3

-- --- the json

-- the string (or number) object[key] as a text, "" without one
local function str(object, key)
    local value = type(object) == "table" and object[key]
    if type(value) == "string" then
        return value
    elseif math.type(value) == "integer" then
        return tostring(value)
    elseif type(value) == "number" then
        return ("%.6g"):format(value)
    end
    return ""
end

-- the names of the list object[key] joined by ", "
local function joinNames(object, key)
    local names, seen = {}, {}
    for _, entry in ipairs(type(object[key]) == "table" and object[key] or {}) do
        local name = text.trim(str(entry, "name"))
        if name ~= "" and not seen[name] then
            seen[name] = true
            names[#names + 1] = name
        end
    end
    return table.concat(names, ", ")
end

local decimalrx = re.compile([=[^[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$]=])

-- the number of a text like QString::toDouble(), nil if it isn't one
local function toNumber(s)
    return decimalrx:find(s) and tonumber(s) or nil
end

-- e.g. "31.2.5" is sorted as 31.2
local function parseNumber(number)
    local prefix = toNumber(number) and number or number:match("^%d+%.%d+") or number:match("^%d+")
    return prefix and tonumber(prefix) or 0
end

-- missing numbers are sometimes the string "None"
local function numberText(object, key)
    local number = text.trim(str(object, key))
    return number:lower() == "none" and "" or number
end

-- --- the uploads

local function parseUpload(c)
    local upload = {
        hid = str(c, "hid"),
        rawChap = text.trim(str(c, "chap")),
        chap = numberText(c, "chap"),
        vol = numberText(c, "vol"),
        lang = str(c, "lang"),
        title = text.trim(str(c, "title")),
        group = "",
    }
    upload.number = parseNumber(upload.chap)
    local volume = toNumber(upload.vol)
    upload.hasVolume, upload.volume = volume ~= nil, volume or 0
    -- the volume of a numbered chapter, from another upload of it if necessary (-1 if unknown)
    upload.knownVolume = upload.number > 0 and volume or -1
    -- whole volumes are uploaded without a chapter number
    upload.isVolume = upload.hasVolume and (upload.chap == "" or upload.chap == "0")
    if type(c) == "table" and type(c.group_name) == "table" then
        local groups = {}
        for _, group in ipairs(c.group_name) do
            if type(group) == "string" then
                groups[#groups + 1] = group .. "\n"
            end
        end
        upload.group = table.concat(groups)
    end
    return upload
end

-- the english uploads of a json list
local function parseUploads(chapters, uploads)
    if type(chapters) ~= "table" then
        return
    end
    for _, c in ipairs(chapters) do
        local upload = parseUpload(c)
        if upload.hid ~= "" and (upload.lang == "" or upload.lang == "en") then
            uploads[#uploads + 1] = upload
        end
    end
end

-- "<slug>/<hid>-chapter-<chap>-<lang>", after the api url for the page list
local function chapterPath(slug, upload)
    return slug .. "/" .. upload.hid .. "-chapter-" .. (upload.rawChap == "" and "0" or upload.rawChap) .. "-" ..
               (upload.lang == "" and "en" or upload.lang)
end

-- byVolume: the chapter numbers start again in every volume
local function makeChapter(slug, upload, byVolume)
    local name
    if upload.hasVolume and (upload.chap == "" or upload.chap == "0" and not byVolume) then
        name = "Vol. " .. upload.vol
    elseif byVolume and upload.hasVolume then
        name = "Vol. " .. upload.vol .. " Ch. " .. upload.chap
    elseif upload.chap == "" then
        name = upload.title == "" and "Oneshot" or upload.title
    else
        name = "Ch. " .. upload.chap
    end
    if upload.title ~= "" and name ~= upload.title then
        name = name .. ": " .. upload.title
    end
    return {title = name, url = chapterPath(slug, upload)}
end

-- a map by numbers (or pairs of numbers) that is gone through in their order, like a QMap
local function newMap()
    return {values = {}, keys = {}}
end

local function mapKey(a, b)
    return b == nil and a or ("%.17g|%.17g"):format(a, b)
end

local function mapGet(map, a, b)
    return map.values[mapKey(a, b)]
end

local function mapSet(map, value, a, b)
    local key = mapKey(a, b)
    if map.values[key] == nil then
        map.keys[#map.keys + 1] = {a, b or 0, key}
    end
    map.values[key] = value
end

-- the values in the order of their keys
local function mapValues(map)
    table.sort(map.keys, function(x, y)
        if x[1] ~= y[1] then
            return x[1] < y[1]
        end
        return x[2] < y[2]
    end)
    local values = {}
    for i, key in ipairs(map.keys) do
        values[i] = map.values[key[3]]
    end
    return values
end

-- keeps one upload per key: the one of the current chapters, else the first one
local function chooseUpload(chosen, upload, currentHids, a, b)
    local other = mapGet(chosen, a, b)
    if not other or currentHids[upload.hid] and not currentHids[other.hid] then
        mapSet(chosen, upload, a, b)
    end
end

-- one whole volume upload of each volume
local function collectVolumes(uploads, currentHids, volumes)
    for _, upload in ipairs(uploads) do
        if upload.isVolume then
            chooseUpload(volumes, upload, currentHids, upload.volume)
        end
    end
end

-- the chapter numbers that a group uploaded in different volumes
local function restartedNumbers(uploads)
    local volumes, restarted, count = {}, {}, 0
    for _, upload in ipairs(uploads) do
        if upload.number > 0 and upload.vol ~= "" then
            local key = upload.group .. upload.chap
            if not volumes[key] then
                volumes[key] = upload.vol
            elseif volumes[key] ~= upload.vol and not restarted[upload.chap] then
                restarted[upload.chap] = true
                count = count + 1
            end
        end
    end
    return count
end

-- One upload per volume and chapter number, sorted by volume. The uploads without volume are usually numbered
-- through all volumes, the ones beyond the chapters in the volumes follow (and prologues or oneshots that are in
-- no volume).
local function chaptersByVolume(slug, uploads, currentHids)
    local inVolumes, withoutVolume, numberedVolumes, volumeNumbers = newMap(), newMap(), {}, {}
    for _, upload in ipairs(uploads) do
        if upload.hasVolume then
            chooseUpload(inVolumes, upload, currentHids, upload.volume, upload.number)
            volumeNumbers[upload.number] = true
            if upload.number > 0 then
                numberedVolumes[upload.volume] = true
            end
        else
            chooseUpload(withoutVolume, upload, currentHids, upload.number)
        end
    end

    local chapters, volumeChapters = {}, 0
    for _, upload in ipairs(mapValues(inVolumes)) do
        if upload.number >= 1 and upload.number == math.floor(upload.number) then
            volumeChapters = volumeChapters + 1
        end
        -- a whole volume, or the prologue of a volume
        local wholeVolume = upload.isVolume and not numberedVolumes[upload.volume]
        chapters[#chapters + 1] = makeChapter(slug, upload, not wholeVolume)
    end
    for _, upload in ipairs(mapValues(withoutVolume)) do
        if upload.number > volumeChapters or upload.number < 1 and not volumeNumbers[upload.number] then
            chapters[#chapters + 1] = makeChapter(slug, upload, true)
        end
    end
    return chapters
end

-- one upload per chapter number, preferably from the group (like the chapter endpoint does)
local function onePerNumber(uploads, group)
    local entries, indices = {}, {}
    for _, upload in ipairs(uploads) do
        local i = indices[upload.rawChap]
        if not i then
            entries[#entries + 1] = upload
            indices[upload.rawChap] = #entries
        elseif entries[i].group ~= group and upload.group == group then
            entries[i] = upload
        end
    end
    return entries
end

-- The upload the chapter endpoint is requested with: its group is preferred for every chapter, the others get the
-- first upload of the chapter list. A new list prefers the group with the most chapters among the uploads (the
-- first page). An update has to find that group again, otherwise most chapters would change: it has the most of
-- the current chapters, and its uploads were chosen over other groups' at least as often as not (a group that
-- uploaded the whole series later, starting with lower numbers, only lost so far; chapters with loaded pages keep
-- their upload).
local function anchorUpload(uploads, currentHids)
    local groups, currentGroups, anyCurrent = {}, {}, false
    for _, upload in ipairs(uploads) do
        groups[upload.rawChap] = groups[upload.rawChap] or {}
        groups[upload.rawChap][upload.group] = true
        if currentHids[upload.hid] then
            currentGroups[upload.rawChap] = currentGroups[upload.rawChap] or {}
            currentGroups[upload.rawChap][upload.group] = true
            anyCurrent = true
        end
    end

    local chapters, balance = {}, {}
    for chap, chapGroups in pairs(groups) do
        if not anyCurrent then
            for group in pairs(chapGroups) do
                chapters[group] = (chapters[group] or 0) + 1
            end
        else
            local current, chosen = 0, nil
            for group in pairs(currentGroups[chap] or {}) do
                current, chosen = current + 1, group
            end
            if current == 1 then
                chapters[chosen] = (chapters[chosen] or 0) + 1
                local count = 0
                for _ in pairs(chapGroups) do
                    count = count + 1
                end
                if count > 1 then
                    for group in pairs(chapGroups) do
                        balance[group] = (balance[group] or 0) + (group == chosen and 1 or -1)
                    end
                end
            end
        end
    end

    -- the first one of the groups with the most chapters
    local anchor, best = 1, 0
    for i, upload in ipairs(uploads) do
        local count = chapters[upload.group] or 0
        if count > best and (balance[upload.group] or 0) >= 0 then
            anchor, best = i, count
        end
    end
    return uploads[anchor]
end

-- whether the numbered chapters (sorted) cover a volume: the numbers don't jump before its first and after its
-- last chapter
local function volumeCovered(entries, volume)
    local first, last
    for i, entry in ipairs(entries) do
        if entry.number > 0 and entry.knownVolume == volume then
            first = first or i
            last = i
        end
    end
    if not first then
        return false
    end

    local previous = 0
    for i = first - 1, 1, -1 do
        previous = entries[i].number
        if previous ~= 0 then
            break
        end
    end
    local next = entries[last].number + 1
    for i = last + 1, #entries do
        if entries[i].number > 0 then
            next = entries[i].number
            break
        end
    end
    return entries[first].number - previous <= 1 and next - entries[last].number <= 1
end

-- the chapters with loaded pages keep their upload if another upload of them is chosen now
local function keepLoadedUploads(chapters, current)
    -- e.g. "Ch. 12" or "Vol. 2 Ch. 3", without the title of the upload
    local function key(chapter)
        local colon = chapter.title:find(": ", 1, true)
        return colon and chapter.title:sub(1, colon - 1) or chapter.title
    end

    local keys, urls = {}, {}
    for _, chapter in ipairs(chapters) do
        keys[key(chapter)] = (keys[key(chapter)] or 0) + 1
        urls[chapter.url] = true
    end
    -- false: more than one
    local loaded = {}
    for _, chapter in ipairs(current) do
        local k = key(chapter)
        if chapter.pagesLoaded and not urls[chapter.url] and keys[k] == 1 then
            loaded[k] = loaded[k] == nil and chapter or false
        end
    end
    for _, chapter in ipairs(chapters) do
        local previous = loaded[key(chapter)]
        if previous then
            log("keeping loaded upload", previous.url, "instead of", chapter.url)
            chapter.title, chapter.url = previous.title, previous.url
        end
    end
end

-- appends the uploads of a page of the chapter list, returns the number of pages
local function fetchUploads(url, uploads)
    local response = http.get(url)
    local ok, doc = pcall(json.decode, response.body)
    if not ok or type(doc) ~= "table" or type(doc.data) ~= "table" then
        raise("parse", "Couldn't parse chapter list.")
    end
    parseUploads(doc.data, uploads)
    local lastPage = type(doc.pagination) == "table" and doc.pagination.last_page
    return math.type(lastPage) == "integer" and lastPage or 1
end

-- the chapters of the uploads, current: the chapters the app has
local function fetchChapters(slug, current)
    local listUrl = apiUrl .. "/comics/" .. slug .. "/chapter-list?lang=en&chapOrder=asc"

    -- all english uploads of all groups, sorted by chapter number (whole volumes first)
    local uploads = {}
    local lastPage = fetchUploads(listUrl, uploads)
    if #uploads == 0 then
        return {}
    end

    -- an update keeps the uploads of the current chapters where there is a choice
    local currentHids = {}
    for _, chapter in ipairs(current) do
        local last = chapter.url:match("[^/]*$")
        local cut = last:find("-chapter-", 1, true)
        currentHids[cut and last:sub(1, cut - 1) or last] = true
    end

    -- Some series start the chapter numbers again in every volume (or season), the chapter endpoint only has one
    -- upload per number. They are sorted by volume from all uploads.
    if lastPage <= maxListPages and restartedNumbers(uploads) >= 3 then
        for page = 2, lastPage do
            fetchUploads(listUrl .. "&page=" .. page, uploads)
        end
        return chaptersByVolume(slug, uploads, currentHids)
    end

    -- the chapter endpoint has the complete list of its language with one entry per chapter number, preferring the
    -- scanlation group of the requested chapter
    local entries = {}
    local anchor = anchorUpload(uploads, currentHids)
    local ok, response = pcall(http.get, apiUrl .. "/comics/" .. chapterPath(slug, anchor), {timeout = 8000})
    local loaded = ok
    if loaded then
        local decoded, doc = pcall(json.decode, response.body)
        loaded = decoded and type(doc) == "table" and type(doc.chapterList) == "table"
        if loaded then
            parseUploads(doc.chapterList, entries)
        end
    end

    if loaded then
        -- the list comes newest first (of an upload that is there twice, the first one counts)
        local newestFirst, hids = {}, {}
        for _, entry in ipairs(entries) do
            if not hids[entry.hid] then
                hids[entry.hid] = true
                newestFirst[#newestFirst + 1] = entry
            end
        end
        entries = {}
        for i = #newestFirst, 1, -1 do
            entries[#entries + 1] = newestFirst[i]
        end
    elseif lastPage == 1 and #current == 0 then
        -- All uploads are known already. An update keeps its current list instead, the choice of the uploads can
        -- differ from the chapter endpoint.
        log("chapter list from the uploads:", ok and "no list" or response.message)
        entries = onePerNumber(uploads, anchor.group)
    elseif not ok then
        error(response, 0)
    else
        raise("parse", "Couldn't parse chapter list.")
    end

    -- stable
    for i, entry in ipairs(entries) do
        entry.position = i
    end
    table.sort(entries, function(a, b)
        if a.number ~= b.number then
            return a.number < b.number
        end
        return a.position < b.position
    end)

    local volumes = newMap()
    collectVolumes(uploads, currentHids, volumes)

    -- the volumes of the numbered chapters, other uploads of a chapter often have one
    local uploadVolumes = {}
    for _, upload in ipairs(uploads) do
        if upload.knownVolume >= 0 and uploadVolumes[upload.chap] == nil then
            uploadVolumes[upload.chap] = upload.knownVolume
        end
    end

    local numbered, allVolumesKnown, maxNumber, chapterVolumes = false, true, 0, {}
    local anyChapterVolume = false
    for _, entry in ipairs(entries) do
        if entry.number > 0 then
            if entry.knownVolume < 0 then
                entry.knownVolume = uploadVolumes[entry.chap] or -1
            end
            numbered = true
            maxNumber = math.max(maxNumber, entry.number)
            if entry.knownVolume >= 0 then
                chapterVolumes[entry.knownVolume] = true
                anyChapterVolume = true
            else
                allVolumesKnown = false
            end
        end
    end

    local chapters = {}

    -- Whole volumes are uploaded as chapter 0, the list only has an arbitrary one of them. The series that are only
    -- uploaded as volumes get all of them from the pages of the chapter list (sorted by chapter, so the volumes
    -- come first). So do the series whose few chapters can't cover the volumes, e.g. only the start of the first
    -- volume.
    local volumeList = mapValues(volumes)
    if #volumeList > 0 and (not numbered or #volumeList > 1 and not anyChapterVolume and
                                maxNumber < 4 * volumes.keys[#volumes.keys][1]) then
        for page = 2, math.min(lastPage, maxListPages) do
            local fetched, err = pcall(fetchUploads, listUrl .. "&page=" .. page, uploads)
            if not fetched then
                if #current > 0 then
                    error(err, 0)
                end
                break
            end
        end
        collectVolumes(uploads, currentHids, volumes)

        for _, entry in ipairs(entries) do
            if entry.number <= 0 then
                -- e.g. oneshots
                if not entry.isVolume then
                    chapters[#chapters + 1] = makeChapter(slug, entry)
                elseif not mapGet(volumes, entry.volume) then
                    mapSet(volumes, entry, entry.volume)
                end
            end
        end
        for _, volume in ipairs(mapValues(volumes)) do
            chapters[#chapters + 1] = makeChapter(slug, volume)
        end
        return chapters
    end

    -- next to numbered chapters the volumes are dropped, except the first one (often a prologue)
    local result, listedVolumes = {}, {}
    for _, entry in ipairs(entries) do
        if not (entry.isVolume and numbered and entry.volume > 1) then
            if entry.isVolume then
                listedVolumes[entry.volume] = true
            end
            result[#result + 1] = entry
            chapters[#chapters + 1] = makeChapter(slug, entry)
        end
    end

    -- the volumes whose chapters are missing (e.g. only the first volume is translated chapter by chapter) go
    -- before the chapters of the following volumes
    if numbered and allVolumesKnown then
        for _, key in ipairs(volumes.keys) do
            local volume, upload = key[1], volumes.values[key[3]]
            if not listedVolumes[volume] and not volumeCovered(entries, volume) then
                local pos = 1
                while pos <= #result and not (result[pos].number > 0 and result[pos].knownVolume > volume) do
                    pos = pos + 1
                end
                table.insert(result, pos, upload)
                table.insert(chapters, pos, makeChapter(slug, upload))
            end
        end
    end
    return chapters
end

-- --- the info

function fetchMangaInfo(url, known)
    -- the comic pages are 150-350 kB, all infos are in them as json
    local body = http.get(url, {timeout = 6000}).body
    local first = body:find('id="comic-data"', 1, true)
    first = first and body:find(">", first, true)
    local last = first and body:find("</script>", first, true)
    if not last then
        raise("parse", "Couldn't parse manga infos.")
    end
    local ok, doc = pcall(json.decode, body:sub(first + 1, last - 1))
    if not ok or type(doc) ~= "table" then
        raise("parse", "Couldn't parse manga infos.")
    end

    local slug = str(doc, "slug")
    local info = {}
    if math.type(doc.status) == "integer" and statuses[doc.status] then
        info.status = statuses[doc.status]
    end
    if math.type(doc.year) == "integer" and doc.year > 0 then
        info.releaseYear = tostring(doc.year)
    end
    info.author = joinNames(doc, "authors")
    info.artist = joinNames(doc, "artists")

    local genres = {}
    local demographic = text.trim(str(doc, "demographic_name"))
    if demographic ~= "" then
        genres[1] = text.upper(demographic:sub(1, 1)) .. demographic:sub(2)
    end
    for _, entry in ipairs(type(doc.md_comic_md_genres) == "table" and doc.md_comic_md_genres or {}) do
        local genre = type(entry) == "table" and entry.md_genres
        local name = str(genre, "name")
        if str(genre, "group") == "Genre" and name ~= "" then
            genres[#genres + 1] = name
        end
    end
    info.genres = table.concat(genres, ", ")

    -- the description ends with a block of links to the official releases
    local summary = str(doc, "parsed")
    local rule = summary:find("<hr", 1, true)
    if rule then
        summary = summary:sub(1, rule - 1)
    end
    summary = text.trim(html.text(summary))
    if summary == "" then
        local description = str(doc, "desc")
        local cut = description:find("\n---", 1, true)
        summary = text.trim(cut and description:sub(1, cut - 1) or description)
    end
    info.summary = summary
    info.cover = text.trim(str(doc, "default_thumbnail"))

    if slug == "" then
        raise("parse", "Couldn't parse manga infos.")
    end
    info.chapters = fetchChapters(slug, known)
    keepLoadedUploads(info.chapters, known)
    return info
end

-- --- the list

-- "solo-leveling" -> "Solo Leveling"
local upperOf = {}
for letter in ("abcdefghijklmnopqrstuvwxyz"):gmatch(".") do
    upperOf[letter] = letter:upper()
end

local function titleFromSlug(slug)
    local title = slug
    -- the slugs of duplicate titles get a random suffix, e.g. "his-starN3nc", "dashu_b0" or "escape-kg4zwqyf"
    if #title > 4 and title:find("[A-Z_]", #title - 3) and not title:sub(1, -5):find("[A-Z_]") then
        title = title:sub(1, -5)
    end
    local id = title:match("%-([a-z0-9]+)$")
    if id and #id == 8 and id:find("%d") and id:find("%l") then
        title = title:sub(1, -10)
    end
    -- and some a number in front
    local digits = title:match("^(%d+)%-.")
    if digits and (#digits == 2 and digits:sub(1, 1) == "0" or #digits >= 8) then
        title = title:sub(#digits + 2)
    end

    title = title:gsub("%-+", " ")
    if title:find("^ ") or title:find(" $") then
        title = title:gsub("^ +", ""):gsub(" +$", "")
    end
    return title == "" and slug or (title:gsub("%f[^ \0]%l", upperOf))
end

local function decodeXml(s)
    if not s:find("&", 1, true) then
        return s
    end
    return (s:gsub("&apos;", "'"):gsub("&quot;", '"'):gsub("&lt;", "<"):gsub("&gt;", ">"):gsub("&amp;", "&"))
end

local function percentEncoded(s)
    return (s:gsub("[^%w%-%._~]", function(c) return ("%%%02X"):format(c:byte()) end))
end

local function addManga(state, title, url)
    local mangas = state.mangas
    mangas.titles[#mangas.titles + 1] = title
    mangas.urls[#mangas.urls + 1] = url
end

-- the pages of a search of the api, false if one failed
local function fetchSearch(order, pages, state, update, progressFrom, progressTo)
    local cursor = ""
    for page = 0, pages - 1 do
        if update.cancelled() then
            break
        end
        local searchUrl = apiUrl .. "/search?order_by=" .. order ..
                              "&order_direction=desc&type=comic&showAll=false&exclude_mylist=false"
        if cursor ~= "" then
            searchUrl = searchUrl .. "&cursor=" .. percentEncoded(cursor)
        end
        local ok, response = pcall(http.get, searchUrl, {timeout = 10000})
        if not ok then
            log("search failed:", order, response.message)
            return false
        end

        local decoded, doc = pcall(json.decode, response.body)
        if not decoded or type(doc) ~= "table" or type(doc.data) ~= "table" then
            return false
        end
        for _, r in ipairs(doc.data) do
            local slug = str(r, "slug")
            local title = text.trim(str(r, "title"))
            local url = "/comic/" .. slug
            if slug ~= "" and not state.urls[url] then
                state.urls[url] = true
                if title == "" then
                    title = update.knownTitle(url) or titleFromSlug(slug)
                end
                addManga(state, title, url)
            end
        end
        cursor = str(doc, "next_cursor")
        update.progress(progressFrom + (progressTo - progressFrom) * (page + 1) // pages)
        if cursor == "" then
            break
        end
    end
    return true
end

local sitemaprx = re.compile([=[<loc>\s*([^<\s]*/sitemap-comic-(\d+)\.xml)\s*</loc>]=])

-- the slugs of the sitemaps, a retry goes on with the ones that are missing
local function fetchSitemaps(state, update)
    local index = http.get(source.baseUrl .. "/sitemap.xml", {timeout = 10000}).body
    local urls, numbers = {}, {}
    for url, number in sitemaprx:gmatch(index) do
        number = tonumber(number)
        if not urls[number] then
            numbers[#numbers + 1] = number
        end
        urls[number] = url
    end
    if #numbers == 0 then
        raise("parse", "Couldn't parse manga list.")
    end
    table.sort(numbers)

    local done = 0
    for _ in pairs(state.sitemapsDone) do
        done = done + 1
    end
    for _, number in ipairs(numbers) do
        if not state.sitemapsDone[number] then
            local body = http.get(urls[number], {timeout = 30000}).body
            if update.cancelled() then
                raise("cancelled", "Download cancelled")
            end
            local entries = 0
            for loc in body:gmatch("<loc>(.-)</loc>") do
                local at = loc:find("/comic/", 1, true)
                if at then
                    local slug = loc:sub(at + 7)
                    if slug:find("^%s") or slug:find("%s$") then
                        slug = text.trim(slug)
                    end
                    slug = decodeXml(slug)
                    local url = "/comic/" .. slug
                    if slug ~= "" and not slug:find("/", 1, true) and not state.urls[url] then
                        state.urls[url] = true
                        addManga(state, update.knownTitle(url) or titleFromSlug(slug), url)
                    end
                end
                -- (lets the GUI go on)
                entries = entries + 1
                if entries % 1000 == 0 then
                    update.progress(math.min(99, 20 + 80 * done // #numbers))
                end
            end
            state.sitemapsDone[number] = true
            done = done + 1
            update.progress(math.min(99, 20 + 80 * done // #numbers))
        end
    end
end

function fetchMangaList(update)
    -- a retry goes on with the sitemaps the failed attempt didn't get
    local state = update.state
    if not state.mangas then
        state.mangas, state.urls, state.searched, state.sitemapsDone = {titles = {}, urls = {}}, {}, false, {}
    end
    update.progress(update.lastProgress)
    local start = clock()

    -- the most popular and the newest titles come from the search api (real titles), the rest of the catalogue
    -- from the sitemaps (only slugs)
    if not state.searched then
        -- the order of the list is the popularity, a retry searches again
        local ok = fetchSearch("user_follow_count", popularPages, state, update, 0, 18)
        ok = ok and not update.cancelled() and fetchSearch("created_at", latestPages, state, update, 18, 20)
        if update.cancelled() then
            raise("cancelled", "Download cancelled")
        end
        if not ok then
            raise("failed", "Couldn't load the popular titles.")
        end
        state.searched = true
    end

    fetchSitemaps(state, update)
    log("mangas:", #state.mangas.urls, "time:", clock() - start)
    update.progress(100)
    return {titles = state.mangas.titles, urls = state.mangas.urls}
end

-- --- the pages

-- the next page of each page of the page lists, for warming up the following pages
local nextPages, nextPagesCount = {}, 0
-- the requests that get the next pages into the cdn's cache, by page
local pageWarmUps = {}

-- Images that are not in the cloudflare cache yet are often answered with 429 for a few seconds (the origin is
-- throttled for all users). HEAD requests (retried while throttled) pull the image into the cache without
-- transferring it, so the download afterwards is served from there.
local function startCacheWarmUp(imageUrl, timeout)
    return http.start(imageUrl, {
        head = true,
        timeout = timeout,
        transferTimeout = 2000,
        retryDelay = 400,
        maxRetryDelay = 400,
        retries = timeout // 400,
        headers = {["User-Agent"] = userAgent, Referer = "https://comick.art/"},
    })
end

function fetchPageList(chapterUrl)
    local response = http.get(apiUrl .. "/comics/" .. chapterUrl, {timeout = 8000})
    local ok, doc = pcall(json.decode, response.body)
    if not ok or type(doc) ~= "table" or type(doc.chapter) ~= "table" then
        raise("parse", "Couldn't parse page list.")
    end

    local urls = {}
    for _, image in ipairs(type(doc.chapter.images) == "table" and doc.chapter.images or {}) do
        local imageUrl = str(image, "url")
        if imageUrl ~= "" then
            urls[#urls + 1] = imageUrl
        end
    end

    if nextPagesCount > 5000 then
        nextPages, nextPagesCount = {}, 0
    end
    for i = 1, #urls - 1 do
        if nextPages[urls[i]] == nil then
            nextPagesCount = nextPagesCount + 1
        end
        nextPages[urls[i]] = urls[i + 1]
    end
    return urls
end

function resolveImage(page)
    -- the following pages are usually requested next
    local next = page.url
    for _ = 1, prefetchPages do
        next = nextPages[next]
        if not next then
            break
        end
        if not pageWarmUps[next] then
            pageWarmUps[next] = startCacheWarmUp(next, 8000)
        end
    end

    local warmUp = pageWarmUps[page.url]
    pageWarmUps[page.url] = nil
    if not warmUp or warmUp:failed() then
        warmUp = startCacheWarmUp(page.url, 5000)
    end
    pcall(warmUp.wait, warmUp, 5000)

    -- the warm-ups of skipped pages
    local count = 0
    for _ in pairs(pageWarmUps) do
        count = count + 1
    end
    if count > 32 then
        for url, request in pairs(pageWarmUps) do
            if request:done() then
                pageWarmUps[url] = nil
            end
        end
    end
    return page
end
