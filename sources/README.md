# Sources

The sources of the app: for each one a definition (`<id>.yaml`), a script if it needs one (`<id>.lua`) and its
logo (`<id>.png`). `common.lua` is shared: it runs the definitions and has helpers for the scripts. `index.yaml`
lists the sources and the libraries with their versions; the app compares it with the index of the repository to
find updates (see "The index" below).

After a change, count up the minor version of the source in the index, then check the index (all sources load,
and the entries that differ from the ones on `master` have newer versions there) and compare with the C++ source
(the reference, see `tests/README.md`):

    tests/run.sh index MangaTown-lua

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
images: {retries: 6, retryDelay: 1000}  # the downloads of the images (pages): attempts after errors that may go
                                        # away (429, 5xx, ...), the pause before one (ms, doubles up to
                                        # maxRetryDelay); default 2, 1000, 4000

list: ...                               # the list of all mangas
info: ...                               # the info of a manga and its chapters
pages: ...                              # the pages of a chapter
image: ...                              # the image of a page of the site (if the pages aren't images)
```

**Patterns** are regular expressions (Perl syntax), in single quotes so that nothing needs escaping (only a `'`
is written `''`). `(?s)` at the start lets `.` match line breaks too. The value of a pattern is its first group,
or the whole match without groups. A list of patterns means: the first one that matches.

**Fields** are values on a page: a pattern (or a list of them), or `{match: pattern, remove: pattern, lines:
true, lua: function}`. `remove` removes what it matches from the value. `lines: true` keeps the line breaks of the
html (of a text that has them instead of `<br>`). `lua` names a function of the script that makes the value: it
gets the item (the fields so far, `text`: the html of the item) and the page (`url`, `body`) and returns the value.
The values are converted by their name:

| field                          | value                                                                     |
|--------------------------------|---------------------------------------------------------------------------|
| `url`, `cover`                 | resolved against the url of the page (`//host/x`, `/x`, `x`), `&amp;` is `&` |
| `author`, `artist`, `genres`   | the texts of the links (each once, `remove` from each) joined by `, `, else the text |
| `popularity`                   | a number, the higher the more popular                                     |
| `title`                        | the text (entities, whitespace collapsed), a `<` is no tag (e.g. in an attribute) |
| the others (`status`, ...)     | the text of the html (entities, tags and whitespace like a browser)       |

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

Helpers in `common.lua`: `common.text(html)`, `common.title(text)`, `common.url(base, url)`,
`common.names(html, remove)`, `common.fill(template, values)`, `common.find(patterns, page, init, limit)`,
`common.unpackJs(script)` (the script of a packed `eval(function(p,a,c,k,e,d){...})`), `common.imageSizes(urls,
timeout)` (from the starts of the files), `common.waitFor(request, update)` (a request of a list update, which
fails when the update is cancelled). `common.fetchMangaInfo(url)` also returns the page (`url`, `body`), for a
script that wants more of it.

Keep the scripts short: what more than one source needs belongs into `common.lua` (or, for work on large data,
into the app as a library function).

A script runs in the GUI thread: while it runs, the GUI waits. It lets the GUI go on when it waits (`http.get`,
`request:wait`, `sleep`), and in a list update `update.progress(percent)` does too if the script ran for a while.
So a long loop of a list update (over tens of thousands of mangas) reports its progress every few hundred items. A
script that runs for 10 s without waiting is stopped (on a Kobo, scripts run about 30 times slower than on a PC).

## The index

```yaml
libraries:                              # run before the scripts, in this order
  - id: common                          # the file is <id>.lua
    version: 1.0
sources:                                # in the order the app shows them
  - id: mangatown                       # the base name of its files
    name: MangaTown                     # the name of its definition (shown in the update prompt)
    version: 1.3
    files: [mangatown.yaml, mangatown.lua, mangatown.png]
```

The `version` is `major.minor`, compared number by number (1.10 is newer than 1.9):

- The **minor** version goes up with every change of the files. The app installs an entry when the repository has
  a newer version of it, with all of its files, so without a new version the apps keep the old files.
- The **major** version is the one of the app that runs the entry (`LuaSource::majorVersion`). An app only runs and
  installs the entries of its major version: the repository's entries of another major version are left alone (not
  installed, and not removed). So a breaking change of the app counts its major version up, and its bundled sources
  go to 2.0: until the repository has the 2.x versions, the new app gets no updates, and once it has them, the older
  apps keep the 1.x versions they have. A new app replaces the installed sources of an older major version with its
  bundled ones; the ones it doesn't bundle wait (not shown) for their update in the repository.

A source that the repository doesn't have anymore is removed from the app (after asking).
