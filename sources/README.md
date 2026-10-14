# Sources

The sources of the app: for each one a definition (`<id>.yaml`), a script if it needs one (`<id>.lua`) and its
logo (`<id>.png`). `common.lua` is shared: it runs the definitions and has helpers for the scripts. `index.yaml`
lists the sources and the libraries with their versions; the app compares it with the index of the repository to
find updates.

After a change, write the index (it checks that every source loads and counts up the versions of the changed
ones), then compare with the C++ source (the reference, see `tests/README.md`):

    harness index sources
    tests/run.sh MangaTown-lua

## The definition

The simple sources are only a definition: where the pages are, and regular expressions for what is on them.
Everything else goes into the script (see "Scripts" below).

```yaml
name: MangaTown                         # shown in the app, identifies the source (cache, favorites)
baseUrl: https://www.mangatown.com      # the site, without a / at the end
headers:                                # for the requests to the domains and their subdomains
  mangahere.org: {Referer: 'https://www.mangatown.com/'}
cookies:                                # for the domains and their subdomains
  mangahere.cc: {isAdult: '1'}

list: ...                               # the list of all mangas
info: ...                               # the info of a manga and its chapters
pages: ...                              # the pages of a chapter
image: ...                              # the image of a page of the site (if the pages aren't images)
```

**Patterns** are regular expressions (Perl syntax), in single quotes so that nothing needs escaping (only a `'`
is written `''`). `(?s)` at the start lets `.` match line breaks too. The value of a pattern is its first group,
or the whole match without groups. A list of patterns means: the first one that matches.

**Fields** are values on a page: a pattern (or a list of them), or `{match: pattern, remove: pattern, lua:
function}`. `remove` removes what it matches from the value. `lua` names a function of the script that makes the
value: it gets the item (the fields so far, `text`: the html of the item) and the page (`url`, `body`) and returns
the value. The values are converted by their name:

| field                          | value                                                                     |
|--------------------------------|---------------------------------------------------------------------------|
| `url`, `cover`                 | resolved against the url of the page (`//host/x`, `/x`, `x`), `&amp;` is `&` |
| `author`, `artist`             | the texts of the links (each once, `remove` from each), else the text     |
| `genres`                       | the text, with the separators as spaces                                   |
| `popularity`                   | a number, the higher the more popular                                     |
| the others (`title`, `status`, ...) | the text of the html (entities, tags and whitespace like a browser) |

**Item lists** (`mangas`, `chapters`, `images`) are the repeated parts of a page: an item for each match of
`each`. Its named groups (`(?<url>...)`) are fields of the item, the other fields are searched in the text of the
item (from its match to the next one). `within: [start, end]` limits the items to the part of the page from the
first match of `start` to the first match of `end` after it (without `end`: to the end of the page). A page with
the start but without the end is incomplete (cut off) and fails.

**Pages** that a section loads can be checked: `expect` (a pattern the page must have, else it fails),
`complete` (a pattern a page has when it wasn't cut off) and `notFound` (its text is the message of a manga or
chapter that isn't there, e.g. "has been licensed": when `expect` fails or the chapter list isn't there).
`timeout` is in ms.

### list

```yaml
list:
  url: /directory/{page}.htm?name.az    # with {page}: pages from 1 until one brings no new mangas
  pageSize: 30                          # a page with fewer mangas may be the last one
  estimatedPages: 800                   # for the progress
  timeout: 30000
  mangas:
    each: '<a class="manga_cover" href="(?<url>/manga/[^"]*?)" title="(?<title>.*?)">'
    popularity: 'Views:\s*(\d+)'        # else the order of the list
```

A list on one page has no `{page}` in its `url`. It can have popular pages, whose mangas come first (in their
order), the rest of the list after them:

```yaml
list:
  url: /mangalist/
  complete: '<div class="footer-bar">'
  mangas:
    each: '<p class="browse-new-block-content"><a href="(?<url>/manga/[^"]*)" title="(?<title>[^"]*)">'
  popular:
    url: /directory/{page}.htm
    pages: 10
    mangas:
      each: '<a href="(?<url>/manga/[^"]*)" title="(?<title>[^"]*)"><img class="manga-list-1-cover"'
```

### info

```yaml
info:
  expect: 'class="detail_info'
  notFound: '>([^<>]*has been licensed[^<>]*)<'
  author: '(?s)<b>Author\(s\):</b>(.*?)</?li'      # also artist, status, genres, summary, cover, releaseYear
  summary:
    match: '(?s)<span id="show"[^>]*>(.*?)</span>'
    remove: '\[.*?\]'
  chapters:
    within: ['<ul class="chapter_list">', '</ul>']
    each: '<a href="(?<url>/manga/[^"]*)"[^>]*>(?<title>[^<]*)</a>'
    title: {lua: chapterTitle}
    order: newestFirst                  # as on the page, the app wants the oldest first
```

### pages, image

```yaml
pages:
  expect: '<div id="viewer"'
  images:                               # images on the page (e.g. a long strip): they are the pages
    within: ['<div id="viewer"', '<div class="manga_read|\z']
    each: '<img src="(?<url>[^"]*)" class="image"'
  count: 'var total_pages\s*=\s*(\d+)'  # else the number of pages (the first above 0)
  page: '{chapter}/{n}.html'            # and their urls ({chapter}: the url of the chapter, without a / at the end)

image:
  when: '\.html$'                       # the pages it is for, the others are images already
  within: ['<div id="viewer"']
  url: ['<img src="([^"]*)"[^>]*id="image"', '<img[^>]*src="([^"]*)"']
```

## Scripts

`<id>.lua` runs after `common.lua`. It defines the functions that fields name with `lua:`, and it can replace
any of the calls of the app (then the section isn't needed): `fetchMangaList(update)`, `fetchMangaInfo(url,
known)`, `fetchPageList(chapterUrl)`, `resolveImage(page)`. What they get and return, and what scripts can use
(`http`, `re`, `html`, `json`, `raise`, ...), is documented in `src/mangasources/luasource.h` and
`src/scripting/lualibs.h`. A script can also call the ones of the definition (`common.fetchMangaInfo(url)`, ...)
and change what they return.

Helpers in `common.lua`: `common.text(html)`, `common.url(base, url)`, `common.names(html, remove)`,
`common.fill(template, values)`, `common.find(patterns, page, init, limit)`.

Keep the scripts short: what more than one source needs belongs into `common.lua` (or, for work on large data,
into the app as a library function).
