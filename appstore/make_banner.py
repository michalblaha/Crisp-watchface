from PIL import Image, ImageDraw, ImageFont, ImageOps

W, H = 720, 320
ARIAL_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
ARIAL = "/System/Library/Fonts/Supplemental/Arial.ttf"

# Vertical gradient background (charcoal, slightly lighter at the top).
top = (26, 29, 38)
bot = (12, 13, 17)
bg = Image.new("RGB", (W, H))
px = bg.load()
for y in range(H):
    t = y / (H - 1)
    r = int(top[0] + (bot[0] - top[0]) * t)
    g = int(top[1] + (bot[1] - top[1]) * t)
    b = int(top[2] + (bot[2] - top[2]) * t)
    for x in range(W):
        px[x, y] = (r, g, b)

draw = ImageDraw.Draw(bg)

# Watchface, scaled crisp (nearest), with a thin bezel border.
wf = Image.open("appstore/screenshots/emery.png").convert("RGB")
target_h = 268
target_w = round(wf.width * target_h / wf.height)
wf = wf.resize((target_w, target_h), Image.NEAREST)
wf = ImageOps.expand(wf, border=3, fill=(58, 61, 68))
wx = 52
wy = (H - wf.height) // 2
bg.paste(wf, (wx, wy))

# Text column to the right of the watchface.
tx = wx + wf.width + 44
title_font = ImageFont.truetype(ARIAL_BOLD, 84)
tag_font = ImageFont.truetype(ARIAL, 27)
feat_font = ImageFont.truetype(ARIAL, 19)

draw.text((tx, 74), "Crisp", font=title_font, fill=(255, 255, 255))
# Gold accent underline (echoes the day-of-month colour).
draw.rectangle([tx + 4, 176, tx + 4 + 104, 176 + 5], fill=(245, 197, 24))
draw.text((tx + 2, 196), "Analog watchface, your way", font=tag_font, fill=(196, 199, 206))
draw.text((tx + 2, 244), "Battery  -  Heart rate  -  Steps  -  Weather", font=feat_font, fill=(135, 139, 148))

bg.save("appstore/banner.png")
print("saved appstore/banner.png", bg.size)
