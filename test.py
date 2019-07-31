import atlas
import numpy as np
from matplotlib import pyplot as P


for ext in ('tif', 'svs'):
    filename = f'../test.{ext}'
    print(filename)

    x = atlas.Image(filename)
    print(x.shape, x.scales)

    for scale in x.scales:
        if np.prod(x.shape[:2]) // scale >= 2**28:
            continue
        P.imshow(x[::scale, ::scale], label=f'Scale = 1:{scale}')
        P.show()
