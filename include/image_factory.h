#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>

class Image;

class ImageFactory {
	static bool external_formats_registered_;
    static std::set<std::string> extensions_;

    const std::string factory_name_;
    const size_t priority_;

protected:
    using FactoryMap = std::map<
        std::string,
        std::pair<std::set<std::string>, ImageFactory*>>;

    static FactoryMap& registry();
    static void add_supported(const std::set<std::string>& extensions);

    static std::shared_ptr<Image>
    open_with_factory(
        const std::string& filename, const ImageFactory* factory);

    virtual std::shared_ptr<Image>
    read_image(const std::string& filename) const = 0;

    virtual bool can_read_image(const std::string& fileName) const = 0;

public:
    ImageFactory(
        const std::string& factory_name,
        const std::set<std::string>& extensions,
        size_t priority
    );

    static std::shared_ptr<Image> open(
        const std::string& filename,
        const std::string factory_name = std::string("default"));

    static void register_external_file_formats();

    static std::vector<std::pair<std::string, std::set<std::string>>>
    get_loaded_factories_and_supported_extensions();

    static std::set<std::string> supported_extensions();
    bool operator< (const ImageFactory &other) const;

};
