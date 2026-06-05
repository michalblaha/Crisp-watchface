# Crisp — appstore assets

Ready-to-upload assets for the Pebble / Rebble appstore
(https://dev-portal.rebble.io). Screenshots are captured at each platform's
exact native screen resolution, which is what the store expects (it renders them
inside a watch frame for you).

## Screenshots (`screenshots/`)

| File | Platform | Pixels |
|------|----------|--------|
| `emery.png`   | Pebble Time 2          | 200 x 228 |
| `gabbro.png`  | Pebble (round, RePebble) | 260 x 260 |
| `chalk.png`   | Pebble Time Round      | 180 x 180 |
| `basalt.png`  | Pebble Time            | 144 x 168 |
| `diorite.png` | Pebble 2               | 144 x 168 |
| `aplite.png`  | Pebble / Pebble Steel  | 144 x 168 |
| `flint.png`   | RePebble (B&W)         | 144 x 168 |

Upload the screenshot matching each hardware platform you publish for (up to 5
per platform are allowed; one is enough for a watchface).

## Marketing banner (`banner.png`)

`banner.png` — exactly 720 x 320, the standard Pebble appstore header/banner size.

## Icons (`icon-80.png`, `icon-144.png`)

Square PNG app icons rendered as a Crisp-style dial (thin markers, gray hands
with white tips, red second hand): `icon-80.png` (80 x 80) and `icon-144.png`
(144 x 144). Rebuild with `python3 appstore/make_icon.py`.

## Listing text

See `DESCRIPTION.md` for the title, tagline, short and full descriptions.

## Regenerating the screenshots

```bash
pebble build
for p in aplite basalt chalk diorite emery flint gabbro; do
  pebble install --emulator "$p"
  sleep 9                                   # let the weather relay populate
  pebble screenshot --no-open --emulator "$p"
done
```

Then move each `pebble_screenshot_*.png` to `appstore/screenshots/<platform>.png`.

Rebuild the banner (needs `python3` + Pillow), run from the repo root:

```bash
python3 appstore/make_banner.py
```

It composes `appstore/banner.png` from `appstore/screenshots/emery.png`.
