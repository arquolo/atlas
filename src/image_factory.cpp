#include "multi_resolution_image_factory.h"
#include "multi_resolution_image.h"

#include "core/filetools.h"
#include "core/string_conversion.h"

#ifdef HAS_MULTIRESOLUTIONIMAGEINTERFACE_VSI_SUPPORT
#include "VSIImage.h"
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

bool MultiResolutionImageFactory::external_formats_registered_ = false;
std::set<std::string> MultiResolutionImageFactory::supported_extensions_;

MultiResolutionImageFactory::FactoryMap& MultiResolutionImageFactory::registry() {
    static FactoryMap type_registry;
    return type_registry;
}

void MultiResolutionImageFactory::add_supported(
    const std::set<std::string>& extensions
) {
    supported_extensions_.insert(extensions.begin(), extensions.end());
}

MultiResolutionImageFactory::MultiResolutionImageFactory(
    const std::string& factory_name,
    const std::set<std::string>& extensions,
    size_t priority
) : factory_name_(factory_name)
  , priority_(priority) {
    registry()[factory_name] = std::make_pair(extensions, this);
    add_supported(extensions);
}

std::vector<std::pair<std::string, std::set<std::string>>>
MultiResolutionImageFactory::get_loaded_factories_and_supported_extensions() {
    MultiResolutionImageFactory::register_external_file_formats();
    std::vector<std::pair<std::string, std::set<std::string>>>
        factories_and_extensions;
    for (auto& [factory, extension] : registry())
        factories_and_extensions.emplace_back(factory, extension.first);
    return factories_and_extensions;
}

std::set<std::string> MultiResolutionImageFactory::supported_extensions() {
  MultiResolutionImageFactory::register_external_file_formats();
  return supported_extensions_;
}

bool MultiResolutionImageFactory::operator<(
    const MultiResolutionImageFactory& other
) const {
    return this->priority_ < other.priority_;
}


std::shared_ptr<MultiResolutionImage>
MultiResolutionImageFactory::open(
    const std::string& fileName,
    const std::string factoryName
) {
    MultiResolutionImageFactory::register_external_file_formats();
    if (factoryName == "default") {
        std::vector<MultiResolutionImageFactory*> suitableFactoriesByPriority;
        for (auto it = registry().begin(); it != registry().end(); ++it) {
            std::string extension = core::extract_file_extension(fileName);
            const std::set<std::string>& supportedExtensions = it->second.first;
            if (std::find(
                    supportedExtensions.begin(), supportedExtensions.end(),
                    extension)
                    != supportedExtensions.end())
                suitableFactoriesByPriority.push_back(it->second.second);
        }
        std::sort(suitableFactoriesByPriority.begin(), suitableFactoriesByPriority.end(), [](MultiResolutionImageFactory* a, MultiResolutionImageFactory* b) {return (*a) < (*b);});
        for (auto it = suitableFactoriesByPriority.begin(); it != suitableFactoriesByPriority.end(); ++it) {
            MultiResolutionImage* img = MultiResolutionImageFactory::openImageWithFactory(fileName, *it);
            if (img)
                return img;
        }
    } else {
        auto requestedFactory = registry().find(factoryName);
        if (requestedFactory != registry().end()) {
            return MultiResolutionImageFactory::openImageWithFactory(fileName, requestedFactory->second.second);
        }
    }
    return nullptr;
}

MultiResolutionImage* MultiResolutionImageFactory::openImageWithFactory(const std::string& fileName, const MultiResolutionImageFactory* factory)
{
  MultiResolutionImage* img = factory->readImage(fileName);
  if (img) {
#ifdef HAS_MULTIRESOLUTIONIMAGEINTERFACE_VSI_SUPPORT
    if ((img->getNumberOfLevels() > 1 || dynamic_cast<VSIImage*>(img) != NULL) || (img->getNumberOfLevels() == 1 && img->getLevelDimensions(0)[0] < 4096)) {
#else
    if ((img->getNumberOfLevels() > 1) || (img->getNumberOfLevels() == 1 && img->getLevelDimensions(0)[0] < 4096)) {
#endif
      return img;
    }
    else {
      delete img;
    }
  }
  return nullptr;
}

using FileFormatLoader = void (*)();

void MultiResolutionImageFactory::registerExternalFileFormats() {
  if (MultiResolutionImageFactory::_externalFormatsRegistered) {
    return;
  }
  std::string pathStr;
#ifdef _WIN32
  char path[2048];
  HMODULE hm = NULL;

  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    (LPCSTR)&MultiResolutionImageFactory::registerExternalFileFormats,
    &hm))
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleHandle returned %d\n", ret);
  }
  GetModuleFileNameA(hm, path, sizeof(path));
  pathStr = std::string(path);
#else
  Dl_info dlInfo;
  dladdr((void*)&MultiResolutionImageFactory::registerExternalFileFormats, &dlInfo);
  pathStr = std::string(dlInfo.dli_fname);
#endif
  std::string rootDir = core::extractFilePath(pathStr);
  std::vector<std::string> formatPlugins;
#ifdef _WIN32
  std::string fileFormatPluginDir = core::completePath("formats", rootDir);
  core::getFiles(fileFormatPluginDir, "*.dll", formatPlugins);
#elif __APPLE__
  std::string fileFormatPluginDir = core::completePath("bin/formats", core::upOneLevel(rootDir));
  core::getFiles(fileFormatPluginDir, "*.dylib", formatPlugins);
#else
  std::string fileFormatPluginDir = core::completePath("bin/formats", core::upOneLevel(rootDir));
  core::getFiles(fileFormatPluginDir, "*.so", formatPlugins);
#endif
  for (std::vector<std::string>::const_iterator it = formatPlugins.begin(); it != formatPlugins.end(); ++it) {
#ifdef _WIN32
    SetDllDirectory(rootDir.c_str());
    HMODULE formatLib = LoadLibrary(it->c_str());
    if (formatLib) {
      FileFormatLoader loadfunction = (FileFormatLoader)GetProcAddress(formatLib, "filetypeLoad");
      if (loadfunction) {
        (*loadfunction)();
      }
    }
    else {
      DWORD test = GetLastError();
    }
#else
    void *hndl = dlopen(core::completePath((*it), fileFormatPluginDir).c_str(), RTLD_NOW);
    if(hndl != NULL){
      FileFormatLoader loadfunction = (FileFormatLoader)dlsym(hndl, "filetypeLoad");
      if (loadfunction) {
        (*loadfunction)();
      }
    }
#endif
  }
  MultiResolutionImageFactory::_externalFormatsRegistered = true;
}
