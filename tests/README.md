# Tests

A headless harness (`harness/`) that runs the core of the app on the desktop:

- **Sources:** what each source makes of a few fixed mangas (`picks/<source>.txt`). This covers the info, the first and last chapters, the page lists, the image links with their headers, and the first image after processing (its size and a hash of its pixels). It also covers the update of the manga list for the sources where that takes few requests.
- **jobs:** the contract of the download jobs, against a local test server (`harness/testserver.py`).
- **storage:** the cache layout, mangas and chapters by URL, the reading progress, the favorites and the image links, against the same server.

```
tests/run.sh                    # all tests
tests/run.sh MangaDex storage   # some of them
tests/run.sh record MangaDex    # record the responses of MangaDex again
```

`run.sh` builds the harness in `../build-UltimateMangaReader-tests` (next to the repository). It uses `qmake6`; set `QMAKE` to use another one. The output, logs and diffs end up in `output/` there.

## Recorded responses

The source tests don't ask the sites. The harness answers every request of the app from the responses that were recorded in `fixtures/<test>/`: an `index.json` plus the bodies. All requests go through `NetworkAccessManager`, which lets the harness answer them. The output of a test is compared with `golden/<test>.txt`. A test fails if the output differs, or if the app makes a request that wasn't recorded (shown in the log).

`tests/run.sh record` asks the sites again. It replaces the recorded responses and writes the golden output from them. Only record with code that is known to work. The golden output then changes because the sites changed, and the diff of `golden/` shows how.

A change in behaviour that is intended updates the golden output in the same commit:

```
cp ../build-UltimateMangaReader-tests/output/<test>.txt tests/golden/
```

The recorded responses are not committed (`.gitignore`), because they contain the pages and images of the sites. They only exist where they were recorded. Without them, `run.sh` tells you to record them.

## The harness

```
harness [--cache <dir>] [--record <dir> | --replay <dir>] <command>
  capture <source> <picks file>
  list <source>
  pick <source>                    # updates the list, prints mangas for a picks file
  download <source> <title> <url> [chapters]   # the download manager, against the sites
  jobs <port> | storage <port>
```

Without `--record` or `--replay` the harness asks the sites.
