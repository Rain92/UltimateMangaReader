# Tests

A headless harness (`harness/`) that runs the core of the app on the desktop:

- **Sources:** what each source of `../sources/` (definitions and Lua scripts, see `../sources/README.md`) makes of a few fixed mangas (`picks/<source>.txt`). This covers the info, the first and last chapters (and a hash of all of them and the summary), the page lists, the image links with their headers, and the first image after processing (its size and a hash of its pixels). It also covers the update of the manga list (`<source>-list`) for the sources where that takes few requests. The golden output was first written by the C++ sources the scripts replaced (they were the reference until all sources were scripts); where a script differs on purpose, its golden output was written by the script: `golden/MangaHere-list.txt` has the titles with their whitespace collapsed (20 of them had double spaces), and the genres of MangaHere and MangaTown are lists joined by ", ".
- **Cases:** made up responses of what the sites rarely have (`harness/cases.py`, `<source>-cases`): restarted and reset volumes, inserted chapters, anthologies, error popups, expired pages, failures. They are made again for every run (`fixtures/` isn't in the repository); their golden output was written by the C++ sources too. The cases of MangaPlus also load the pages (`CASES_WITH_PAGES` in `run.sh`).
- **jobs:** the contract of the download jobs and of the coroutines that wait for them (`src/task.h`), against a local test server (`harness/testserver.py`).
- **storage:** the cache layout, mangas and chapters by URL, the reading progress, the favorites, the sources (enabled, current, the list model of the dialogs), the manga lists and their updates, and the repository (page images, joined updates, failed updates), against the same server and a fake source.
- **scripts:** the Lua engine (the sandbox, the limits on memory and on running without waiting, errors), its libraries (regular expressions with UTF-8 positions, text, html, json, protobuf), the sources of scripts (http, sleep, the conversions of what they return, the state of a list update for a retry, loading, the index and the registry) and the definitions (`common.lua`: every section against a small site of the test server, and the checks when a definition is loaded), against the same server.
- **index:** `../sources/index.yaml` lists all files of `../sources/`, and its sources load with the names it has for them. An entry whose files differ from the published ones (`origin/master` of the repository of the sources, the submodule `../sources/`; `SOURCES_BASE=<git ref>` for another ref of it) must have a newer version than there.

```
tests/run.sh                    # all tests
tests/run.sh MangaDex storage   # some of them
tests/run.sh record MangaDex    # record the responses of MangaDex again
```

`run.sh` builds the harness in `build/tests` of the repository. It uses `qmake6`; set `QMAKE` to use another one. The output, logs and diffs end up in `output/` there.

## Recorded responses

The source tests don't ask the sites. The harness answers every request of the app from the responses that were recorded in `fixtures/<test>/`: an `index.json` plus the bodies. All requests go through `NetworkAccessManager`, which lets the harness answer them. The output of a test is compared with `golden/<test>.txt`. A test fails if the output differs, or if the app makes a request that wasn't recorded (shown in the log).

`tests/run.sh record` asks the sites again. It replaces the recorded responses and writes the golden output from them. Only record with code that is known to work. The golden output then changes because the sites changed, and the diff of `golden/` shows how. (`record <source>-cases` writes the golden output of the cases with the sources as they are.)

To compare a change of a source with what it did before on many mangas, record a capture with the old one and replay it with the new one: `HARNESS_LIST_DUMP=<file>` writes all entries of a list update, `HARNESS_ALL_CHAPTERS=1` makes `capture` print all chapters, `HARNESS_REFETCH=1` loads each info a second time with the chapters as the known ones (like the update of a favorite), and `HARNESS_NO_PAGES=1` loads only the infos and chapters.

A change in behaviour that is intended updates the golden output in the same commit:

```
cp build/tests/output/<test>.txt tests/golden/
```

The recorded responses are not committed (`.gitignore`), because they contain the pages and images of the sites. They only exist where they were recorded. Without them, `run.sh` tells you to record them.

## The harness

```
harness [--cache <dir>] [--record <dir> | --replay <dir>] [--sources <dir>] <command>
  capture <source> <picks file>
  list <source>
  pick <source>                    # updates the list, prints mangas for a picks file
  bench <source> <picks file> [rounds] [image rounds]   # the time of reading, best with --replay
  download <source> <title> <url> [chapters]   # the download manager, against the sites
  flow <source> <title> <url>      # reading, favorites and a chapter download through the core
  listflow <source>                # list updates and a cancelled one through the core
  jobs <port> | storage <port> | scripts <port>
  index <dir> [<base dir>]         # checks the index of a directory of sources
```

Without `--record` or `--replay` the harness asks the sites. The sources are the ones of `sources/` of the repository (its path is built into the harness); `--sources` uses another directory of sources (e.g. a copy on the Kobo, or an older version to compare with).

`index` checks `index.yaml` of a directory of sources: it lists all files of the directory, the files it lists are there, and every source loads with the name the index has for it. With a base directory (e.g. the published sources) it also checks that every entry whose files differ from the ones there has a newer version than there: the app only installs an entry with a newer version.

The harness also builds for the Kobo (`qmake harness.pro CONFIG+=kobo CONFIG+=release`), to time the replays there. It needs the offscreen platform plugin (`QT_QPA_PLATFORM_PLUGIN_PATH`), the recorded responses next to it and `--sources` with a copy of `sources/` (the path built into it is the one of the desktop).

`bench` times what reading asks of a source, for the mangas of a picks file: opening a manga (its info), opening a chapter (its page list), resolving a page and downloading and processing its image. It prints the median of each. With `--replay` the responses come without waiting, so the times are those of the app alone, e.g. to compare two versions of a source (`--sources`):

```
./harness --replay ../../tests/fixtures/MangaTown bench MangaTown ../../tests/picks/MangaTown.txt
```

`flow` and `listflow` drive `UltimateMangaReaderCore` the way the GUI does. They only run against the sites and check each step, with no golden output. Run them after changes to the core, for example with the first line of a picks file:

```
cd build/tests
./harness flow MangaTown "Tales of Demons and Gods" https://www.mangatown.com/manga/tales_of_demons_and_gods/
./harness listflow MangaPlus
```

Comick's CDN answers 429 for a few seconds until an image is in its cache: its images are retried for up to 31 s (`images` in `sources/comick.yaml`), a chapter download that still loses one says so.
