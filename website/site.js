(() => {
  const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  // Scroll reveals
  const reveals = document.querySelectorAll(".reveal");
  if (reduce) {
    reveals.forEach((el) => el.classList.add("is-in"));
  } else if ("IntersectionObserver" in window) {
    const io = new IntersectionObserver(
      (entries) => {
        for (const e of entries) {
          if (!e.isIntersecting) continue;
          e.target.classList.add("is-in");
          io.unobserve(e.target);
        }
      },
      { rootMargin: "0px 0px -8% 0px", threshold: 0.12 }
    );
    reveals.forEach((el) => io.observe(el));
  } else {
    reveals.forEach((el) => el.classList.add("is-in"));
  }

  // Sticky nav border
  const nav = document.querySelector("[data-nav]");
  if (nav) {
    const onScroll = () => nav.classList.toggle("is-scrolled", window.scrollY > 8);
    onScroll();
    window.addEventListener("scroll", onScroll, { passive: true });
  }

  // Mobile nav
  const toggle = document.querySelector("[data-nav-toggle]");
  if (toggle && nav) {
    toggle.addEventListener("click", () => {
      const open = nav.classList.toggle("is-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    nav.querySelectorAll(".nav-links a").forEach((a) => {
      a.addEventListener("click", () => {
        nav.classList.remove("is-open");
        toggle.setAttribute("aria-expanded", "false");
      });
    });
  }

  // latest.json → download CTA (resolve next to stylesheet so any Pages base works)
  const verEl = document.querySelector("[data-latest-version]");
  const dlEl = document.querySelector("[data-latest-download]");
  if (verEl || dlEl) {
    const css = document.querySelector('link[rel="stylesheet"]');
    const latestUrl = css
      ? new URL("latest.json", css.href).href
      : new URL("latest.json", location.href).href;
    fetch(latestUrl, { cache: "no-store" })
      .then((r) => (r.ok ? r.json() : null))
      .then((data) => {
        if (!data) return;
        const tag = data.tag || ("v" + data.version);
        const version = data.version || String(tag).replace(/^v/, "");
        const zipName = "Block-" + version + "-windows-x64.zip";
        const assetUrl =
          "https://github.com/ape-squared/Block-GPPI/releases/download/" +
          encodeURIComponent(tag) +
          "/" +
          encodeURIComponent(zipName);
        if (verEl) verEl.textContent = "latest " + tag;
        if (dlEl) {
          dlEl.href = assetUrl;
          dlEl.textContent = "Download " + zipName;
        }
      })
      .catch(() => {
        if (verEl) verEl.textContent = "see GitHub Releases";
      });
  }
})();
