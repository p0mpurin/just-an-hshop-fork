# Nocturne Vercel Updater

This is a tiny static Vercel project for the in-app updater.

It serves:

- `/nocturne-version`
- `/3hs.cia`

Before deploying a release:

1. Copy the built root `3hs.cia` to `vercel-updater/public/3hs.cia`.
2. Update `vercel-updater/public/nocturne-version` to match `include/update.hh`.
3. Deploy this folder to production.

Do not commit `vercel-updater/public/3hs.cia`; it is a generated release artifact.
