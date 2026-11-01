// @ts-check
const { defineConfig } = require("@playwright/test");

const port = Number(process.env.BLOCK_UI_PORT || 4173);
const baseURL = process.env.BLOCK_UI_BASE || `http://127.0.0.1:${port}/Block/`;

module.exports = defineConfig({
  testDir: "./specs",
  timeout: 30_000,
  expect: { timeout: 10_000 },
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? [["github"], ["list"]] : "list",
  use: {
    baseURL,
    trace: "on-first-retry",
    screenshot: "only-on-failure",
  },
  webServer: {
    command: `node ./serve.js ${port}`,
    url: baseURL,
    reuseExistingServer: !process.env.CI,
    timeout: 60_000,
  },
});
