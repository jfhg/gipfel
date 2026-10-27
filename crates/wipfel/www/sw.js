/* wipfel service worker: full offline + Web Share Target.
 *
 * Strategy: network-first with cache fallback, so an online app is
 * always fresh and the app still starts offline after the first visit.
 *
 * Share target: the browser POSTs multipart/form-data to __share; we
 * stash the file in a cache under __shared and redirect to the app
 * with ?shared=1, which the app picks up on startup.
 */
const VERSION = "wipfel-v1";
const SHARE_CACHE = "wipfel-share";
const base = self.registration.scope;
const url = (p) => new URL(p, base).href;
const sharePath = new URL("__share", base).pathname;
const sharedPath = new URL("__shared", base).pathname;

const SHELL = [
  "",
  "index.html",
  "wipfel.js",
  "wipfel_bg.wasm",
  "style.css",
  "gipfel.dat.gz",
  "manifest.webmanifest",
  "icons/icon-192.png",
  "icons/icon-512.png",
  "icons/icon-maskable-512.png",
].map(url);

self.addEventListener("install", (e) => {
  e.waitUntil(
    caches
      .open(VERSION)
      .then((c) => c.addAll(SHELL))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches
      .keys()
      .then((keys) =>
        Promise.all(
          keys.filter((k) => k !== VERSION && k !== SHARE_CACHE).map((k) => caches.delete(k))
        )
      )
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (e) => {
  const req = e.request;
  const u = new URL(req.url);
  if (u.origin !== self.location.origin) return;

  // Web Share Target handoff
  if (req.method === "POST" && u.pathname === sharePath) {
    e.respondWith(
      (async () => {
        const form = await req.formData();
        const file = form.getAll("files")[0] || form.get("files");
        if (file && file.size) {
          const buf = await file.arrayBuffer();
          const cache = await caches.open(SHARE_CACHE);
          await cache.put(
            sharedPath,
            new Response(buf, {
              headers: {
                "Content-Type": file.type || "image/jpeg",
                "X-File-Name": encodeURIComponent(file.name || "shared.jpg"),
              },
            })
          );
        }
        return Response.redirect(url("?shared=1"), 303);
      })()
    );
    return;
  }

  // the stashed shared image
  if (req.method === "GET" && u.pathname === sharedPath) {
    e.respondWith(
      caches.open(SHARE_CACHE).then((c) =>
        c.match(sharedPath).then(
          (r) => r || new Response("no shared image", { status: 410 })
        )
      )
    );
    return;
  }

  // app assets: network first, cache fallback
  e.respondWith(
    (async () => {
      try {
        const res = await fetch(req);
        if (res.ok) {
          const cache = await caches.open(VERSION);
          cache.put(req, res.clone());
        }
        return res;
      } catch (err) {
        const cached = await caches.match(req, { ignoreSearch: true });
        if (cached) return cached;
        if (req.mode === "navigate") {
          const idx = await caches.match(url("index.html"));
          if (idx) return idx;
        }
        throw err;
      }
    })()
  );
});
