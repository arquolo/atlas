#include "FilelistSource.h"

#include <functional>
#include <fstream>
#include <stdexcept>
#include <boost/filesystem.hpp>

#include "multiresolutionimageinterface/MultiResolutionImageFactory.h"

namespace ASAP::Data
{
	FilelistSource::FilelistSource(const std::string filepath) : m_images_(GetImageFilelist_(filepath))
	{
	}

	WorklistSourceInterface::SourceType FilelistSource::GetSourceType(void)
	{
		return WorklistSourceInterface::SourceType::FILELIST;
	}

	size_t FilelistSource::AddWorklistRecord(const std::string& title, const std::function<void(const bool)>& observer)
	{
		return 0;
	}

	size_t FilelistSource::UpdateWorklistRecord(const std::string& worklist_index, const std::string title, const std::vector<std::string> images, const std::function<void(const bool)>& observer)
	{
		return 0;
	}

	size_t FilelistSource::DeleteWorklistRecord(const std::string& worklist_index, const std::function<void(const bool)>& observer)
	{
		return 0;
	}

	size_t FilelistSource::GetWorklistRecords(const std::function<void(DataTable&, const int)>& receiver)
	{
		return 0;
	}

	size_t FilelistSource::GetPatientRecords(const std::string& worklist_index, const std::function<void(DataTable&, const int)>& receiver)
	{
		return 0;
	}

	size_t FilelistSource::GetStudyRecords(const std::string& patient_index, const std::function<void(DataTable&, const int)>& receiver)
	{
		return 0;
	}

	size_t FilelistSource::GetImageRecords(const std::string& worklist_index, const std::string& study_index, const std::function<void(DataTable&, int)>& receiver)
	{
		receiver(m_images_, 0);
		return 0;
	}

	std::set<std::string> FilelistSource::GetWorklistHeaders(const DataTable::FIELD_SELECTION selectionL)
	{
		return std::set<std::string>();
	}

	std::set<std::string> FilelistSource::GetPatientHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return std::set<std::string>();
	}

	std::set<std::string> FilelistSource::GetStudyHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return std::set<std::string>();
	}

	std::set<std::string> FilelistSource::GetImageHeaders(const DataTable::FIELD_SELECTION selection)
	{
		return std::set<std::string>();
	}

	size_t FilelistSource::GetImageThumbnailFile(const std::string& image_index, const std::function<void(boost::filesystem::path)>& receiver, const std::function<void(uint8_t)>& observer)
	{
		receiver(boost::filesystem::path(*m_images_.At(std::stoi(image_index), { "location" })[0]));
		return 0;
	}

	size_t FilelistSource::GetImageFile(const std::string& image_index, const std::function<void(boost::filesystem::path)>& receiver, const std::function<void(uint8_t)>& observer)
	{
		receiver(boost::filesystem::path(*m_images_.At(std::stoi(image_index), { "location" })[0]));
		return 0;
	}

	DataTable FilelistSource::GetImageFilelist_(const std::string filepath)
	{
		std::ifstream stream(filepath);
		if (stream.is_open())
		{
			std::set<std::string> allowed_extensions = MultiResolutionImageFactory::getAllSupportedExtensions();
			DataTable images({ "id", "location", "title" });

			while (!stream.eof())
			{
				std::string line;
				std::getline(stream, line);

				boost::filesystem::path image_path(line);
				if (boost::filesystem::is_regular_file(image_path) && allowed_extensions.find(image_path.extension().string().substr(1)) != allowed_extensions.end())
				{
					images.Insert({ std::to_string(images.Size()), image_path.string(), image_path.filename().string() });
				}
			}
			return images;
		}
		throw std::runtime_error("Unable to open file: " + filepath);
	}
}