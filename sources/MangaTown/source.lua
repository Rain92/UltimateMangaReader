-- MangaTown: the pages of the site (html). Image hosts: zjcdn.mangahere.org (pages), fmcdn.mangahere.com (covers),
-- they want the Referer of the manifest.

local directoryUrl = "https://www.mangatown.com/directory/"

local function absoluteImageUrl(url)
    if url:sub(1, 2) == "//" then
        return "https:" .. url
    end
    return url
end

local linkrx = re.compile([[<a[^>]*>([^<]*)</a>]])

local function joinLinkTexts(fragment)
    local names, seen = {}, {}
    for name in linkrx:gmatch(fragment) do
        name = text.trim(name)
        if name ~= "" and not seen[name] then
            seen[name] = true
            names[#names + 1] = name
        end
    end
    return #names == 0 and fragment or table.concat(names, ", ")
end

local licensedrx = re.compile([[>([^<>]*has been licensed[^<>]*)<]])

local function licensedMessage(page)
    local message = licensedrx:match(page)
    return message and text.trim(html.text(message)) or ""
end

-- the pages of the directory (A-Z, the popularity comes from the view counts)

local mangarx = re.compile([[<a class="manga_cover" href="(/manga/[^"]*?)" title="(.*?)">]])
local viewsrx = re.compile([[Views:\s*(\d+)]])
local matchesPerPage = 30

function fetchMangaList(update)
    -- the pagination links stop at page 334 but the pages behind them are served as well (~800 pages in 2026),
    -- so keep requesting pages while they are full of new mangas
    local estimatedPages = math.max(800, (update.currentSize + matchesPerPage - 1) // matchesPerPage)
    local maxPages = 3 * estimatedPages

    -- a retry continues with the pages the failed attempt didn't get
    local state = update.state
    if not state.nextPage then
        state.nextPage = 1
        state.titles, state.urls, state.views, state.known = {}, {}, {}, {}
        state.queued, state.shortPages, state.refetched = {}, {}, {}
        state.processed = 0
    end

    update.progress(update.lastProgress)

    -- parallel to state.queued, taken in order: first to last
    local requests, first, last = {}, 1, 0
    local function requestPage(url)
        state.queued[#state.queued + 1] = url
        last = last + 1
        requests[last] = http.start(url, {timeout = 30000})
    end
    local function requestNextPage()
        if state.nextPage > maxPages then
            return
        end
        requestPage(directoryUrl .. state.nextPage .. ".htm?name.az")
        state.nextPage = state.nextPage + 1
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
        local page = request:wait().body

        if update.cancelled() then
            raise("cancelled", "Download cancelled")
        end

        requests[first] = nil
        first = first + 1
        local pageUrl = table.remove(state.queued, 1)

        local oldSize = #state.titles
        local subject = re.text(page)
        local matches = mangarx:all(subject)
        for i, match in ipairs(matches) do
            local url = match[1]
            if not state.known[url] then
                state.known[url] = true

                local nextMatch = matches[i + 1]
                local views = viewsrx:match(subject, match.last + 1, nextMatch and nextMatch.first)

                -- the html converter is slow and only a few titles have entities. It collapses whitespace like
                -- simplify
                local title = match[2]
                if title:find("&", 1, true) then
                    title = text.trim(html.text(title))
                else
                    title = text.simplify(title)
                end

                state.titles[#state.titles + 1] = title
                state.urls[#state.urls + 1] = url
                state.views[#state.views + 1] = views and tonumber(views) or 0
            end
        end

        if #matches >= matchesPerPage and #state.titles > oldSize then
            requestNextPage()

            -- the short pages before this one weren't the end: fetch them once more
            for _, url in ipairs(state.shortPages) do
                state.refetched[url] = true
                requestPage(url)
            end
            state.shortPages = {}
        else
            log("Incomplete page:", pageUrl, #matches, #state.titles - oldSize)
            if not state.refetched[pageUrl] then
                state.shortPages[#state.shortPages + 1] = pageUrl
            end
        end

        state.processed = state.processed + 1
        update.progress(math.min(99, 10 + 90 * state.processed // estimatedPages))
    end

    local size = #state.titles
    if size == 0 then
        -- nothing worth resuming, a retry starts over
        update.clearState()
        raise("parse", "Couldn't parse manga list.")
    end

    log("pages:", state.nextPage - 1, "mangas:", size)
    update.progress(100)

    return {titles = state.titles, urls = state.urls, popularity = state.views, absoluteUrls = false}
end

-- the page of a manga

local authorrx = re.compile([[<b>Author\(s\):</b>(.*?)</?li]], "s")
local artistrx = re.compile([[<b>Artist\(s\):</b>(.*?)</?li]], "s")
local statusrx = re.compile([[<b>Status\(s\):</b>\s*([^<&]*)]])
local genresrx = re.compile([[<b>Genre\(s\):</b>(.*?)</li>]], "s")
local summaryrx = re.compile(
    [[<span id="show"[^>]*>(.*?)(?:&nbsp;)?\s*(?:<a class="more"[^>]*>[^<]*</a>\s*)?</span>]], "s")
local coverrx = re.compile([[<div class="detail_info[^"]*">\s*<img src="([^"]*)"]])
local chapterrx = re.compile([[<li>(.*?)</li>]], "s")
local chapterlinkrx = re.compile([[<a href="(/manga/[^"]*)"[^>]*>([^<]*)</a>]])
local chapterspanrx = re.compile([[<span>([^<]*)</span>]])
local volumerx = re.compile([[^Vol\s+\S+$]])
local bbrx = re.compile([=[\[.*?\]]=])

function fetchMangaInfo(url)
    -- info pages of long series are large (~350 KB) and the server sometimes takes > 1 s to answer
    local page = http.get(url).body
    if not page:find('class="detail_info', 1, true) then
        raise("parse", "Couldn't parse manga infos.")
    end
    local subject = re.text(page)

    local info = {}
    local author = authorrx:match(subject)
    if author then
        info.author = text.trim(html.text(joinLinkTexts(author)))
    end
    local artist = artistrx:match(subject)
    if artist then
        info.artist = text.trim(html.text(joinLinkTexts(artist)))
    end
    local status = statusrx:match(subject)
    if status then
        info.status = text.trim(html.text(status))
    end
    local genres = genresrx:match(subject)
    if genres then
        info.genres = html.genres(genres)
    end
    local summary = summaryrx:match(subject)
    if summary then
        info.summary = text.trim(bbrx:replace(html.text(summary), ""))
    end
    info.cover = absoluteImageUrl(coverrx:match(subject) or "")

    local spos = page:find('<ul class="chapter_list">', 1, true)
    if not spos then
        local message = licensedMessage(subject)
        if message ~= "" then
            raise("notfound", message)
        end
        return info
    end
    -- a page that was cut off would give some of the chapters only
    local epos = page:find("</ul>", spos, true)
    if not epos then
        raise("parse", "Couldn't parse the chapter list.")
    end

    -- newest first on the page
    local newestFirst, known = {}, {}
    for _, match in ipairs(chapterrx:all(subject, spos, epos)) do
        local li = match[1]
        local link, linkTitle = chapterlinkrx:match(li)
        local chapterUrl = link and source.baseUrl .. link
        if chapterUrl and not known[chapterUrl] then
            known[chapterUrl] = true

            local volume, subtitles = "", {}
            for span in chapterspanrx:gmatch(li) do
                local spanText = text.trim(html.text(span))
                if volume == "" and #subtitles == 0 and volumerx:match(spanText) then
                    volume = spanText
                elseif spanText ~= "" then
                    subtitles[#subtitles + 1] = spanText
                end
            end

            local title = text.trim(html.text(linkTitle))
            if volume ~= "" then
                title = volume .. " " .. title
            end
            if #subtitles > 0 then
                title = title .. ": " .. table.concat(subtitles, " ")
            end
            newestFirst[#newestFirst + 1] = {title = title, url = chapterUrl}
        end
    end

    local chapters = {}
    for i = #newestFirst, 1, -1 do
        chapters[#chapters + 1] = newestFirst[i]
    end
    info.chapters = chapters
    return info
end

-- the pages of a chapter: the images of a long strip, else the pages of the site (resolveImage)

local totalpagesrx = re.compile([[var total_pages\s*=\s*(\d+)]])
local numpagesrx = re.compile(
    [[>(\d+)</option>\s*?(?:<option\s*?value="/manga/[^"]*?"\s*?>Featured</option>)?\s*?</select>]])
local stripimagerx = re.compile([[<img src="([^"]*)" class="image"]])

function fetchPageList(chapterUrl)
    local response = http.get(chapterUrl, {timeout = 7000})
    local page = response.body

    local vpos = page:find('<div id="viewer"', 1, true)
    if not vpos then
        local message = licensedMessage(page)
        if message ~= "" then
            raise("notfound", message)
        end
        raise("parse", "Couldn't process pagelist.")
    end
    local subject = re.text(page)

    -- long strip chapters show all images on one page
    local epos = page:find('<div class="manga_read', vpos, true)
    local images = {}
    for image in stripimagerx:gmatch(subject, vpos, epos) do
        images[#images + 1] = absoluteImageUrl(image)
    end
    if #images > 0 then
        return images
    end

    local numPages = tonumber(totalpagesrx:match(subject) or "") or 0
    if numPages <= 0 then
        numPages = tonumber(numpagesrx:match(subject) or "") or 0
    end
    if numPages <= 0 then
        raise("parse", "Couldn't process pagelist.")
    end

    local baseChapterUrl = response.url
    if baseChapterUrl:sub(-1) ~= "/" then
        baseChapterUrl = baseChapterUrl .. "/"
    end
    local pages = {}
    for i = 1, numPages do
        pages[i] = baseChapterUrl .. i .. ".html"
    end
    return pages
end

local imageurlrx = re.compile([[<img src="([^"]*)"[^>]*id="image"]])
local anyimageurlrx = re.compile([[<img[^>]*src="([^"]*)"]])

function resolveImage(page)
    if page.url:sub(-5) ~= ".html" then
        return page
    end

    local body = http.get(page.url).body
    local subject = re.text(body)
    local vpos = body:find('<div id="viewer"', 1, true) or 1

    local url = imageurlrx:match(subject, vpos) or anyimageurlrx:match(subject, vpos)
    if not url then
        raise("parse", "Couldn't process pages/images.")
    end
    return absoluteImageUrl(url)
end
