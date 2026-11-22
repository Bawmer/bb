from PIL import Image, ImageOps, ImageFilter
import numpy as np
src = Image.open('../docs/design_reference.png').convert('RGB')
bg = src.copy()

def synth(box, w, h, horiz=True):
    tile = src.crop(box); tw, th = tile.size
    out = Image.new('RGB', (w, h))
    for y in range(0, h, th):
        for x in range(0, w, tw):
            t = tile
            if (x // tw) % 2: t = ImageOps.mirror(t)
            if (y // th) % 2 and not horiz: t = ImageOps.flip(t)
            out.paste(t, (x, y))
    return out

def patch(box, srcx0, srcx1, feather=6):
    """Lighting-preserving fill: per-row lerp between the box's left/right edges + grain from columns srcx0..srcx1."""
    x0, y0, x1, y1 = box
    A = np.asarray(bg).astype(np.float32)
    w, h = x1 - x0, y1 - y0
    left = A[y0:y1, x0 - 4:x0].mean(1); right = A[y0:y1, x1:x1 + 4].mean(1)
    t = np.linspace(0, 1, w)[None, :, None]
    base = left[:, None, :] * (1 - t) + right[:, None, :] * t
    strip = np.asarray(src).astype(np.float32)[y0:y1, srcx0:srcx1]
    detail = strip - strip.mean(1, keepdims=True)
    reps = int(np.ceil(w / detail.shape[1]))
    tiles = [detail if i % 2 == 0 else detail[:, ::-1] for i in range(reps)]
    detail = np.concatenate(tiles, 1)[:, :w]
    out = np.clip(base + detail, 0, 255)
    xx = np.arange(w)[None, :].repeat(h, 0); yy = np.arange(h)[:, None].repeat(w, 1)
    d = np.minimum.reduce([xx, w - 1 - xx, yy, h - 1 - yy])
    m = np.clip(d / feather, 0, 1)[..., None]
    A[y0:y1, x0:x1] = A[y0:y1, x0:x1] * (1 - m) + out * m
    bg.paste(Image.fromarray(A.astype(np.uint8)))

patch((1598, 44, 1850, 80), 1060, 1100, 8)       # latency text
patch((918, 890, 1150, 950), 872, 914, 12)      # caption next to LUFS
patch((410, 1030, 1900, 1094), 280, 400)      # GR LEDs + labels
arr = np.asarray(bg).copy()
arr[222:842, 118:1913] = (20, 18, 16)        # rack interior (units are drawn live)
Image.fromarray(arr).save('s2/bg.jpg', quality=93)

# procedural brushed metal 220 x 603, tinted per module colour
from scipy.ndimage import uniform_filter1d, gaussian_filter
rng = np.random.default_rng(7)
n = rng.normal(0, 1, (603, 520))
streak = uniform_filter1d(n, 90, axis=1)[:, 150:370] * 9.0          # long horizontal brush lines
fine = gaussian_filter(rng.normal(0, 1, (603, 220)), (0.4, 3.0)) * 2.2
g = streak / streak.std() * 3.2 + fine
cols = {
    'graphite': (104, 104, 108), 'silver': (112, 109, 104), 'black': (58, 54, 50), 'teal': (46, 98, 112),
    'cream': (202, 172, 134), 'blue': (78, 97, 117), 'bronze': (118, 84, 52), 'red': (118, 38, 36), 'empty': (66, 61, 57),
}
for k, c in cols.items():
    k_ = 0.8 + (sum(c) / 3) / 110
    out = np.clip(np.array(c, np.float32)[None, None, :] + g[..., None] * k_, 0, 255).astype(np.uint8)
    Image.fromarray(out).save(f's2/plate_{k}.jpg', quality=92)
print('done')
