from functools import partial
from pathlib import Path

import glow
import numpy as np
import torchslide as ts
from matplotlib import pyplot as P


def read(scale, image):
    return scale, image[::scale, ::scale]


def read_open(scale, filename):
    image = ts.Image(filename)
    # return scale, image[::scale, ::scale]
    h, w, *_ = image.shape
    return scale, image[-1000:h + 1000:scale, -1000:w + 1000:scale]


def show(tile, scale):
    P.figure(figsize=(10, 10))
    P.imshow(tile)
    P.suptitle(f'Scale = 1:{scale}, shape = {tile.shape}')
    P.tight_layout()
    P.show()


print(f'Version: {ts.torchslide.__version__}')

n = 1  # 25
for p in sorted(Path('..').glob('*.svs')):
    filename = p.as_posix()
    print(filename)

    try:
        image = ts.Image(filename)
    except RuntimeError as exc:
        print(f'Dead: {exc!r}')
        continue
    print(image.shape, image.scales)

    scales = image.scales
    scales = [
        s for s in scales
        if np.prod(image.shape[:2], dtype='int64') / (s ** 2) < 2 ** 25
    ]
    with glow.timer('[main:open -> worker:read]'):
        tiles = glow.mapped(partial(read, image=image), scales * n)
        tiles = dict(tiles)

    del image
    with glow.timer('[worker:(open, read)]'):
        tiles = glow.mapped(partial(read_open, filename=filename), scales * n)
        tiles = dict(tiles)

    for scale, tile in tiles.items():
        show(tile, scale)
