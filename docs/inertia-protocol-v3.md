# Inertia.js server protocol: reference for cpp-inertia

This document was compiled on 2026-10-23 from the primary sources listed below.

**Versions targeted:**
- `@inertiajs/core` / `@inertiajs/react` **3.9.1**
- `inertia-laravel` **v3.5.2**, used as the reference implementation

**Sources:**
- **[P]** https://inertiajs.com/docs/v3/core-concepts/the-protocol.md, the framework-agnostic wire spec. Append `.md` to any doc URL to get Markdown.
- **[UG]** https://inertiajs.com/docs/v3/getting-started/upgrade-guide.md
- **[L:x]** `inertiajs/inertia-laravel/src/x`
- **[Core:x]** `inertiajs/inertia/packages/core/src/x`

Facts below are confirmed in source unless marked **[S]**.

## Page object

Keys are emitted in this order:
1. `component`
2. `props`
3. `url`
4. `version`
5. the metadata fields
6. the flags

**Always present**
| key | type | notes |
|---|---|---|
| `component` | string | |
| `props` | object | always contains `errors` (`{}` when none) |
| `url` | string | request path + query **as received**; a trailing slash is preserved |
| `version` | string | `""` when assets are not versioned |

**Metadata (emitted only when non-empty)**
| key | shape |
|---|---|
| `sharedProps` | `string[]`: top-level keys of shared props, including `errors`. A dot key contributes its first segment. Config `exposeSharedPropKeys` (default true). |
| `mergeProps` | `string[]` of dot-paths (append) |
| `prependProps` | `string[]` of dot-paths |
| `deepMergeProps` | `string[]` of dot-paths |
| `matchPropsOn` | `string[]` of `"<propPath>.<keyField>"` |
| `deferredProps` | `{ "<group>": ["path", …] }`, default group `"default"` |
| `rescuedProps` | `string[]`: deferred paths whose resolver threw with rescue enabled. The prop itself is **omitted** from `props`. |
| `scrollProps` | `{ "<path>": { "pageName": string, "previousPage": int\|string\|null, "nextPage": int\|string\|null, "currentPage": int\|string\|null, "reset": bool } }` |
| `onceProps` | `{ "<key>": { "prop": "<path>", "expiresAt": int-ms\|null } }` |

**Flags (emitted only when `true`)**
| key | source |
|---|---|
| `clearHistory` | session key `inertia.clear_history`, pulled once |
| `encryptHistory` | global config or per-response override |
| `preserveFragment` | session key `inertia.preserve_fragment`, pulled once |
| `preserveBigIntegers` | config. Integers outside ±(2^53−1) are serialized as `{"$bigint":"<digits>"}`. |

**Optional**: `flash` (object) is pulled from session key `inertia.flash_data`. The key is omitted when the object is empty. Repeated flash calls merge into it.

The server never emits these client-only fields: `initialDeferredProps`, `rememberedState`, `optimisticUpdatedAt`.

## Headers

**Request** (case-insensitive):
- `X-Inertia: true`
- `X-Inertia-Version`
- `X-Inertia-Partial-Component`
- `X-Inertia-Partial-Data`
- `X-Inertia-Partial-Except`
- `X-Inertia-Reset`
- `X-Inertia-Error-Bag`
- `X-Inertia-Infinite-Scroll-Merge-Intent`: `prepend`, otherwise append
- `X-Inertia-Except-Once-Props`
- `Purpose: prefetch`
- `X-XSRF-TOKEN`
- `Referer`: used for "redirect back"

**List headers** are split on `,` and empty entries are dropped. An empty result means the header is absent. The client joins entries without spaces. cpp-inertia additionally trims whitespace around entries. **[S]** That deviation is safe.

**Response headers:**
- JSON page: `X-Inertia: true` and `Content-Type: application/json`.
- `Vary: X-Inertia` on **every** response. It is appended to an existing `Vary` value, deduplicated, and empty entries are dropped.
- 409 responses carry `X-Inertia-Location`, or `X-Inertia-Redirect`, or `X-Inertia-Version`. They **never** carry `X-Inertia`, and their body is empty.

## Behaviors

**First visit (non-Inertia request).** The server returns 200 `text/html`. The root template contains:
```html
<script data-page="app" type="application/json">{page json}</script><div id="app"></div>
```
- The root id is configurable (default `app`), and the `data-page` value equals that id.
- The JSON must escape `/` as `\/`, `<` as `<` and `>` as `>`. It must not use HTML entities.

**Inertia request.** The server returns 200 JSON with `X-Inertia: true`.

**Version mismatch.** When the request is Inertia, the method is `GET`, and `X-Inertia-Version` (default `""`) differs from the current version, the server returns **409** with:
- `X-Inertia-Location: <full request URL>`;
- `X-Inertia-Version: <current>`.

Other methods never get this 409.

**303.** On an Inertia request, a 302 becomes a 303 for `PUT`, `PATCH` and `DELETE` (not `POST`).

**Empty response.** On an Inertia request, a 200 with an empty body becomes a 302 to `Referer`, or to `/` if there is none. A body of `"0"` is not empty.

**Fragment redirect.** On an Inertia request that is not a prefetch, a redirect (3xx with `Location`) whose `Location` contains `#` becomes **409** with `X-Inertia-Redirect: <Location>`. This applies to any method.

**External redirect, `location(url)`.**
- Inertia request: 409 with `X-Inertia-Location: url`.
- Otherwise: 302 with `Location: url`.

**Partial reload.** A request is partial only when `X-Inertia-Partial-Component` equals the rendered component. Otherwise it is treated as a full visit. Prop paths are dot-paths.
- A path is **included** if it equals an `only` entry, is a descendant of one, or is an ancestor of one.
- It is then **excluded** if it equals an `except` entry or is a descendant of one. `except` wins over `only`.
- Always props bypass both filters, and so do children of a value produced by resolving a closure.
- Top-level dot keys (`"a.b": v`) are unpacked into nested objects before resolution. Nested dot keys are not unpacked.

**Prop evaluation per category.** "Partial" means the prop path passes the filter.
| category | full visit | partial |
|---|---|---|
| regular / closure | resolved | resolved if selected |
| always | resolved | always resolved |
| optional | skipped, no metadata | resolved if selected |
| defer | skipped, listed in `deferredProps[group]` | resolved if selected; `deferredProps` never emitted on partials |
| defer + once, already loaded | no `deferredProps` entry; `onceProps` kept | resolved if selected |
| defer + merge | merge metadata emitted even while deferred | normal merge rules |
| defer + scroll | `deferredProps` + merge metadata, **no** `scrollProps` | normal |

Prop types work at any depth: inside nested objects, and inside values returned by closures. Metadata paths are full dot-paths (e.g. `auth.notifications`).

**Merge metadata.**
- It is suppressed for paths listed in `X-Inertia-Reset`.
- On partial reloads, it is emitted only when the path equals an `only` entry or is a descendant of one (ancestors don't count), and is not excluded by `except`.
- Classification:
  - deep merge → `deepMergeProps`;
  - root append → `mergeProps`;
  - root prepend → `prependProps`;
  - nested append/prepend paths → `"<path>.<sub>"` entries in `mergeProps` or `prependProps`.
- A `matchOn` key adds `"<path>.<key>"` to `matchPropsOn`. For nested paths, the entry becomes `"<path>.<sub>.<key>"`.

**Once props.**
- A once prop is excluded only when all of these hold:
  - the request is Inertia;
  - it is not partial;
  - the prop is not `fresh`;
  - its key (custom key, else path) is in `X-Inertia-Except-Once-Props`.
- Even when excluded, its `onceProps` entry is emitted.
- `expiresAt` = (now + ttl seconds) × 1000, or `null` without a ttl.
- On the first HTML load the header is ignored.

**Scroll props.**
- The merge label is `"<path>.<wrapper>"`, with the wrapper defaulting to `data`. It goes in `prependProps` when the merge-intent header is `prepend`, otherwise in `mergeProps`.
- `scrollProps[path].reset` is true when the path is in the reset list. The merge label is then suppressed.

**Validation errors.**
- `errors` is an always prop. It is `{}` when there are no errors, never `[]`.
- Each field maps to its first message. In all-errors mode, each field maps to `string[]`.
- With an `X-Inertia-Error-Bag` header, errors are scoped as `{ "<bag>": { field: msg } }`.
- Errors are stored in the session (`inertia.errors`) by `withErrors` and pulled on the next render.

**SSR.**
- Request: `POST {ssrUrl, default http://127.0.0.1:13714}/render` with the page JSON as body. In Vite dev mode the path is `/__inertia_ssr` on the hot URL.
- Response: `{"head": string[], "body": string}`. `head` is joined with `"\n"`.
- `body` replaces both the script element and the root div, and contains `data-server-rendered="true"`.
- Any failure (connection, non-2xx, invalid JSON) falls back to client-side rendering.
- `GET /health` returns `{"status":"OK",…}`.

**Asset version default.** cpp-inertia uses a hash of the Vite manifest file. Laravel uses xxh128, but any hash that changes with the assets is valid. **[S]**

## Out of scope (Laravel-specific)

Each of these is listed as `n/a` in the coverage matrix:
- previous-URL storage;
- Precognition;
- the exception handler integration;
- `ensure_pages_exist`;
- Mix manifest;
- devtools recorder;
- PHP enums and Arrayable/Responsable objects;
- `ProvidesInertiaProperties` (C++ equivalent: plain functions returning `Props`).
