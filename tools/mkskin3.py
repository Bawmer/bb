# Regenerates the GUI skin from docs/design_reference.png. Run from tools/: python3 mkskin3.py (needs numpy, scipy, opencv-python, pillow)
import cv2, numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, uniform_filter1d
SRC = cv2.cvtColor(cv2.imread('../docs/design_reference.png'), cv2.COLOR_BGR2RGB)
src = SRC.astype(np.float32)
out_dir = '../Resources/skin/'

# ---------------------------------------------------------------- background
exec(open('mkskin2.py').read().split("arr = np.asarray(bg).copy()")[0].replace("src = Image.open('../docs/design_reference.png').convert('RGB')", "src = Image.open('../docs/design_reference.png').convert('RGB')"))
B = np.asarray(bg).copy()
src = SRC.astype(np.float32)
# remove the printed needles of the two main VU meters (orange-brown line), keep faces
for x0 in (438, 645):
    reg = B[874:952, x0 + 4:x0 + 190].astype(int)
    r, g, b = reg[..., 0], reg[..., 1], reg[..., 2]
    needle = (r > 120) & (g > 70) & (g < 175) & (b < 110) & (r - b > 70) & (g > b + 10) & ~((r > 170) & (g < 80))
    m = np.zeros(B.shape[:2], np.uint8)
    m[874:952, x0 + 4:x0 + 190] = needle.astype(np.uint8) * 255
    m = cv2.dilate(m, np.ones((5, 5), np.uint8))
    B = cv2.inpaint(B, m, 5, cv2.INPAINT_TELEA)
B[222:842, 118:1913] = (20, 18, 16)
Image.fromarray(B).save(out_dir + 'bg.jpg', quality=93)

# ---------------------------------------------------------------- faceplates from the real units
UX = [123, 351, 577, 801, 1026, 1253, 1472, 1692]
W, H = 219, 603
def clean_unit(x0, keep_screws=True, empty=False):
    u = src[229:229 + H, x0:x0 + W]
    lum = u.mean(2)
    bgm = cv2.medianBlur(np.clip(lum, 0, 255).astype(np.uint8), 31).astype(np.float32)
    mask = np.abs(lum - bgm) > 14
    sat = u.max(2) - u.min(2)
    mask |= sat > (np.median(sat) + 40)
    # known control regions
    yy, xx = np.mgrid[0:H, 0:W]
    if empty:
        mask = np.zeros_like(mask); mask[200:360, 14:W - 14] = True     # only the printed text
    for cy in (() if empty else (106, 214, 316)):
        for cx in (57, 160):
            mask |= (xx - cx) ** 2 + (yy - cy) ** 2 < 50 ** 2
    if not empty:
        mask[4:56, 10:W - 10] = True               # title + title LED glow
        mask[140:175, :] |= True; mask[245:280, :] |= True; mask[345:380, :] |= True   # label rows
        mask[380:600, 8:W - 8] = True              # switches, trims, VU, bottom label
    screws = np.zeros_like(mask)
    for sx, sy in ((14, 14), (W - 14, 14), (14, H - 14), (W - 14, H - 14)):
        screws |= (xx - sx) ** 2 + (yy - sy) ** 2 < 13 ** 2
    if keep_screws: mask &= ~screws
    mask = cv2.dilate(mask.astype(np.uint8), np.ones((5, 5), np.uint8)).astype(bool)
    if keep_screws: mask &= ~screws
    w = (~mask).astype(np.float32)
    # lighting: normalized convolution over clean pixels
    # lighting: smooth 2-D polynomial fitted to the clean pixels (robust under large masked areas)
    X = xx / W - 0.5; Y = yy / H - 0.5
    feats = [X ** i * Y ** j for i in range(5) for j in range(5) if i + j <= 4]
    A = np.stack([f[~mask] for f in feats], 1)
    Fall = np.stack([f.ravel() for f in feats], 1)
    L = np.zeros_like(u)
    for c in range(3):
        sm = gaussian_filter(u[..., c], 2)[~mask]
        coef, *_ = np.linalg.lstsq(A, sm, rcond=None)
        L[..., c] = (Fall @ coef).reshape(H, W)
    # grain: horizontal brushed noise matched to the clean grain level
    hp = (lum - gaussian_filter(lum, 3))[~mask]
    amp = float(1.4826 * np.median(np.abs(hp - np.median(hp)))) if hp.size else 2.5   # robust (MAD) grain level
    rng = np.random.default_rng(x0)
    n = rng.normal(0, 1, (H, W + 300))
    streak = uniform_filter1d(n, 70, axis=1)[:, 150:150 + W]
    fine = gaussian_filter(rng.normal(0, 1, (H, W)), (0.35, 4.0)); fine /= fine.std()
    grain = streak / streak.std() * amp * 0.8 + fine * amp * 0.7
    synth = L + grain[..., None]
    # keep real pixels where clean (feathered), synth elsewhere
    soft = gaussian_filter(w, 2.0)[..., None]
    outp = u * soft + synth * (1 - soft)
    return np.clip(outp, 0, 255).astype(np.uint8)

plates = {'graphite': 0, 'silver': 1, 'teal': 2, 'cream': 3, 'blue': 4, 'black': 5}
made = {}
for k, i in plates.items():
    made[k] = clean_unit(UX[i])
    Image.fromarray(made[k]).save(out_dir + f'plate_{k}.jpg', quality=93)
empty = src[229:229 + H, UX[6]:UX[6] + W].astype(np.uint8)
# empty panel text is drawn live -> clean its centre too
Image.fromarray(clean_unit(UX[6], empty=True)).save(out_dir + 'plate_empty.jpg', quality=93)

def tint(img, rgb, k=1.0):
    a = img.astype(np.float32); lum = a.mean(2, keepdims=True)
    return np.clip(lum / lum.mean() * np.array(rgb)[None, None, :] * k, 0, 255).astype(np.uint8)
Image.fromarray(tint(made['silver'], (122, 86, 52))).save(out_dir + 'plate_bronze.jpg', quality=93)
Image.fromarray(tint(made['black'], (120, 40, 36), 1.1)).save(out_dir + 'plate_red.jpg', quality=93)
Image.fromarray(tint(made['graphite'], (100, 110, 104))).save(out_dir + 'plate_hammer.jpg', quality=93)

# ---------------------------------------------------------------- sprites
def sprite(img, cx, cy, r, scale=4, desat=False):
    pad = int(r + 3)
    reg = img[int(cy) - pad:int(cy) + pad + 1, int(cx) - pad:int(cx) + pad + 1].astype(np.float32)
    if desat:
        l = reg.mean(2, keepdims=True); reg = np.repeat(l, 3, 2) * np.array([1.0, 1.0, 1.02])[None, None, :]
    S = (2 * pad + 1) * scale
    up = cv2.resize(reg, (S, S), interpolation=cv2.INTER_LANCZOS4)
    yy, xx = np.mgrid[0:S, 0:S]
    c = ((pad + cx - int(cx) + 0.5) * scale, (pad + cy - int(cy) + 0.5) * scale)
    d = np.sqrt((xx - c[0]) ** 2 + (yy - c[1]) ** 2) / scale
    a = np.clip((r - d) / 0.9, 0, 1) * 255
    R = int(np.ceil(r * scale)); x0 = int(c[0] - R); y0 = int(c[1] - R)
    o = np.dstack([np.clip(up, 0, 255), a])[y0:y0 + 2 * R, x0:x0 + 2 * R]
    return Image.fromarray(o.astype(np.uint8), 'RGBA')

S2 = SRC.copy()
pm = np.zeros(S2.shape[:2], np.uint8)
cv2.line(pm, (402, 310), (403, 325), 255, 5)          # baked pointer of the gold knob
S2 = cv2.inpaint(S2, pm, 4, cv2.INPAINT_TELEA)
sprite(SRC, 178.5, 334.5, 31.5, desat=True).save(out_dir + 'knob_chrome.png')
sprite(S2, 407.5, 335.0, 30.5).save(out_dir + 'knob_gold.png')
sprite(SRC, 161.0, 712.0, 23.0).save(out_dir + 'knob_trim.png')
sprite(SRC, 1373.0, 120.0, 32.5).save(out_dir + 'knob_top.png')

# unit VU without needle
vu = SRC[766:808, 165:299].copy()
r, g, b = [vu[..., i].astype(int) for i in range(3)]
needle = (r > 25) & (r > 0.62 * g) & (g > 30)
needle[:, 75:] = False                                   # keep the red zone on the right
needle[29:, 50:] = False                                   # keep the pivot dome
m = cv2.dilate(needle.astype(np.uint8) * 255, np.ones((3, 3), np.uint8))
m[:4, :] = 0; m[-4:, :] = 0
vu = cv2.inpaint(vu, m, 3, cv2.INPAINT_TELEA)
Image.fromarray(vu).resize((134 * 3, 42 * 3), Image.LANCZOS).save(out_dir + 'vu_unit.png')

# mini toggle (paddle up)
Image.fromarray(SRC[615:642, 506:528]).resize((22 * 4, 27 * 4), Image.LANCZOS).save(out_dir + 'toggle_up.png')
print('assets ok')
