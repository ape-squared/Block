// @ts-check
const { test, expect } = require("@playwright/test");
const fs = require("fs");
const path = require("path");

const latest = JSON.parse(
  fs.readFileSync(path.resolve(__dirname, "../../../website/latest.json"), "utf8")
);

test.describe("Block website UI", () => {
  test("home hero and primary nav", async ({ page }) => {
    await page.goto("./");
    await expect(page).toHaveTitle(/Block/i);
    await expect(page.getByRole("heading", { level: 1 })).toContainText(/BLOCK/i);
    await expect(page.getByRole("link", { name: /Download for Windows/i })).toBeVisible();

    const nav = page.locator("header.nav, [data-nav]");
    await expect(nav.getByRole("link", { name: "Download" })).toBeVisible();
    await expect(nav.getByRole("link", { name: "Docs" })).toBeVisible();
    await expect(nav.getByRole("link", { name: "Contribute" })).toBeVisible();
    await expect(nav.getByRole("link", { name: "AI policy" })).toBeVisible();
  });

  test("download page loads latest.json into CTA", async ({ page }) => {
    await page.goto("./download/");
    await expect(page.getByRole("heading", { level: 1 })).toContainText(/BLOCK/i);

    const chip = page.locator("[data-latest-version]");
    await expect(chip).toContainText(new RegExp(latest.tag.replace(".", "\\."), "i"), {
      timeout: 15_000,
    });

    const dl = page.locator("[data-latest-download]");
    await expect(dl).toBeVisible();
    const href = await dl.getAttribute("href");
    expect(href).toMatch(/github\.com\/ape-squared\/Block(?:-GPPI)?\/releases\/download\//);
    expect(href).toContain(latest.tag);
    expect(href).toMatch(/Block-.*-windows-x64\.zip/);
  });

  test("docs, contribute, and AI policy pages render", async ({ page }) => {
    for (const pathSeg of ["docs/", "contribute/", "ai/"]) {
      await page.goto("./" + pathSeg);
      await expect(page.locator("main, .page-shell, .section").first()).toBeVisible();
      await expect(page.locator("h1").first()).toBeVisible();
    }
  });

  test("AI policy page mentions human authorship", async ({ page }) => {
    await page.goto("./ai/");
    await expect(page.locator("body")).toContainText(/AI/i);
    await expect(page.locator("body")).toContainText(/human/i);
  });

  test("mobile nav toggle opens links", async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    await page.goto("./");
    const toggle = page.locator("[data-nav-toggle]");
    await expect(toggle).toBeVisible();
    await toggle.click();
    await expect(page.locator("[data-nav]")).toHaveClass(/is-open/);
    await expect(page.locator(".nav-links a", { hasText: "Download" })).toBeVisible();
  });

  test("styles and site.js load", async ({ page }) => {
    const css = page.waitForResponse((r) => /styles\.css/.test(r.url()) && r.ok());
    const js = page.waitForResponse((r) => /site\.js/.test(r.url()) && r.ok());
    await page.goto("./");
    await css;
    await js;
    const color = await page.locator("body").evaluate((el) => getComputedStyle(el).backgroundColor);
    // dark theme — not browser default white
    expect(color).not.toMatch(/^rgb\(255,\s*255,\s*255\)$/);
  });
});

