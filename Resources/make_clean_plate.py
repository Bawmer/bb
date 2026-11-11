import cv2, numpy as np
im = cv2.imread('face.jpg').astype(np.float32)
H, W = im.shape[:2]
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)

knobs = [(110,560),(289,560),(466,560),(644,560),(822,560),(1000,560),(1178,560),
         (110,727),(289,727),(466,727),(644,727),(823,727),(1001,727),(1179,727),
         (945,188),(945,289),(946,391)]

# refine centres: centroid of the bright cap disc (r<24)
g = cv2.cvtColor(im.astype(np.uint8), cv2.COLOR_BGR2GRAY).astype(np.float32)
refined = []
for cx, cy in knobs:
    best = None
    for dx in range(-4, 5):
        for dy in range(-4, 5):
            r = np.hypot(xx[cy-50:cy+50, cx-50:cx+50]-(cx+dx), yy[cy-50:cy+50, cx-50:cx+50]-(cy+dy))
            ring = (r > 29.5) & (r < 33)          # dark gap just outside the cap
            s = g[cy-50:cy+50, cx-50:cx+50][ring].mean()
            if best is None or s < best[0]: best = (s, cx+dx, cy+dy)
    refined.append((cx, cy))
print(refined)

out = im.copy()
hsv = cv2.cvtColor(im.astype(np.uint8), cv2.COLOR_BGR2HSV)
glowMask = np.zeros((H, W), np.uint8)
for cx, cy in refined:
    r = np.hypot(xx-cx, yy-cy)
    samples = np.concatenate([im[cy-25:cy-15, cx-66:cx-58].reshape(-1,3), im[cy-25:cy-15, cx+58:cx+66].reshape(-1,3),
                              im[cy-60:cy-54, cx-8:cx+8].reshape(-1,3)])
    base = np.median(samples, axis=0)
    dev = np.abs(im - base).max(-1)
    m = ((r >= 41.0) & (r < 64) & (dev > 9)) | ((r >= 29.0) & (r < 44.5))
    m[cy+52:, :] = False                     # never touch the value box below
    m[:cy-45, :] = False                     # never touch the label above
    glowMask |= m.astype(np.uint8)
    # cap tick: locally bright marks in the upper sector of the cap
    ang = np.degrees(np.arctan2(xx-cx, cy-yy))
    sector = (r > 9) & (r < 30) & (np.abs(ang) < 40)
    hp = g - cv2.GaussianBlur(g, (0,0), 2.5)
    tick = (sector & (hp > 30)).astype(np.uint8)
    glowMask |= cv2.dilate(tick, np.ones((3,3),np.uint8))
out = cv2.inpaint(out.astype(np.uint8), glowMask, 4, cv2.INPAINT_TELEA).astype(np.float32)

# bezel annulus repaint
for cx, cy in refined:
    r = np.hypot(xx-cx, yy-cy)
    t = np.clip((r - 30.0) / 13.0, 0, 1)
    col = np.stack([ (6 + 10*t), (8 + 9*t), (11 + 9*t) ], -1)          # BGR: near-black, slightly lighter outward
    rim = np.exp(-((r-42.3)**2)/1.2)[..., None] * np.array([38, 30, 24])  # faint blue-steel rim
    col = col + rim
    a = np.clip(r - 29.0, 0, 1) * np.clip(43.6 - r, 0, 1)
    out = out*(1-a[...,None]) + col*a[...,None]

# restore label text the glow cleanup may have touched (top-right column)
orig = cv2.imread('face.jpg').astype(np.float32)
for (x0,y0,x1,y1) in [(900,131,992,149),(912,230,980,249),(888,332,1004,351),(914,435,976,454)]:
    out[y0:y1, x0:x1] = orig[y0:y1, x0:x1]

# value boxes: clear text
boxes = [(x,614,100,24) for x in (62,240,418,595,773,950,1128)] + [(x,780,100,23) for x in (61,239,417,595,773,951,1129)]
for x,y,w,h in boxes:
    out[y+1:y+h-1, x+2:x+w-2] = np.array([4,4,5])

# HUD panel interior
out[150:396, 1013:1254] = np.array([10, 9, 9])

# lit M/S LED -> copy the unlit 1:1 button
src = out[417-18:417+19, 1135-18:1135+19].copy()
rr = np.hypot(*np.mgrid[-18:19, -18:19])
a = np.clip(17.5 - rr, 0, 1)[..., None]
out[417-18:417+19, 1050-18:1050+19] = out[417-18:417+19, 1050-18:1050+19]*(1-a) + src*a

# LCD text: per-row background from a text-free column
for y in range(38, 94):
    row = out[y, 1140:1152].mean(0)
    out[y, 955:1153] = row

# Screen interior (rounded) -> dark glass, live scope draws over it
cv2.imwrite('face_clean.png', np.clip(out,0,255).astype(np.uint8))
open('knobs.txt','w').write(repr(refined))
