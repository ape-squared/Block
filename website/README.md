# Block website

Static site for GitHub Pages: https://ape-squared.github.io/Block/

Clean URLs use folders (`/download/`, `/docs/`, …). Old `*.html` paths redirect.

| Path | Purpose |
|------|---------|
| `/` | Product landing |
| `/download/` | Get started — Windows zip / Setup |
| `/docs/` | Doc index + links into the repo |
| `/contribute/` | Contributor guide (human-only) |
| `/ai/` | AI policy |
| `/latest.json` | OTA channel for Setup + overlay |
| `styles.css` / `site.js` | Shared look and motion |

Visual form follows `design/web-mockup/`. Deploy: push to `main` (workflow `pages.yml`).
