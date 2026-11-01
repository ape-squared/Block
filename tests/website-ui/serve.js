/**
 * Serve website/ under /Block/ so absolute paths (/Block/styles.css, latest.json) match Pages.
 * Usage: node serve.js [port]
 */
const http = require("http");
const fs = require("fs");
const path = require("path");

const port = Number(process.argv[2] || 4173);
const root = path.resolve(__dirname, "../../website");
const prefix = "/Block";

const types = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "application/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".webp": "image/webp",
  ".ico": "image/x-icon",
};

function send(res, status, body, type) {
  res.writeHead(status, {
    "Content-Type": type || "text/plain; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(body);
}

const server = http.createServer((req, res) => {
  let urlPath = decodeURIComponent((req.url || "/").split("?")[0]);
  if (urlPath === "/" || urlPath === "") {
    res.writeHead(302, { Location: prefix + "/" });
    return res.end();
  }
  if (!urlPath.startsWith(prefix)) {
    return send(res, 404, "Not found (expected /Block/…)");
  }
  let rel = urlPath.slice(prefix.length) || "/";
  if (rel.endsWith("/")) rel += "index.html";
  const file = path.normalize(path.join(root, rel));
  if (!file.startsWith(root)) {
    return send(res, 403, "Forbidden");
  }
  fs.readFile(file, (err, data) => {
    if (err) {
      return send(res, 404, `Not found: ${rel}`);
    }
    const ext = path.extname(file).toLowerCase();
    send(res, 200, data, types[ext] || "application/octet-stream");
  });
});

server.listen(port, "127.0.0.1", () => {
  console.log(`Block website UI server: http://127.0.0.1:${port}${prefix}/`);
});
