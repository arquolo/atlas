#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include <cpprest/http_client.h>

#include "http.h"

namespace asap::network {

/// <summary>
/// Represents a connection towards a specified Django URI that handles authentication.
/// Retains information until this object is destructed, so potential connects can be refreshed.
/// <summary>
class Django : public HTTP_Connection {
public:
	using Credentials = std::unordered_map<std::string, std::wstring>;
	enum class AuthenticationType {
        NONE,
        SESSION,
        TOKEN,
    };
	enum class AuthenticationStatus {
        AUTHENTICATED,
        UNAUTHENTICATED,
        INVALID_CREDENTIALS
    };

	Django(
        const std::wstring base_uri,
        const AuthenticationType authentication_type = AuthenticationType::NONE,
        const Credentials credentials = {},
        const web::http::client::http_client_config& config = web::http::client::http_client_config(),
    );

	static Credentials
    create_credentials(const std::wstring token, const std::wstring validation_path);
	static Credentials static
    create_credentials(const std::wstring username, const std::wstring password, const std::wstring csrf_path, const std::wstring auth_path);
	
    const Credentials& credentials();
	void credentials(const Credentials credentials);

	AuthenticationStatus authentication_status() const;

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
    send(const web::http::http_request& request);

private:
	AuthenticationType m_authentication_;
	Credentials m_credentials_;
	AuthenticationStatus m_status_;

	bool token(web::http::http_response& response);
	void modify(web::http::http_request& request);
	void setup_connection();
	void validate(Credentials& credentials);
};

} // namespace asap::network
