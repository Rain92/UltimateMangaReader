-- The api of MANGA Plus, its answers are protobuf messages (proto.decode). The list is the titles of all
-- languages' english versions, the hottest and the completed ones first. The images are encrypted (XOR) and their
-- urls expire.

-- the urls of the mangas and chapters are relative to the api, like the ones of the app before the scripts
local api = source.baseUrl .. "/"
local mangalistUrl = "api/title_list/allV2"
local rankingUrl = "api/title_list/rankingV2?lang=eng&type={type}&clang=eng"
local titleDetailUrl = "api/title_detailV3?title_id={id}&clang=eng"
local pagesUrl = "api/manga_viewer_v3?chapter_id={id}&split=yes&img_quality=super_high&clang=eng"

-- like the web client: without a session token the api answers with an "Account Banned" error
local headers = {["SESSION-TOKEN"] = ("%08x-%04x-4%03x-%04x-%06x%06x"):format(math.random(0, 0xffffffff),
    math.random(0, 0xffff), math.random(0, 0xfff), 0x8000 + math.random(0, 0x3fff), math.random(0, 0xffffff),
    math.random(0, 0xffffff))}

-- --- the messages

local function has(message, number)
    return message[number] ~= nil
end

-- the first value of the field that is a number (0 without one)
local function int(message, number)
    for _, value in ipairs(message[number] or {}) do
        if math.type(value) == "integer" then
            return value
        end
    end
    return 0
end

-- the first value of the field that is a string ("" without one)
local function bytes(message, number)
    for _, value in ipairs(message[number] or {}) do
        if type(value) == "string" then
            return value
        end
    end
    return ""
end

local function decoded(message, number)
    return proto.decode(bytes(message, number))
end

-- all the messages of the field
local function messages(message, number)
    local list = {}
    for _, value in ipairs(message[number] or {}) do
        if type(value) == "string" then
            list[#list + 1] = proto.decode(value)
        end
    end
    return list
end

--    message Response {
--      SuccessResult success = 1;
--      ErrorResult error = 2;   // ErrorResult { Popup englishPopup = 2; } Popup { subject = 1; body = 2; }
--    }
local function successOf(body)
    local response = proto.decode(body)
    if has(response, 1) then
        return decoded(response, 1)
    end
    local popup = decoded(decoded(response, 2), 2)
    local subject, text_ = text.trim(bytes(popup, 1)), text.trim(bytes(popup, 2))
    if subject == "" and text_ == "" then
        raise("parse", "Couldn't parse MangaPlus response.")
    end
    raise("failed", (subject == "" or text_ == "") and subject .. text_ or subject .. ": " .. text_)
end

local function get(url, timeout)
    return successOf(http.get(url, {timeout = timeout, headers = headers}).body)
end

-- --- the list

--    message AllTitlesViewV2 { repeated AllTitlesGroup groups = 1; }
--    message AllTitlesGroup { string theTitle = 1; repeated Title titles = 2; }   // one per language
--    message Title {
--      uint32 titleId = 1;
--      string name = 2;
--      string author = 3;
--      string portraitImageUrl = 4;
--      int32 language = 7;   // 0 = english (omitted)
--    }
function fetchMangaList(update)
    local response = http.get(api .. mangalistUrl, {timeout = 20000, headers = headers})
    if update.cancelled() then
        raise("cancelled", "Download cancelled")
    end
    update.progress(40)

    local ids, titles = {}, {}
    for _, group in ipairs(messages(decoded(successOf(response.body), 25), 1)) do
        for _, title in ipairs(messages(group, 2)) do
            local id, name = int(title, 1), text.trim(bytes(title, 2))
            if int(title, 7) == 0 and id ~= 0 and name ~= "" and not titles[id] then
                ids[#ids + 1] = id
                titles[id] = name
            end
        end
    end
    if #ids == 0 then
        raise("parse", "Couldn't parse the manga list.")
    end

    -- the order of the popularity: the hottest and the completed rankings first, then the rest from A to Z
    --    message TitleRankingViewV2 { repeated TitleRankingGroup rankedTitles = 3; }
    --    message TitleRankingGroup { uint32 originalTitleId = 1; repeated Title titles = 2; }
    local ordered, added, progress = {}, {}, 40
    -- a ranking that couldn't be loaded only costs the order
    local rankingError
    for _, kind in ipairs({"hottest", "completed"}) do
        local ok, ranking = pcall(get, api .. common.fill(rankingUrl, {type = kind}), 10000)
        if update.cancelled() then
            raise("cancelled", "Download cancelled")
        end
        progress = progress + 25
        update.progress(progress)
        if not ok then
            local message = type(ranking) == "table" and ranking.message or tostring(ranking)
            log("couldn't load the ranking", kind, message)
            rankingError = "Couldn't load the ranking: " .. message
        else
            for _, group in ipairs(messages(decoded(ranking, 37), 3)) do
                for _, title in ipairs(messages(group, 2)) do
                    local id = int(title, 1)
                    if titles[id] and not added[id] then
                        added[id] = true
                        ordered[#ordered + 1] = id
                    end
                end
            end
        end
    end
    for _, id in ipairs(ids) do
        if not added[id] then
            ordered[#ordered + 1] = id
        end
    end

    local list = {titles = {}, urls = {}}
    for i, id in ipairs(ordered) do
        list.titles[i], list.urls[i] = titles[id], "/" .. common.fill(titleDetailUrl, {id = id})
    end
    update.progress(100)
    return list, rankingError
end

-- --- the info

-- the chapter names are "#001", "ex" or "One-shot", the subtitles usually have the number
local function chapterTitle(name, subTitle)
    local number, title = text.trim(name), text.trim(subTitle)
    if title == "" then
        return number
    end
    local digits = number:match("^#0*(%d+)$")
    if digits and not title:find("%f[%d]" .. digits .. "%f[%D]") then
        return number .. " - " .. title
    end
    return title
end

local function titleStatus(detail, genres, chapterCount)
    -- TitleLabels.releaseSchedule: 1-6 regular releases, 7 other, 8 completed, 9 one-shot, 10 hiatus
    local schedule = int(decoded(detail, 32), 1)
    local oneShot = false
    for _, genre in ipairs(genres) do
        oneShot = oneShot or text.lower(genre) == "one-shot"
    end

    if schedule == 8 then
        return "Completed"
    elseif schedule == 9 or oneShot then
        return "One-shot"
    elseif schedule == 10 then
        return "Hiatus"
    elseif schedule >= 1 and schedule <= 6 then
        return "Ongoing"
    end
    local nonAppearanceInfo = text.lower(bytes(detail, 8))
    if nonAppearanceInfo:find("complete", 1, true) then
        return "Completed"
    elseif nonAppearanceInfo:find("hiatus", 1, true) then
        return "Hiatus"
    end
    -- the award titles have no release schedule
    return chapterCount == 1 and "One-shot" or ""
end

--    message TitleDetailView {
--      Title title = 1;
--      string titleImageUrl = 2;
--      string overview = 3;
--      string viewingPeriodDescription = 7;
--      string nonAppearanceInfo = 8;
--      repeated ChapterGroup chapterListGroup = 28;
--      repeated Tag tags = 31;   // Tag { string tag = 1; string slug = 2; }
--      TitleLabels titleLabels = 32;   // TitleLabels { int32 releaseSchedule = 1; }
--    }
--    message ChapterGroup {
--      string chapterNumbers = 1;
--      repeated Chapter firstChapterList = 2;
--      repeated Chapter midChapterList = 3;   // only readable with a subscription
--      repeated Chapter lastChapterList = 4;
--    }
--    message Chapter {
--      uint32 titleId = 1;
--      uint32 chapterId = 2;
--      string name = 3;
--      string subTitle = 4;
--      uint32 startTimeStamp = 6;
--      uint32 endTimeStamp = 7;
--    }
function fetchMangaInfo(url)
    -- the api server is far away (~300 ms rtt), a new connection often takes longer than 2 s
    local detail = decoded(get(url, 6000), 8)
    local title = decoded(detail, 1)
    if not has(title, 1) then
        raise("parse", "Couldn't parse manga info.")
    end

    local info, authors = {}, {}
    for author in bytes(title, 3):gmatch("[^/]+") do
        author = text.trim(author)
        if author ~= "" then
            authors[#authors + 1] = author
        end
    end
    info.author = table.concat(authors, ", ")
    info.artist = info.author

    local genres = {}
    for _, tag in ipairs(messages(detail, 31)) do
        local genre = text.trim(bytes(tag, 1))
        if genre ~= "" then
            genres[#genres + 1] = genre
        end
    end
    info.genres = table.concat(genres, ", ")

    local summary = {}
    for _, field in ipairs({3, 8, 7}) do
        local part = text.trim(bytes(detail, field))
        if part ~= "" and not part:find("latest 0 chapters", 1, true) then
            summary[#summary + 1] = part
        end
    end

    info.cover = bytes(title, 4)
    if info.cover == "" then
        info.cover = bytes(detail, 2)
    end

    local chapters, allChapters, chapterCount, subscriptionChapters = {}, {}, 0, false
    for _, group in ipairs(messages(detail, 28)) do
        for _, list in ipairs({2, 3, 4}) do
            for _, chapter in ipairs(messages(group, list)) do
                local id = int(chapter, 2)
                if not allChapters[id] then
                    allChapters[id] = true
                    chapterCount = chapterCount + 1
                end
                if list == 3 then
                    subscriptionChapters = true
                else
                    chapters[#chapters + 1] = chapter
                end
            end
        end
    end
    info.status = titleStatus(detail, genres, chapterCount)

    info.chapters = {}
    local seen = {}
    for _, chapter in ipairs(chapters) do
        local id = int(chapter, 2)
        -- the expired chapters have no subtitle
        if id ~= 0 and has(chapter, 4) and not seen[id] then
            seen[id] = true
            info.chapters[#info.chapters + 1] = {title = chapterTitle(bytes(chapter, 3), bytes(chapter, 4)),
                                                 url = common.fill(pagesUrl, {id = id})}
        end
    end

    if #info.chapters == 0 and subscriptionChapters then
        summary[#summary + 1] = "The chapters of this title are only readable with a MANGA Plus subscription."
    end
    info.summary = table.concat(summary, "\n\n")
    return info
end

-- --- the pages

-- the download manager resolves the images of all chapters before it downloads them
local usableImageMargin = 600
-- the image host only accepts the view token of the fetch the image urls came from
local viewTokenHeader = "Plus-Vw-Token"
-- the view tokens of this session
local viewTokens = {}

local function urlExpiry(url)
    local expires = url:match("[?&]expires=(%d+)")
    return expires and tonumber(expires) or math.maxinteger
end

local function hexBytes(hex)
    local key = {}
    for i = 1, #hex, 2 do
        key[#key + 1] = string.char((tonumber(hex:sub(i, i + 1), 16) or 0) % 256)
    end
    return table.concat(key)
end

-- The image urls expire one to two hours after they were loaded (at the start of the hour after next), and their
-- view token is only sent in the session that loaded them.
local function isUsableImage(image)
    local token = image.headers and image.headers[viewTokenHeader] or ""
    return urlExpiry(image.url) > time() + usableImageMargin and (token == "" or viewTokens[token] ~= nil)
end

--    message MangaViewer {
--      repeated Page pages = 1;
--      uint32 chapterId = 2;
--      string vwToken = 19;
--    }
--    message Page { MangaPage mangaPage = 1; ... }   // the other fields are banners and ads
--    message MangaPage {
--      string imageUrl = 1;
--      uint32 width = 2;
--      uint32 height = 3;
--      string encryptionKey = 5;
--    }
function fetchPageList(chapterUrl)
    -- the chapter urls are relative to the api
    local url = chapterUrl:find("^http") and chapterUrl or api .. chapterUrl
    local viewer = decoded(get(url, 7000), 10)

    local token = bytes(viewer, 19)
    if token ~= "" then
        viewTokens[token] = true
    end

    local images = {}
    for _, page in ipairs(messages(viewer, 1)) do
        local mangaPage = decoded(page, 1)
        local imageUrl = bytes(mangaPage, 1)
        if imageUrl ~= "" then
            local image = {url = imageUrl, xorKey = hexBytes(bytes(mangaPage, 5)), headers = {}}
            if token ~= "" then
                image.headers[viewTokenHeader] = token
            end
            -- only used in this session (the view token), the image is resolved again before it expires
            local expiry = urlExpiry(imageUrl)
            if expiry ~= math.maxinteger then
                image.expires = expiry - usableImageMargin
            end
            images[#images + 1] = image
        end
    end
    return images
end

-- the page lists that were loaded again, by chapter: {time, images}
local refreshedPageLists, refreshedCount = {}, 0
-- the refreshes that wait for their page list, by chapter
local runningRefreshes = {}

local function findImage(images, fileName)
    for _, image in ipairs(images or {}) do
        if image.url:find(fileName, 1, true) then
            return image
        end
    end
end

local pagerx = re.compile([=[/chapter/(\d+)/manga_page/(?:[^/?#]*/)*([^/?#]+)\?]=])

-- The pages of the page list are its images. The expired ones and the ones loaded in an earlier session are
-- replaced by the images of a fresh page list of their chapter.
function resolveImage(page)
    local chapterId, file = pagerx:match(page.url)
    if not chapterId or isUsableImage(page) then
        return page
    end
    local fileName = "/" .. file .. "?"

    if refreshedCount > 16 and not refreshedPageLists[chapterId] then
        refreshedPageLists, refreshedCount = {}, 0
    end
    local refreshed = refreshedPageLists[chapterId] or {time = 0}
    local image = findImage(refreshed.images, fileName)

    -- At most one request per chapter and minute: neither a failing network nor a wrong clock of the device may
    -- cause a request (and a timeout) per page. The pages that are needed while the request runs make their own
    -- one (they can't wait for it).
    local now = time()
    if (not image or not isUsableImage(image)) and (now - refreshed.time >= 60 or runningRefreshes[chapterId]) then
        if not refreshedPageLists[chapterId] then
            refreshedCount = refreshedCount + 1
        end
        refreshedPageLists[chapterId] = {time = now, images = refreshed.images}

        runningRefreshes[chapterId] = (runningRefreshes[chapterId] or 0) + 1
        local ok, images = pcall(fetchPageList, common.fill(pagesUrl, {id = chapterId}))
        runningRefreshes[chapterId] = runningRefreshes[chapterId] > 1 and runningRefreshes[chapterId] - 1 or nil
        if not ok then
            error(images, 0)
        end

        if not refreshedPageLists[chapterId] then
            refreshedCount = refreshedCount + 1
        end
        refreshedPageLists[chapterId] = {time = now, images = images}
        image = findImage(images, fileName)
    end

    if not image then
        raise("failed", "Couldn't refresh the expired page url.")
    end
    return image
end
