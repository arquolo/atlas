#include "http.h"

#include <stdexcept>

#include "../misc/string_conversions.h"

namespace asap::network {

HTTP::HTTP(
    const std::wstring base_uri,
    const web::http::client::http_client_config& config
) : client_(base_uri, config) {
	// If the task throws an exception, assume the URL is incorrect
	try {
		web::http::http_request request {web::http::methods::GET};
		this->client_.request(request).wait();
	} catch (...) {
		throw std::runtime_error("Unable to connect to: " + misc::wide_string_to_string(base_uri));
	}
}

void HTTP::cancel_all_tasks() {
	std::lock_guard lk {this->access_};
	for (auto& [task_id, _] : this->active_tasks_)
	    this->cancel(task_id);
}

void HTTP::cancel(size_t task_id) {
	std::lock_guard lk {this->access_};
	auto entry = this->active_tasks_.find(task_id);
	if (entry == this->active_tasks_.end())
        return;

    auto& [task_id, [token, task]] = *entry;
	if (!task.is_done())
		token.cancel();
	this->active_tasks_.erase(task_id);
}

size_t HTTP::enqueue(
    const web::http::http_request& request,
    std::function<void(web::http::http_response&)> observer,
) {
	std::lock_guard lk {this->access_};
	size_t token_id = this->token_counter_;
	this->active_tasks_.insert({ this->token_counter_, TokenTaskPair() });
	auto inserted_pair(this->active_tasks_.find(token_id));
	++this->token_counter_;

	// Catches the response so the attached token can be removed.
	inserted_pair->second.task = std::move(
        this->client_
            .request(request, inserted_pair->second.token.get_token())
            .then([observer, token_id, this](auto response) {
			     // Passes the response to the observer
			     observer(response);

			     // Remove token
			     this->cancel(token_id);
		    })
    );
	return token_id;
}

} // namespace asap::network