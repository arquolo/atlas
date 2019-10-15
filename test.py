import numpy as np
import torchslide as ts
from matplotlib import pyplot as P


for ext in ('tif', 'svs'):
    filename = f'../test.{ext}'
    print(filename)

    img = ts.Image(filename)
    print(img.shape, img.scales)

    for scale in img.scales:
        if np.prod(img.shape[:2]) // (scale ** 2) >= 2**23:
            continue
        P.figure(figsize=(10, 10))

        P.subplot(121 if img.shape[0] > img.shape[1] else 211)
        P.imshow(img[::scale, ::scale])

        P.subplot(122 if img.shape[0] > img.shape[1] else 212)
        xo = img.shape[0] // 10
        yo = img.shape[1] // 10
        tile = img[-xo:img.shape[0] + xo:scale, -yo:img.shape[1] + yo:scale]
        P.imshow(tile)

        P.suptitle(f'Scale = 1:{scale}')
        P.tight_layout()
        P.show()
