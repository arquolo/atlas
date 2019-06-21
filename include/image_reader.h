#pragma once

#include "multiresolutionimage_interface_export.h"

#include "multiresolutionimage.h"
#include "core/filetools.h"

#include "multiresolutionimage_factory.h"

#include <string>

class MultiResolutionImage;

class EXPORT MultiResolutionImageReader {
public:
    //! Opens the slide file and keeps a reference to it
    MultiResolutionImage* open(
        const std::string& file_name,
        const std::string factory_name = "default",
    ) {
        return MultiResolutionImageFactory::open(file_name, factory_name);
    }
};
