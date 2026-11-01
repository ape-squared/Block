# Website UI smoke tests

Playwright checks for the public site under `website/`. These run on every pull request so contributors get fast feedback when docs/site HTML/JS change.

## What this covers

- Home hero + primary nav  
- Download page wiring of `latest.json` → CTA  
- Docs / Contribute / AI policy pages render  
- Mobile nav toggle  
- `styles.css` + `site.js` load  

## What this does **not** cover

| Surface | Why |
|---------|-----|
| In-viewer overlay (`Shift`+`B`) | Needs a live OpenGL viewer + GPU; use manual test in the PR template |
| `BlockSetup.exe` WinForms UI | Needs a Windows GUI runner + signed-off automation; not in PR CI yet |

## Local

```bash
cd tests/website-ui
npm install
npx playwright install chromium
npm test
```

CI installs Chromium via `npx playwright install --with-deps chromium`.
