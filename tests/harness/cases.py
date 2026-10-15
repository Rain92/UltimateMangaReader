"""The responses of cases that the sites rarely have, as fixtures for the harness (tests/run.sh <Source>-cases).

    python3 -I cases.py <Comick|MangaDex> <fixtures dir> <picks file>

The C++ source gives the golden output with them, the script of the source has to give the same
(<Source>-cases-lua). The data is made up, only its form is the one of the sites.
"""
import hashlib
import json
import os
import shutil
import sys


class Fixtures:
    def __init__(self):
        self.responses = {}
        self.bodies = {}

    def add(self, url, body, status=200, content_type='application/json'):
        data = body if isinstance(body, bytes) else (
            body.encode() if isinstance(body, str) else json.dumps(body, ensure_ascii=False).encode())
        name = hashlib.sha1(data).hexdigest()
        self.bodies[name] = data
        response = {'status': status, 'headers': [['content-type', content_type]], 'body': name}
        if status >= 400:
            response['error'] = 203 if status == 404 else 299
            response['errorString'] = f'Error transferring {url} - server replied with status code {status}'
        self.responses['GET ' + url] = response

    def write(self, directory):
        shutil.rmtree(directory, ignore_errors=True)
        os.makedirs(os.path.join(directory, 'bodies'))
        for name, data in self.bodies.items():
            with open(os.path.join(directory, 'bodies', name), 'wb') as f:
                f.write(data)
        with open(os.path.join(directory, 'index.json'), 'w') as f:
            json.dump(self.responses, f, indent=1, sort_keys=True)


# --- Comick

COMICK = 'https://comick.art'


def upload(hid, chap='', vol=None, title=None, group='Alpha', lang='en'):
    u = {'hid': hid, 'chap': chap, 'lang': lang, 'group_name': group if isinstance(group, list) else [group]}
    if vol is not None:
        u['vol'] = vol
    if title is not None:
        u['title'] = title
    return u


def comick_case(f, slug, info, pages, endpoint=None, endpoint_status=200):
    """pages: the uploads of the pages of the chapter list, endpoint: the chapter list of the chapter endpoint
    (newest first), answered for every upload."""
    data = dict(slug=slug, **info)
    html = ('<html><head><title>x</title></head><body><script id="comic-data" type="application/json">' +
            json.dumps(data, ensure_ascii=False) + '</script></body></html>')
    f.add(f'{COMICK}/comic/{slug}', html, content_type='text/html')
    list_url = f'{COMICK}/api/comics/{slug}/chapter-list?lang=en&chapOrder=asc'
    for i, uploads in enumerate(pages):
        f.add(list_url + ('' if i == 0 else f'&page={i + 1}'),
              {'data': uploads, 'pagination': {'current_page': i + 1, 'last_page': len(pages)}})
    for uploads in pages:
        for u in uploads:
            if not u['hid']:
                continue
            chap = str(u['chap']).strip() or '0'
            url = f"{COMICK}/api/comics/{slug}/{u['hid']}-chapter-{chap}-{u['lang'] or 'en'}"
            if endpoint_status != 200:
                f.add(url, 'not found', status=endpoint_status, content_type='text/plain')
            else:
                f.add(url, {'chapter': {'hid': u['hid'], 'images': []}, 'chapterList': endpoint or []})


def comick(f):
    picks = []

    def case(slug, *args, **kwargs):
        comick_case(f, slug, *args, **kwargs)
        picks.append((slug, f'{COMICK}/comic/{slug}'))

    # the chapter numbers start again in every volume: one upload per volume and number
    case('case-restarted', {'status': 2, 'year': 2020, 'demographic_name': 'shounen',
                            'authors': [{'name': ' A '}, {'name': 'A'}, {'name': 'B'}], 'artists': [{'name': 'C'}],
                            'md_comic_md_genres': [{'md_genres': {'name': 'Action', 'group': 'Genre'}},
                                                   {'md_genres': {'name': 'Ghosts', 'group': 'Theme'}},
                                                   {'md_genres': None}, {'other': 1}],
                            'parsed': '<p>The <b>story</b>.</p><hr><p>Links</p>', 'desc': 'unused',
                            'default_thumbnail': ' https://cdn.example/cover.webp '},
         [[upload('r1', '1', '1'), upload('r2', '2', '1'), upload('r3', '3', '1'),
           upload('r4', '1', '2'), upload('r5', '2', '2'), upload('r6', '3', '2', 'Finale'),
           upload('r7', '0', '1', 'Prologue'), upload('r8', '', '3'), upload('r9', '2', '1', group='Beta'),
           upload('r10', '7'), upload('r11', '5'), upload('r12', '0.5'), upload('r13', '2', '1', lang='fr'),
           upload('', '9')],
          [upload('r14', '4', '2'), upload('r15', 'None', '4'), upload('r16', '4', '2', group='Beta')]])

    # only whole volumes (and a oneshot): all of them from all pages, in the order of their numbers
    case('case-volumes', {'status': 9, 'year': 0, 'parsed': '<hr>', 'desc': 'Old volumes.\n---\nLinks'},
         [[upload('v1', '', '1'), upload('v2', '0', '2'), upload('v3', '', '2', group='Beta'),
           upload('v4', '', None, 'Special')],
          [upload('v5', '', '3'), upload('v6', '', '10')]],
         [upload('v4', '', None, 'Special'), upload('v1', '', '1')])

    # a few chapters without volumes next to whole volumes: the volumes
    case('case-few', {'status': 1},
         [[upload('f1', '1'), upload('f2', '2'), upload('f3', '3'), upload('f4', '', '1'), upload('f5', '', '2'),
           upload('f6', '', '3')]],
         [upload('f3', '3'), upload('f2', '2'), upload('f1', '1'), upload('f4', '', '1')])

    # the volumes whose chapters are missing are inserted, the covered ones aren't
    case('case-insert', {'status': 4},
         [[upload('i1', '1', '1'), upload('i2', '2', '1'), upload('i3', '3', '1'), upload('i4', '10', '3'),
           upload('i5', '11', '3'), upload('i6', '12'), upload('i6b', '12', '3', group='Beta'),
           upload('iv1', '', '1'), upload('iv2', '', '2'), upload('iv4', '0', '4'), upload('iv5', '', '5'),
           upload('i7', '13', '5'), upload('i8', '14', '5'), upload('i9', '15', '5')]],
         [upload('i9', '15', '5'), upload('i8', '14', '5'), upload('i7', '13', '5'), upload('i6', '12'),
          upload('i5', '11', '3'), upload('i4', '10', '3'), upload('i3', '3', '1'), upload('i3', '3', '1'),
          upload('i2', '2', '1'), upload('i1', '1', '1'), upload('iv1', '', '1')])

    # the chapter endpoint fails: the uploads of the one page, preferring the group with the most chapters
    case('case-fallback', {},
         [[upload('a1', '1'), upload('b1', '1', group='Beta'), upload('b2', '2', group='Beta'),
           upload('b3', '3', group='Beta'), upload('a2', '2')]], endpoint_status=404)

    # the chapter endpoint fails with more than one page: an error
    case('case-error', {},
         [[upload('e1', '1'), upload('e2', '2')], [upload('e3', '3')]], endpoint_status=404)

    # numbers and titles in their forms
    formats = [upload('n1', 1), upload('n2', 2.5), upload('n3', '31.2.5'), upload('n4', '12abc'),
               upload('n5', ' 7 ', title=' Seven '), upload('n6', '8', title='Ch. 8'), upload('n7', '', title=''),
               upload('n8', '9', 1.5, group=['A', 'B']), upload('n9', 'none', title='Extra')]
    case('case-formats', {'status': 3, 'year': 1999, 'demographic_name': ' seinen '},
         [formats], list(reversed(formats)))
    return picks


# --- MangaDex

MANGADEX = 'https://api.mangadex.org'
RATINGS = '&contentRating[]=safe&contentRating[]=suggestive&contentRating[]=erotica&contentRating[]=pornographic'


def chapter(cid, number=None, volume=None, title=None, created='2020-01-01T00:00:00+00:00'):
    attributes = {'chapter': number, 'volume': volume, 'translatedLanguage': 'en', 'createdAt': created}
    if title is not ...:
        attributes['title'] = title
    return {'id': cid, 'type': 'chapter', 'attributes': attributes}


def mangadex_case(f, mid, attributes, relationships, chapters, page_size=500):
    f.add(f'{MANGADEX}/manga/{mid}?includes[]=author&includes[]=artist&includes[]=cover_art',
          {'result': 'ok', 'response': 'entity',
           'data': {'id': mid, 'type': 'manga', 'attributes': attributes, 'relationships': relationships}})
    for offset in range(0, max(len(chapters), 1), page_size):
        f.add(f'{MANGADEX}/manga/{mid}/feed?limit=500&offset={offset}&translatedLanguage[]=en' + RATINGS +
              '&includeExternalUrl=0&includeEmptyPages=0&includeFuturePublishAt=0&includeUnavailable=0'
              '&order[volume]=asc&order[chapter]=asc&order[createdAt]=asc',
              {'result': 'ok', 'data': chapters[offset:offset + page_size], 'limit': 500, 'offset': offset,
               'total': len(chapters)})


def mangadex(f):
    picks = []
    ids = iter(f'00000000-0000-4000-8000-{i:012d}' for i in range(1, 1000))

    def case(name, attributes, relationships, chapters, **kwargs):
        mid = next(ids)
        mangadex_case(f, mid, attributes, relationships, chapters, **kwargs)
        picks.append((name, f'{MANGADEX}/manga/{mid}'))

    def cid(n):
        return f'c0000000-0000-4000-8000-{n:012d}'

    description = ('**Bold** and *italic* and __underlined__, a [link](https://x.example) and [b]bbcode[/b].\r\n'
                   '\r\n\r\n\r\n---\r\nSecond part.\r\n\r\n**Links:**\r\n- [Official](https://y.example)')
    relationships = [{'type': 'author', 'attributes': {'name': ' Writer '}},
                     {'type': 'author', 'attributes': {'name': 'Writer'}},
                     {'type': 'artist', 'attributes': {'name': 'Painter'}},
                     {'type': 'artist', 'attributes': None},
                     {'type': 'author'},
                     {'type': 'cover_art', 'attributes': {'fileName': 'cover.jpg'}}]
    tags = [{'attributes': {'name': {'en': 'Romance'}, 'group': 'genre'}},
            {'attributes': {'name': {'en': 'School Life'}, 'group': 'theme'}},
            {'attributes': {'name': {'ja': 'Nur japanisch'}, 'group': 'genre'}},
            {'attributes': {'name': {'en': 'Format'}, 'group': 'format'}}]

    # an anthology: the numbers start again in every volume (without the flag)
    case('anthology', {'status': 'completed', 'year': 2001, 'publicationDemographic': 'josei', 'tags': tags,
                       'description': {'en': description, 'de': 'Beschreibung'}},
         relationships,
         [chapter(cid(v * 10 + n), str(n), str(v), f'Story {v}.{n}') for v in (1, 2, 3) for n in (1, 2, 3)])

    # flagged, with volumes that aren't whole numbers and chapters without volume
    case('flagged', {'status': 'ongoing', 'chapterNumbersResetOnNewVolume': True, 'description': []}, [],
         [chapter(cid(101), '1', '1'), chapter(cid(102), '2', '1'), chapter(cid(103), '1', '1.5'),
          chapter(cid(104), '1', None), chapter(cid(105), '2', None), chapter(cid(106), None, None, 'Bonus')])

    # parts and decimals, duplicates, unnumbered chapters, oneshots and whole volumes
    case('parts', {'status': 'hiatus', 'year': None, 'description': {'ja': 'nur japanisch'}}, [],
         [chapter(cid(201), '14.1'), chapter(cid(202), '14.10'), chapter(cid(203), '14.11'),
          chapter(cid(204), '14.2'), chapter(cid(205), '24.5'), chapter(cid(206), '24.75'),
          chapter(cid(207), '3.0'), chapter(cid(208), '014.5'), chapter(cid(209), '5-6'),
          chapter(cid(210), 'Extra', None, 'Side story'), chapter(cid(211), None, None, ''),
          chapter(cid(212), None, '2', None), chapter(cid(213), None, None, ...),
          chapter(cid(214), '7', None, '', '2021-05-01T00:00:00+00:00'),
          chapter(cid(215), '7', None, 'Again', '2020-05-01T00:00:00+00:00'),
          chapter(cid(216), '8', None, ...), chapter(cid(217), '42.8'), chapter(cid(218), '42.08'),
          chapter(cid(219), ' 9 ', ' 3 ', 'Spaces')])

    # a feed of more than one page
    case('paged', {'status': 'completed'}, [],
         [chapter(cid(1000 + n), str(n), None, f'Chapter {n}') for n in range(1, 601)])
    return picks


def main():
    source, directory, picks_file = sys.argv[1:4]
    f = Fixtures()
    picks = {'Comick': comick, 'MangaDex': mangadex}[source](f)
    f.write(directory)
    with open(picks_file, 'w') as out:
        out.write(f'# {source}: made up cases (tests/harness/cases.py)\n')
        for title, url in picks:
            out.write(f'{title}\t{url}\n')


if __name__ == '__main__':
    main()
