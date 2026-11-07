# tint_tiles.py - generate per-level tinted tile atlases from the base Kenney atlas.
# Usage: python scripts/tint_tiles.py <input.png> <out_dir>
# Produces tiles_blue.png / tiles_green.png / tiles_red.png (RGB channel multiply, alpha kept).
import sys
import os
from PIL import Image

TINTS = {
    "blue":  (0.62, 0.70, 1.00),
    "green": (0.62, 1.00, 0.68),
    "red":   (1.00, 0.60, 0.55),
}

def main() -> int:
    if len(sys.argv) != 3:
        print("usage: tint_tiles.py <input.png> <out_dir>", file=sys.stderr)
        return 1
    src_path, out_dir = sys.argv[1], sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)
    img = Image.open(src_path).convert("RGBA")
    px = img.load()
    w, h = img.size
    for name, (mr, mg, mb) in TINTS.items():
        out = Image.new("RGBA", (w, h))
        opx = out.load()
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                opx[x, y] = (int(r * mr), int(g * mg), int(b * mb), a)
        dst = os.path.join(out_dir, f"tiles_{name}.png")
        out.save(dst)
        print("wrote", dst)
    return 0

if __name__ == "__main__":
    sys.exit(main())
