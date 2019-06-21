#pragma once

#include <cpprest/http_client.h>

#include "Data/DataTable.h"

namespace asap::serialize::json {
	web::json::object tag_recursive(
        std::wstring tag, const web::json::value& json);

	std::vector<std::vector<std::string>> json_fields(
        const web::http::http_response& response,
        const std::vector<std::string>& fields);

	Data::DataTable records(const web::http::http_response& response);
	Data::DataTable table_schema(const web::http::http_response& response);
}
