---
title: "Publishing and Search Setup"
description: "Maintainer instructions for publishing this tutorial with GitHub Pages."
robots: "noindex, follow"
sitemap: false
---

# Publishing and Search Setup

## Before the first deployment

Edit `_config.yml`:

```yaml
url: "https://YOUR-GITHUB-USERNAME.github.io"
baseurl: "/cpp-coroutines-tutorial"
```

If the repository has a different name, use that name for `baseurl`. For a user site named `YOUR-GITHUB-USERNAME.github.io`, set `baseurl: ""`. For a custom domain, set `url` to the custom origin and normally set `baseurl: ""`.

Incorrect values produce incorrect canonical URLs, sitemap entries, and social-sharing URLs, so replace them before enabling GitHub Pages.

Optionally add verified author information to `_config.yml` so the generated JSON-LD can identify the tutorial's author:

```yaml
author:
  name: "Your Name"
  url: "https://github.com/YOUR-GITHUB-USERNAME"
```

## GitHub Pages

1. Push the repository to GitHub.
2. Open **Settings → Pages**.
3. Under **Build and deployment**, choose **Deploy from a branch**.
4. Select the `main` branch and the repository root.
5. Wait for the Pages deployment to finish.

## Search-engine registration

After the site is live:

1. Open `https://YOUR-SITE/sitemap.xml` and `https://YOUR-SITE/robots.txt` to confirm both work.
2. Add the site to Google Search Console and submit `sitemap.xml`.
3. Add the site to Bing Webmaster Tools and submit the same sitemap.
4. Put the verification tokens in `_config.yml` under `webmaster_verifications` if you use HTML-tag verification.
5. Request indexing for the home page after the first successful crawl.

Search indexing is not immediate or guaranteed. Helpful signals include unique page titles and descriptions, crawlable internal links, a valid sitemap, useful original content, incoming links, and keeping the tutorial current.

The `keywords` metadata is included for clients that use it, but Google does not use the `meta keywords` tag for ranking. The visible headings, descriptions, and technical content are the important keyword signals.
