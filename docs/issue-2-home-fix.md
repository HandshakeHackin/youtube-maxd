# TV account Home grouping correction (issue #2)

When an OAuth TV Home response contains multiple shelves, the account adapter
previously flattened every shelf's cards into the primary grid. Later category
videos and Shorts could consequently appear to be later rows of one continuous
recommendation list. Empty ad-only or fully filtered shelves could still leave
navigation tiles.

## Resulting behavior

- The first usable source group supplies the main preview. Subsequent shelves
  are reached through their own named folders, in source order.
- The primary shelf retains a folder so its horizontal continuation is
  reachable independently of the Home page continuation.
- A final shelf with no items surviving Shorts/mixes/ad filtering contributes
  no folder. A filtered-empty shelf with a continuation remains reachable:
  a later page may still contain visible cards.
- An unnamed nonempty shelf uses "More videos", preserving access to its cards.
- Watched filtering applies to the primary TV preview and flat Home continuation
  pages. Default and explicit flag handling match the web Home rule. Explicitly
  opened category shelves retain watched cards, matching existing web behavior.
- Home-page continuation shelves stay grouped instead of becoming a new primary
  preview. Shelf and Home continuation tokens are kept separate.
- Existing profile ownership checks and the 4,096-item account budget remain.
  Other account sections keep their existing mapping.

The library now passes the selected hide flags to the OAuth TV adapter.
No recommendation-ranking, upstream client identity, OAuth handling or native
rendering code changes are part of this correction.

## Verification and limits

The workspace execution server failed to initialize during this change.
Consequently npm/Node integration tests, the PS5 build and console execution
could not be run in the workspace.

The new `tv_home_groups.test.mjs` contains 11 Node regression cases and is
imported by the existing worker test entry point. Ten adapter-level cases were
also run in the available isolated JavaScript runtime using the production
TopicCatalog, TvAccountCatalog, SuggestionFilters, video normalizer and content
filter source. The harness replaced Node I/O/crypto/URL facilities and used
plain-JSON cloning for synthetic fixtures; this is not a Node-suite pass.

- Original adapter through an entry-point compatibility shim: 2 pass, 8 fail.
- Corrected adapter: 10 pass, 0 fail.
- The additional full Library routing test is prepared but was not executed.

Cases cover source grouping, filter selections, ad-only final shelves, primary
watched filtering, unnamed shelves, retained filtered-empty continuations,
independent Home/shelf tokens, flat continuation filters, profile ownership,
malformed groups and the account-wide item limit.

Run these checks once a Node environment is available:

```sh
cd companion/innertube
npm ci --ignore-scripts
node --test tv_home_groups.test.mjs tv_account_shapes.test.mjs home_feed.test.mjs content_filters.test.mjs
npm test
```

Earlier authorized live testing confirmed the flattening behavior with two
authenticated responses containing four shelves apiece. It did not establish an
actual ad-video leak or a personalized-ranking defect. The corrected code has
not been replayed against that live account; its temporary session was closed.
Tests contain only invented identities, titles and credentials. No live account
data is included.

On PS5, check the primary preview, each category folder, Home and shelf
pagination, changing hide settings, an empty result, and profile switching.
This patch intentionally changes how later categories are accessed; it does
not attempt to reproduce the official client's full horizontal-row UI.
