import math
from PIL import Image, ImageDraw

# Render a Crisp-style clock dial at high resolution, then downscale to the
# requested icon sizes for clean antialiasing.
SS = 8            # supersampling factor
BASE = 144        # master is BASE*SS, then resized to each target
SIZES = [144, 80]

M = BASE * SS
cx = cy = M / 2.0
R = M / 2.0

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY = (170, 170, 170)
RED = (170, 0, 0)
GOLD = (255, 255, 0)


def s(v):
    return v * SS


def pt(angle_deg, length):
    a = math.radians(angle_deg)
    return (cx + math.sin(a) * length, cy - math.cos(a) * length)


def hand(draw, angle, length, tip_len, body_col, tip_col, width):
    joint = pt(angle, length - tip_len)
    tip = pt(angle, length)
    draw.line([(cx, cy), joint], fill=body_col, width=int(width))
    draw.line([joint, tip], fill=tip_col, width=int(width))


img = Image.new("RGB", (M, M), BLACK)
d = ImageDraw.Draw(img)

# Hour markers: 12 thin ticks hugging the rim.
marker_outer = R - s(8)
tick_len = s(13)
for i in range(12):
    ang = i * 30
    outer = pt(ang, marker_outer)
    inner = pt(ang, marker_outer - tick_len)
    d.line([inner, outer], fill=WHITE, width=int(s(6)))

# Hands posed at 10:10 (classic), with a red second hand for a splash of colour.
minute_angle = 10 / 60.0 * 360.0            # 60 deg
hour_angle = (10 + 10 / 60.0) / 12.0 * 360.0  # ~305 deg
second_angle = 40 / 60.0 * 360.0            # 240 deg

hand(d, hour_angle, s(38), s(11), GRAY, WHITE, s(8))
hand(d, minute_angle, s(56), s(11), GRAY, WHITE, s(8))
hand(d, second_angle, s(60), s(14), RED, GOLD, s(3))

# Center cap.
r = s(6)
d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=WHITE)

for size in SIZES:
    out = img.resize((size, size), Image.LANCZOS)
    out.save("appstore/icon-%d.png" % size)
    print("saved appstore/icon-%d.png" % size, out.size)
