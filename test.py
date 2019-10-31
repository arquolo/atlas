from functools import partial

import numpy as np
import torchslide as ts
from matplotlib import pyplot as P

from glow import mapped, timer


def read(image, scale):
    return scale, image[::scale, ::scale]


def read_open(filename, scale):
    image = ts.Image(filename)
    return scale, image[::scale, ::scale]


def show(tile, scale):
    P.figure(figsize=(10, 10))
    P.imshow(tile)
    P.suptitle(f'Scale = 1:{scale}, shape = {tile.shape}')
    P.tight_layout()
    P.show()


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
    with timer('getitem'):
        tiles = mapped(partial(read, image=image), scales)
        tiles = dict(tiles)

    del image
    with timer('open+getitem'):
        tiles = mapped(partial(read_open, filename=filename), scales)
        tiles = dict(tiles)

    for scale, tile in tiles.items():
        show(tile, scale)
