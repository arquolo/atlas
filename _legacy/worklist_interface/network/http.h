#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include <cpprest/http_client.h>

namespace asap::network {

/// <summary>
/// Represents a connection towards a specified URI.
/// <summary>
class HTTP {
	struct TokenTaskPair {
		pplx::task<void> task;
		concurrency::cancellation_token_source token;
	};

	web::http::client::http_client client_;
	size_t token_counter_ = 0;
	std::unordered_map<size_t, TokenTaskPair> active_tasks_ = {};

protected:
    std::mutex access_;

public:
	HTTP(
        const std::wstring base_uri,
        const web::http::client::http_client_config& config = {},
    );
	~HTTP() {
		this->cancel_all_tasks();
	}

    void cancel_all_tasks();
    void cancel(size_t task_id);

	/// <summary>
	/// Allows the connection to handle the request, and returns the information to the passed observer function.
	/// </summary>
	size_t enqueue(
        const web::http::http_request& request,
        std::function<void(web::http::http_response&)> observer,
    );

	/// <summary>
	/// Sends the request through the internal client and returns the async task handling the request.
	/// </summary>
	pplx::task<web::http::http_response>
    send(const web::http::http_request& request) {
        return this->client_.request(request);
    }

};

} // namespace asap::network
