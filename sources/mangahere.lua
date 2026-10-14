-- The pages of a chapter come from the mobile long strip reader (all images on one page), else from the chapter
-- page of the site: one of its packed scripts has the images, or the key for chapterfun.ashx, whose packed script
-- has the image of a page.

local mobileUrl = "https://m.mangahere.cc"
local fanfoxUrl = "https://fanfox.net"

local lazyrx = re.compile([=[<img class="lazy" data-original="([^"]*)"]=])
local rollPagesrx = re.compile([=[<div class="roll-page">\d+/(\d+)</div>]=])
local blockedrx = re.compile([=[(?s)<p class="detail-block-content">(.*?)</p>]=])
local packedrx = re.compile([=[(?s)eval\(function\(p,a,c,k,e,d\).*?</script>]=])
local quotedrx = re.compile([=['([^']*)']=])
local imageInfosrx = re.compile([=[newImginfos=\[([^\]]*)\]]=])
local chapterIdrx = re.compile([=[var chapterid\s*=\s*(\d+)]=])
local imageCountrx = re.compile([=[var imagecount\s*=\s*(\d+)]=])
local pixrx = re.compile([=[var pix="([^"]*)"]=])
local pvaluerx = re.compile([=[var pvalue=\["([^"]*)"]=])

-- The images of the mobile reader, nil if it doesn't have all of them (e.g. of licensed and adult titles).
local function mobileImages(chapterUrl)
    local path = chapterUrl:match("/manga/(.+/)[^/]*$")
    if not path then
        return nil
    end
    local ok, response = pcall(http.get, mobileUrl .. "/roll_manga/" .. path, {timeout = 4000, retries = 0})
    if not ok then
        log("The mobile reader failed:", response.message)
        return nil
    end
    local images = {}
    for url in lazyrx:gmatch(response.body) do
        images[#images + 1] = common.url(response.url, url)
    end
    if #images > 0 and tonumber(rollPagesrx:match(response.body) or "") == #images then
        return images
    end
end

-- The pages of the chapter page of the site, or of fanfox.net, which often still has the blocked chapters.
local function sitePages(chapterUrl, timeout)
    local response = http.get(chapterUrl, {timeout = timeout})
    local body = response.body

    local blocked = blockedrx:match(body)
    if blocked then
        if chapterUrl:sub(1, #source.baseUrl) == source.baseUrl then
            local ok, pages = pcall(sitePages, fanfoxUrl .. chapterUrl:sub(#source.baseUrl + 1), 4000)
            if ok then
                return pages
            end
        end
        raise("notfound", common.text(blocked))
    end

    local key = ""
    for packed in packedrx:gmatch(body) do
        local script = common.unpackJs(packed) or ""
        local images = script:find("newImgs=", 1, true)
        if images then
            -- a long strip: the images are in the page
            local urls = {}
            for url in quotedrx:gmatch(script, images, script:find("];", images, true)) do
                urls[#urls + 1] = common.url(response.url, url)
            end
            -- the last one is an advertisement that has the id of the one before it
            local infos = {}
            for info in (imageInfosrx:match(script) or ""):gmatch("([^,]*),?") do
                infos[#infos + 1] = info
            end
            local n = #urls
            if n > 1 and #infos == n and infos[n] == infos[n - 1] then
                urls[n] = nil
            end
            if #urls > 0 then
                return urls
            end
        elseif script:find("guidkey", 1, true) then
            for part in quotedrx:gmatch(script, 1, script:find(";", 1, true)) do
                key = key .. part
            end
        end
    end

    local chapterId = chapterIdrx:match(body)
    local count = tonumber(imageCountrx:match(body) or "")
    if not chapterId or not count then
        raise("parse", "Couldn't read the pages.")
    end
    -- the last page is an advertisement
    if count > 1 then
        count = count - 1
    end
    local pageUrl = response.url:match("^.*/") .. "chapterfun.ashx?cid=" .. chapterId .. "&page="
    local pages = {}
    for n = 1, count do
        pages[n] = pageUrl .. n .. "&key=" .. key
    end
    return pages
end

function fetchPageList(chapterUrl)
    local start = clock()
    -- both readers together shouldn't take much longer than one request
    return mobileImages(chapterUrl) or sitePages(chapterUrl, math.max(4000, 8000 - (clock() - start)))
end

function resolveImage(page)
    local url = page.url
    if not url:find("/chapterfun.ashx", 1, true) then
        return page
    end
    local start = clock()
    for _ = 1, 3 do
        if clock() - start >= 5000 then
            break
        end
        local body = http.get(url, {timeout = 6000 - (clock() - start)}).body
        if body == "" then
            -- it's empty at times, then once more without the key
            url = url:gsub("&key=.*$", "&key=")
        else
            local script = common.unpackJs(body) or ""
            local pix, pvalue = pixrx:match(script), pvaluerx:match(script)
            if not pix or not pvalue then
                break
            end
            return common.url(url, pix .. pvalue)
        end
    end
    raise("parse", "Couldn't read the image.")
end
