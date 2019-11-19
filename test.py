from functools import partial

import numpy as np
import torchslide as ts
from matplotlib import pyplot as P

from glow import mapped, timer


def read(scale, image):
    return scale, image[::scale, ::scale]


def read_open(scale, filename):
    image = ts.Image(filename)
    return scale, image[::scale, ::scale]


def show(tile, scale):
    P.figure(figsize=(10, 10))
    P.imshow(tile)
    P.suptitle(f'Scale = 1:{scale}, shape = {tile.shape}')
    P.tight_layout()
    P.show()


print(f'Version: {ts.torchslide.__version__}')
for ext in ('tif', 'svs'):
    filename = f'../test.{ext}'
    print(filename)

    image = ts.Image(filename)
    print(image.shape, image.scales)

    scales = image.scales
    scales = [
        s for s in scales
        if np.prod(image.shape[:2]) / (s ** 2) < 2 ** 25
    ]
    with timer('[main:open -> worker:read]'):
        tiles = mapped(partial(read, image=image), scales * 25)
        tiles = dict(tiles)

    del image
    with timer('[worker:(open, read)]'):
        tiles = mapped(partial(read_open, filename=filename), scales * 25)
        tiles = dict(tiles)

    for scale, tile in tiles.items():
        show(tile, scale)
