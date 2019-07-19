import atlas
from matplotlib import pyplot as P


x = atlas.Image('../test.tif')
print(x.shape, x.scales)

for s in x.scales:
    if s > 8:
        P.imshow(x[::s, ::s])
        P.show()
