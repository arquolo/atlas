#include "LIFImageFactory.h"
#include "LIFImage.h"

const LIFImageFactory LIFImageFactory::registerThis;

LIFImageFactory::LIFImageFactory()
  : MultiResolutionImageFactory("Leica LIF", { "lif" }, 0) {}

std::unique_ptr<MultiResolutionImage> LIFImageFactory::readImage(const std::string& fileName) const {
    return std::make_unique<LIFImage>(filename);
}
