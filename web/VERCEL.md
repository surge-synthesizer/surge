# Deploy the browser development build to Vercel

The production app is https://surge-xt-browser.vercel.app, project
`surge-xt-browser` in `amila-welihindas-projects`. This is the current development
port, not a declaration that the full migration parity gate has passed.

Build the browser using `web/scripts/build.sh` and the instructions in
`web/README.md`. Package and stage the resulting artifacts:

```sh
python3 web/scripts/package-static.py
python3 web/scripts/vercel-static.py \
  --package web/dist/<distribution-digest> \
  --output web/dist/vercel-deploy \
  --commit 58914e59c608ed4384ba6002e44c3465c58b2e71
vercel link --yes --project surge-xt-browser \
  --scope amila-welihindas-projects --cwd web/dist/vercel-deploy
vercel deploy --dry --json --cwd web/dist/vercel-deploy
vercel deploy --prod --yes --cwd web/dist/vercel-deploy
node web/scripts/verify-deployment.mjs https://surge-xt-browser.vercel.app
```

Use a new staging directory for each build. The pinned commit supplies the
unchanged factory resources. If the resources change, update the commit and
manifest together; a content mismatch fails the build. Deployment uploads omit
factory objects except the generated patch index. Vercel restores every resource
from that pinned upstream archive at build time and verifies size and SHA-256
against the packaged manifest. The app has no backend, and browser asset requests
stay on the deployed origin.

The configuration serves `/` as the application, enables cross-origin isolation,
and serves Wasm with `application/wasm`. Content-addressed factory objects have
immutable caching; application filenames revalidate on repeat visits. No harness
binaries, credentials, or local user files are included. A source overlay plus
pinned upstream reconstruction instructions and the available license notices
are published under `/source/` and `/THIRD-PARTY-NOTICES.txt`.

The verification script launches desktop Chrome against the public URL. It checks
isolation, startup, factory catalog, keyboard-triggered nonzero audio, patch search
and switching, context suspension/resumption, and patch save/reload persistence.
It writes a screenshot and JSON report to `/tmp/surge-deployment-check` by default.
Chrome runs with silent physical output: this verifies the real worklet and audio
samples, not speakers or attached MIDI/audio devices. Full migration parity and
the complete dependency-notice audit remain separate outstanding requirements.
