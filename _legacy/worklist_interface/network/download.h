#pragma once

#include <functional>
#include <string> 
#include <thread>

#include <boost/filesystem.hpp>
#include <cpprest/filestream.h>
#include <cpprest/http_client.h>

namespace asap::network {

enum class DownloadStatus {
    DOWNLOAD_FAILURE,
    FILE_CREATION_FAILURE,
    NO_ATTACHMENT,
    SUCCESS,
};

struct FileDownloadResult {
	boost::filesystem::path filepath;
	DownloadStatus status;
};

/// <summary>
/// Downloads a file that may be tied to a HTTP response.
/// </summary>
/// <param name="response">The response holding the file.</param>
/// <param name="output_directory">The directory to write the image to.</param>
/// <return>A struct containing the absolute path to the downloaded file (empty on failure) and the state of the download.</return>
FileDownloadResults download(
    const web::http::http_response& response,
    const boost::filesystem::path& output_directory,
    std::function<void(uint8_t)> observer = {}
);

namespace {
	bool has_correct_size(
        const boost::filesystem::path& filepath, 
        size_t size);

	bool is_unique(
        const boost::filesystem::path& filepath,
        size_t size);

	void fix_filepath(boost::filesystem::path& filepath);

	std::thread monitor_thread(
        const bool& stop,
        const size_t length,
        concurrency::streams::ostream& stream,
        std::function<void(uint8_t)>& observer,
    );
}

} // asap::network
