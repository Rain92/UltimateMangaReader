-- The list comes from the sitemaps (the paged lists allow ~20 requests a minute), the chapters from the api of the
-- site. The site cuts the images taller than 1500 px into a piece of 1500 px and the rest, the pages are joined
-- again.

local mirrorrx = re.compile([=[^https?://(?:www\.)?(?:mangakakalove\.com|mangakakalot\.gg|manganato\.gg|natomanga\.com|nelomanga\.(?:net|com))/]=])
local sitemaprx = re.compile([=[<loc>\s*(https?://[^<\s]*/sitemap-comic-\d+\.xml)\s*</loc>]=])
local slugrx = re.compile([=[<loc>\s*https?://[^/<]+/manga/([^/<\s]+)\s*</loc>]=])
local popularrx = re.compile([=[<h3>\s*<a href="[^"]*/manga/([^/"]+)"\s*title="([^"]*)"]=])
local comicSlugrx = re.compile([=[data-comic-slug="([^"]+)"]=])
local entityrx = re.compile([=[&(?:quot|amp|lt|gt|#0?39);]=])
local cdnrx = re.compile([=[cdns\s*=\s*\[\s*"([^"]+)"]=])
local imagesrx = re.compile([=[chapterImages\s*=\s*\[([^\]]*)\]]=])
local quotedrx = re.compile([=["([^"]+)"]=])
local imgrx = re.compile([=[<img\s+src=['"]([^'"]+)['"]]=])

-- --- the list

-- the url on this mirror: the mirrors share the catalogue and the slugs
local function onThisMirror(url)
    local _, last = mirrorrx:find(url)
    return last and source.baseUrl .. "/" .. url:sub(last + 1) or url
end

local upperOf = {}
for letter in ("abcdefghijklmnopqrstuvwxyz"):gmatch(".") do
    upperOf[letter] = letter:upper()
end

-- "solo-leveling" -> "Solo Leveling", a double dash stands for a dash in the title. Made for most of the list on
-- the first update, so it is written for speed (a Kobo needs ~25 µs).
local function titleFromSlug(slug)
    local dashes = slug:find("--", 1, true)
    local title = dashes and slug:gsub("%-%-+", "\1") or slug
    title = title:gsub("%-", " "):gsub("%f[^ \1\0]%l", upperOf)
    if title:find("^[ \1]") or title:find("[ \1]$") then
        title = title:gsub("^[ \1]+", ""):gsub("[ \1]+$", "")
    end
    return dashes and (title:gsub("\1", " - ")) or title
end

function fetchMangaList(update)
    local state = update.state
    local sitemaps = {}
    for url in sitemaprx:gmatch(http.get(source.baseUrl .. "/sitemap.xml", {timeout = 7000}).body) do
        sitemaps[#sitemaps + 1] = onThisMirror(url)
    end
    if #sitemaps == 0 then
        raise("parse", "Couldn't find the manga sitemaps.")
    end
    update.progress(5)

    -- the sitemaps have no titles: the ones of the popular pages or of the last list are taken, the others are
    -- made from the slug. A popular page that fails only costs the order (and is a message).
    local message
    if not state.popularDone then
        state.popular = {}
        for page = 1, 2 do
            if update.cancelled() then
                break
            end
            local url = source.baseUrl .. "/genre/all?type=topview&state=all&page=" .. page
            local ok, response = pcall(http.get, url, {timeout = 5000, retries = 0})
            if not ok then
                log("A popular page failed:", response.message)
                message = "Couldn't load the popular titles: " .. response.message
                break
            end
            for slug, title in popularrx:gmatch(response.body) do
                state.popular[#state.popular + 1] = {slug = slug, title = common.title(title)}
            end
        end
        state.popularDone = not update.cancelled()
    end

    local slugs, ordered, titles = {}, {}, {}
    local function add(slug)
        if slug ~= "" and not slugs[slug] then
            slugs[slug] = true
            ordered[#ordered + 1] = slug
        end
    end
    for _, manga in ipairs(state.popular) do
        if manga.title ~= "" then
            titles[manga.slug] = manga.title
        end
        add(manga.slug)
    end
    update.progress(10)

    -- two at a time, a retry gets the ones the failed attempt didn't
    state.sitemapSlugs = state.sitemapSlugs or {}
    local missing = {}
    for _, url in ipairs(sitemaps) do
        if not state.sitemapSlugs[url] then
            missing[#missing + 1] = url
        end
    end
    local done = #sitemaps - #missing
    common.downloadAll(missing, update, function(k, response)
        local found = {}
        for slug in slugrx:gmatch(response.body) do
            found[#found + 1] = slug
        end
        state.sitemapSlugs[missing[k]] = found
        done = done + 1
        update.progress(10 + 85 * done // #sitemaps)
        log("sitemap", missing[k], "matches:", #found)
        return true
    end, {timeout = 60000})
    -- (the progress lets the GUI go on in the long loops)
    for _, url in ipairs(sitemaps) do
        for _, slug in ipairs(state.sitemapSlugs[url]) do
            add(slug)
        end
        update.progress(95)
    end

    -- the site has duplicate entries of some works ("x-s-y" / "xs-y", "x--y" / "x-y", "slow-start" /
    -- "slow-startt") with different chapters, all of them stay
    local list = {titles = {}, urls = {}}
    for i, slug in ipairs(ordered) do
        local url = "/manga/" .. slug
        local title = titles[slug] or update.knownTitle(url)
        if not title then
            -- the site appends an id to the slugs of different works with the same name
            local name, id
            if slug:find("_", 1, true) then
                name, id = slug:match("^(.+)_(%d+)$")
            end
            title = titleFromSlug(name or slug)
            if name and slugs[name] then
                title = title .. " (" .. id .. ")"
            end
        end
        list.titles[i], list.urls[i] = title, url
        if i % 500 == 0 then
            update.progress(95 + 4 * i // #ordered)
        end
    end
    log("mangas:", #list.urls)
    update.progress(100)
    return list, message
end

-- --- the info

-- some summaries have their entities twice (&amp;quot;)
function summary(info)
    return info.summary and entityrx:find(info.summary) and common.text(info.summary) or info.summary
end

-- a page of the chapter list of the api (newest first), started
local function chapterPage(slug, offset)
    return http.start(source.baseUrl .. "/api/manga/" .. slug .. "/chapters?limit=-1&offset=" .. offset,
                      {timeout = 8000})
end

-- The chapters of the api, oldest first. request: its first page if it was started already.
local function chapters(slug, request)
    local list, known, offset = {}, {}, 0
    for _ = 1, 10 do
        local data = common.json((request or chapterPage(slug, offset)):wait().body)
        request = nil
        local items = data and data.success == true and type(data.data) == "table" and data.data.chapters
        if type(items) ~= "table" then
            raise("parse", "Couldn't load the chapter list.")
        end
        for _, chapter in ipairs(items) do
            if type(chapter) == "table" and type(chapter.chapter_slug) == "string" then
                local url = source.baseUrl .. "/manga/" .. slug .. "/" .. chapter.chapter_slug
                if not known[url] then
                    known[url] = true
                    local title = type(chapter.chapter_name) == "string" and common.title(chapter.chapter_name) or ""
                    if title == "" and type(chapter.chapter_num) == "number" then
                        title = "Chapter " .. string.format("%g", chapter.chapter_num)
                    end
                    list[#list + 1] = {title = title ~= "" and title or "Chapter", url = url}
                end
            end
        end
        offset = offset + #items
        local pagination = data.data.pagination
        if type(pagination) ~= "table" or pagination.has_more ~= true or #items == 0 then
            break
        end
    end
    return common.reverse(list)
end

function fetchMangaInfo(url)
    -- the first page of the chapters loads meanwhile
    local urlSlug = url:match("^%a+://[^/]+/manga/([^/?#]+)/?$")
    local request = urlSlug and chapterPage(urlSlug, 0)
    local info, page = common.fetchMangaInfo(url)
    local slug = comicSlugrx:match(page.body)
    info.chapters = chapters(slug, slug == urlSlug and request or nil)

    -- some mangas have only the host of the cdn as cover, the site shows its placeholder then
    if not info.cover or not info.cover:find("^%a+://[^/?#]+/[^?#]") then
        info.cover = source.baseUrl .. "/images/404-avatar.webp"
    end
    -- The covers are on several hosts of the cdn and some are missing on the one the site links (503 for good,
    -- like some pages), imgs-2 had all of them so far.
    local scheme, host, path = info.cover:match("^(%a+://)([^/]+)(/thumb/.*)$")
    if host and host:find("%.2xstorage%.com$") then
        local other = host == "imgs-2.2xstorage.com" and "img-r2.2xstorage.com" or "imgs-2.2xstorage.com"
        info.coverFallbacks = {scheme .. other .. path}
    end
    return info
end

-- --- the pages

local pieceHeight = 1500

-- The pages of the images: {piece, rest} where an image is cut into a piece and the rest, {image} for the others.
-- sizes: {width, height} or false (unknown) by image.
local function pairSplitImages(sizes)
    local n = #sizes
    local function known(i) return i >= 1 and i <= n and sizes[i] ~= false end
    local function width(i) return sizes[i][1] end
    local function isPiece(i) return known(i) and sizes[i][2] == pieceHeight end
    -- the middle and the end of long strips follow a piece of the same width
    local function continuesStrip(i) return isPiece(i - 1) and width(i - 1) == width(i) end
    -- the rest of the image of a piece: together they have the proportions of a page, or the rest is short (also
    -- at the end of a long strip)
    local function isRestOf(rest, pieceWidth, ofStrip)
        if not known(rest) or width(rest) ~= pieceWidth or sizes[rest][2] >= pieceHeight then
            return false
        end
        local height = sizes[rest][2]
        return height <= pieceHeight // 4 or (not ofStrip and pieceHeight + height <= 2 * pieceWidth)
    end

    local pieces, pairCount, unknown = 0, 0, 0
    for i = 1, n do
        if not known(i) then
            unknown = unknown + 1
        elseif isPiece(i) then
            pieces = pieces + 1
            if not continuesStrip(i) and isRestOf(i + 1, width(i)) then
                pairCount = pairCount + 1
            end
        end
    end
    -- Some images are missing at the source (they always fail). Where most pages are a piece and a rest, a missing
    -- one is taken as the part its neighbours lack, the page shows the other part only then.
    local pagesSplit = pairCount > 0 and pairCount * 2 >= pieces and unknown * 4 <= n
    -- the width of the closest piece, for a missing piece
    local function pieceWidthNear(i)
        for d = 1, n - 1 do
            for _, j in ipairs({i - d, i + d}) do
                if isPiece(j) then
                    return width(j)
                end
            end
        end
        return -1
    end

    local pages, i = {}, 1
    while i <= n do
        local paired = false
        if isPiece(i) then
            local strip = continuesStrip(i)
            local nextIsPieceOfMissingOne = not known(i + 1) and isRestOf(i + 2, pieceWidthNear(i + 1))
            paired = isRestOf(i + 1, width(i), strip) or
                     (pagesSplit and not strip and i + 1 <= n and not known(i + 1) and not nextIsPieceOfMissingOne)
        elseif pagesSplit and not known(i) then
            paired = isRestOf(i + 1, pieceWidthNear(i))
        end
        pages[#pages + 1] = paired and {i, i + 1} or {i}
        i = i + (paired and 2 or 1)
    end
    return pages
end

-- The images joined into pages again (the rest of a piece is its appendUrl, stitched below it).
local function joinSplitPages(urls)
    local start = clock()
    -- The sizes that didn't come in time are asked for once more: the list is stored with the chapter, pages that
    -- weren't joined because of a slow connection would stay that way.
    local sizes, timedOut = common.imageSizes(urls, 8000)
    if #timedOut > 0 then
        local again = {}
        for k, i in ipairs(timedOut) do
            again[k] = urls[i]
        end
        local more, stillTimedOut = common.imageSizes(again, 8000)
        for k, i in ipairs(timedOut) do
            sizes[i] = more[k]
        end
        if #stillTimedOut * 4 > #urls then
            raise("timeout", "The pages of the chapter couldn't be checked, the connection is too slow.")
        end
    end

    local pages, unknown = {}, 0
    for _, size in ipairs(sizes) do
        unknown = unknown + (size and 0 or 1)
    end
    for k, images in ipairs(pairSplitImages(sizes)) do
        pages[k] = images[2] and {url = urls[images[1]], appendUrl = urls[images[2]]} or urls[images[1]]
    end
    log("Page sizes probed in", clock() - start, "ms:", #urls, "images,", unknown, "unknown ->", #pages, "pages")
    return pages
end

function fetchPageList(chapterUrl)
    local body = http.get(chapterUrl, {timeout = 7000}).body
    local urls = {}

    local cdn, images = cdnrx:match(body), imagesrx:match(body)
    if cdn and images then
        cdn = cdn:gsub("\\", ""):gsub("/+$", "")
        for path in quotedrx:gmatch(images) do
            path = path:gsub("\\", "")
            urls[#urls + 1] = path:find("^http") and path or cdn .. "/" .. path:gsub("^/", "")
        end
    end
    if #urls == 0 then
        local first = body:find('class="container-chapter-reader"', 1, true)
        if first then
            for url in imgrx:gmatch(body, first, body:find("</div>", first, true)) do
                urls[#urls + 1] = url
            end
        end
    end
    if #urls == 0 then
        raise("parse", "Couldn't read the pages.")
    end
    return joinSplitPages(urls)
end
