# Issue #2: Home feed reproduction

Initial offline scope: host investigation of the current signed-in **OAuth TV account path**,
with invented credentials, video IDs and titles. No Google login, network
request or real account response was used. This is a partial reproduction of
observable layout/filter behavior, not verification of the reporter's personal
recommendations or a PS5 screenshot.

## Method

`scripts/reproduce_home_issue.mjs` clones the repository's sanitized
`companion/innertube/fixtures/tv-home-shape.json`, retaining its TV response
wrappers and tile shape. It creates four shelves with six tiles each:

1. Primary recommendations, marked watched.
2. Shorts, with `TILE_CONTENT_TYPE_SHORTS`.
3. Ad tiles, explicitly marked `isAd: true`.
4. Other recommendations, normal videos.

It calls the real `Library.execute({action:'home',profile:'saved'})` with an
OAuth profile. Only `/browse` is mocked; it asserts the real request uses
`browseId: 'default'` and `client: 'TV'`. It collects output across local
pagination and opens each retained shelf using `suggestions_filter`. It
repeats with `shorts_home` and `watched_home` flags.

Run after installing the locked Node dependencies:

```sh
cd companion/innertube
npm ci --ignore-scripts
cd ../..
node scripts/reproduce_home_issue.mjs
```

## Observed results

| Check | Result |
| --- | --- |
| Default output | 18 video cards: 6 primary, 6 Shorts, 6 other recommendations |
| Source video order | Preserved among surviving cards |
| Shelf layout | All shelves' videos flattened into one grid; four shelf-navigation tiles appended |
| Hide Shorts from Home | 0 Shorts videos, but Shorts navigation tile remains and opens with 0 videos |
| Explicit ad markers | All 6 marked ad videos removed |
| Ad-only shelf | Navigation tile remains and opens with 0 videos |
| Hide watched from Home | All 6 watched primary videos still returned, same as default |

The additional shelf titles and category grouping in this fixture are
synthetic. These tests do not establish that a real TV answer identifies its
ads in exactly the same way, nor that the reporter saw an empty ad shelf
rather than an actual sponsored video.

## Source explanation

For OAuth Home, `companion/innertube/library.mjs` routes to
`TvAccountCatalog.browse()`, not the generic `HomeCatalog.root()` except for a
legacy response without raw TV tiles.

`TvAccountCatalog.feed()` flattens the video cards from every supported shelf
in source order, then appends `TopicCatalog.feed()`'s shelf folders. Folder
creation does not check whether normalization/content filtering leaves any
playable cards, so empty ad-only and hidden-Shorts shelves remain navigable.

`content_filters.mjs:hides()` explicitly delegates watched-Home filtering to
the Home grid instead of its normalized-item predicate. The generic
`HomeCatalog` handles that rule, but the tested raw-TV route does not apply
it. This explains why the explicit `watched_home` flag has no effect on these
TV cards. It is not yet established that this is the cause of the reporter's
unfamiliar recommendations.

The TV tile adapter already rejects `isAd` and `adSlotMetadata` and classifies
Shorts from content type or reel endpoints. The test confirmed those known
tile markers work. No actual ad-video bypass was demonstrated.

## Existing tests and remaining work

Ran:

```sh
cd companion/innertube
node --test home_feed.test.mjs tv_account_shapes.test.mjs content_filters.test.mjs
```

**28 passed, 0 failed.** The separate reproduction script also completed all
assertions and printed the observations above. No production behavior was
changed during this investigation.

A live comparison is still required to determine whether the TV endpoint is
returning a different personalized feed from the user's usual YouTube client,
or whether another currently unrepresented renderer is being lost. Use the
same account, client type, locale and closely timed responses when comparing;
do not expect a desktop WEB feed and a TV feed to be identical by assumption.
Capture only sanitized renderer structure and counts for regression fixtures.
Sign in through Google's normal consent flow if live testing is needed; no
password or account token needs to be placed in an issue, test or report.

## Authorized live-account follow-up

The user subsequently authorized a temporary test session through Google's
device sign-in flow. Two authenticated TV Home responses were read with the
same pinned YouTube.js client and `browseId: 'default', client: 'TV'` used by
the account adapter. Each response was replayed through `Library.execute()`
with default settings, `shorts_home`, and `watched_home`, so differences in
upstream recommendations between requests did not confound filter comparisons.
These were host API/adapter checks, not PS5 execution or a comparison against
the official YouTube application's screen.

| Observation | Live response A | Live response B |
| --- | --- | --- |
| TV shelves | 4 | 4 |
| Raw tile renderers across those shelves | 18 (3, 5, 5, 5) | 20 (3, 7, 5, 5) |
| Normalized video cards | 18 | 13 |
| Navigation/folder cards | 4 | 9 |
| Total first-page cards | 22 | 22 |
| Additional page available | Yes | Yes |
| Tiles classified as Shorts | 0 | 0 |
| Explicit `isWatched` tiles | 0 | 0 |
| Raw ad layouts | 1 top masthead | 2 in-feed ad layouts |

Both responses confirmed that the live TV route combines shelf contents into
one grid and appends shelf navigation. Response B's tile/video count difference
is not by itself evidence of dropped recommendations: normalization can also
produce playlist/navigation cards, and raw renderer counts include structures
that are not direct video cards. No personal video identities were retained
to claim an item-by-item baseline comparison.

The top-level masthead in response A was omitted by the adapter's shelf/card
mapping. No actual ad-video leak was established in either sample. Shorts and
watched filters did not change first-page results, but neither response had
tiles classified as Shorts or explicitly `isWatched`. Three resume-progress
overlays existed in each raw response; their percentages were not retained,
so this cannot validate fully-watched filtering. The offline watched-filter
reproduction above remains independent evidence.

The live samples therefore confirm the **layout transformation**, while the
reporter's specific claims of unfamiliar personalized recommendations,
displayed ads and Shorts remain only partially reproduced. A visual comparison
with the same account in the official client and additional relevant response
shapes would be needed to attribute those claims to a specific parser defect.
No production recommendation behavior was changed.

Only aggregate renderer names, counts and adapter summaries were retained.
No account name, email, OAuth credential, recommendation title, video/channel
identity, tracking parameter or raw account response was saved in this report
or the repository. Authorization credentials remained in the temporary test
process; that process was closed after testing. Closing it is not a revocation
of the Google grant.
