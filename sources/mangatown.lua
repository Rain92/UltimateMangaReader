local spanrx = re.compile([[<span>([^<]*)</span>]])

-- "Vol 01 Title 5: Subtitle": the plain spans after the link of a chapter are its volume and its subtitles
function chapterTitle(chapter)
    local volume, subtitles = nil, {}
    for span in spanrx:gmatch(chapter.text) do
        span = common.text(span)
        if not volume and #subtitles == 0 and span:find("^Vol%s+%S+$") then
            volume = span
        elseif span ~= "" then
            subtitles[#subtitles + 1] = span
        end
    end
    local title = volume and volume .. " " .. chapter.title or chapter.title
    return #subtitles > 0 and title .. ": " .. table.concat(subtitles, " ") or title
end
