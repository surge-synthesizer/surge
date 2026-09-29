# Distribution notice audit

`inventory.json` records original notice documents and their hashes. It is a
partial audit, not a complete license bundle or a determination of license
compatibility. It must not be used to mark the distribution gate complete.

The first pass follows `surge-common` link dependencies, the browser application's
link response file, and known header dependencies. All 182 indexed sources exist
and are nonempty. Preserve their text and authorship notices when packaging; do
not replace them with a list of license identifiers. Changes to their hashes
require review of the upstream document.

PFFFT, SQLite, tinyxml and strnatcmp notices are indexed as exact byte ranges in
their original source headers. Both the complete source and selected notice have
hashes.

All six bundled fonts also have reviewed byte ranges for their English Unicode
copyright, license-description and license-URL name records (IDs 0, 13 and 14).
The collector verifies the full font and each range before decoding UTF-16BE;
it preserves the original notice text, including Indie Flower's full OFL. The
standalone OFL document is included alongside the Lato and Fira Mono metadata.

The 128 eurorack sources/headers referenced by the current Wasm build's compiler
dependency files are also indexed individually. Their complete leading MIT
notices retain per-file years, authors and comments. A scan found no additional
copyright/permission blocks outside those leading notices. This covers the
observed dependency set, not every future eurorack revision or uncompiled file;
dependency additions must be audited before release.

The compiler dependencies in the generated JUCE browser overlay confirm use of
HarfBuzz, SheenBidi, PNG, JPEG, zlib, Ogg/Vorbis and FLAC. Their primary license
documents and the FLAC/SheenBidi copyright headers are indexed. All nine added
source documents match their copies in the compiled overlay byte for byte.
JUCE per-file exceptions and toolchain runtime/port notices still need audit.

Generate the current partial bundle with:

```sh
python3 web/scripts/license-notices.py /tmp/surge-third-party-notices.txt
python3 web/tests/license-notices.test.py
```

The collector validates every document before atomically replacing its output.
Three checks pass for verbatim retention, changed/range-invalid notice rejection,
and preservation of an existing bundle when validation fails. This collector is
not yet wired into release packaging, and its output explicitly lists audit gaps.

The `remaining` list identifies embedded/per-file notices, JUCE and toolchain
runtime dependencies, factory assets and final dependency-to-bundle
coverage. Existing static packaging includes Surge, Lua and BitOp only. Integration
of the completed notice set is still required before release.

The additional PFFFT convolution MIT notices, ChowDSP omega MIT notice,
Vintage Ladder BSD notice and pink-noise attribution are retained as exact source
ranges. Font distribution readmes and the Voxengo impulse terms are included in
full. Voxengo assets have their own distribution conditions; inclusion in this
partial bundle does not turn those assets into GPL-licensed material. The final
asset packaging audit must verify that their complete original notices accompany
the downloaded files.
