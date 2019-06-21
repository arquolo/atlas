#include "django.h"

#include <functional>
#include <stdexcept>

namespace asap::network {

Django::Django(
    const std::wstring base_uri,
    const AuthenticationType authentication_type,
    const Credentials credentials,
    const web::http::client::http_client_config& config
) : HTTP(base_uri, config)
  , m_authentication_(authentication_type)
  , m_credentials_(credentials)
  , m_status_(UNAUTHENTICATED) {
    this->setup_connection();
}

Django::Credentials Django::create_credentials(
    const std::wstring token,
    const std::wstring validation_path
) {
	Credentials credentials;
	credentials.insert({ "token", token });
	credentials.insert({ "validation", validation_path });
	return credentials;
}

Django::Credentials Django::create_credentials(
    const std::wstring username,
    const std::wstring password,
    const std::wstring csrf_path,
    const std::wstring auth_path
) {
	Credentials credentials;
	credentials.insert({ "username", username });
	credentials.insert({ "password", password });
	credentials.insert({ "csrf", csrf_path });
	credentials.insert({ "auth", auth_path });
	return credentials;
}

Django::Credentials const& Django::credentials() {
	return m_credentials_;
}

void Django::credentials(Credentials const& credentials) {
	std::lock_guard {m_access_mutex};
	cancel_all_tasks();
	m_credentials_ = credentials;
	setup_Django_();
}

Django::AuthenticationStatus Django::authentication_status() const {
	return m_status_;
}

size_t Django::enqueue(
    const web::http::http_request& request,
    std::function<void(web::http::http_response&)> observer,
) {
    web::http::http_request authenticated_request {request};
    this->modify(authenticated_request);
	return HTTP_Connection::enqueue(authenticated_request, observer);
}

pplx::task<web::http::http_response>
Django::send(const web::http::http_request& request) {
    web::http::http_request authenticated_request {request};
	this->modify(authenticated_request);
	return HTTP_Connection::send(authenticated_request);
}

bool Django::token(web::http::http_response& response) {
	auto it = response.headers().find(L"Set-Cookie");
	if (it == response.headers().end() || it->second.find(L"csrf") == std::string::npos)
		return false;
		
    m_credentials_.insert({ "token", it->second });
    return true;
}

void Django::modify(web::http::http_request& request) {
	if (m_authentication_ == TOKEN)
		request.headers().add(L"Authorization", std::wstring(L"Token ").append(m_credentials_["token"]));

	else if (m_authentication_ == SESSION) {
		request.headers().add(L"X-CSRFToken", m_credentials_["cookie"]);
		request.headers().add(L"CSRF_COOKIE", m_credentials_["cookie"]);
		request.headers().add(L"Cookie", L"csrf_token=" + m_credentials_["cookie"]);
	}
}

void Django::setup_connection() {
	// Ensures the credentials contain the required information.
	this->validate(m_credentials_);

	try {
		if (m_authentication_ == TOKEN) {
			web::http::http_request token_test(web::http::methods::GET);
			this->modify(token_test);
			token_test.set_request_uri(m_credentials_["validation"]);

			AuthenticationStatus* status_ptr = &m_status_;
			HTTP::send(token_test)
                 .then([status_ptr](const auto& response) {
				    if (response.status_code() != web::http::status_codes::OK)
					    *status_ptr = AuthenticationStatus::INVALID_CREDENTIALS;
				 })
                .wait();
		} else if (m_authentication_ == SESSION) {
			web::http::http_request cookie_request {web::http::methods::GET};
			cookie_request.request_uri(m_credentials_["csrf"]);

			Credentials* cred_ptr = &m_credentials_;
			HTTP::send(cookie_request)
                 .then([cred_ptr](const auto& response) {
					auto it = response.headers().find(L"Set-Cookie");
					if (it != response.headers().end() && it->second.find(L"csrf") != std::string::npos)
						cred_ptr->insert({ "cookie", it->second });
				 })
                .wait();

			if (m_credentials_.find("cookie") != m_credentials_.end()) {
				std::wstringstream body_stream;
				body_stream << L"{ \"username\": \"" << m_credentials_["username"] << L"\", \"password\": \"" << m_credentials_["password"] << L"\" }", L"application/json";

				web::http::http_request token_request(web::http::methods::POST);
				token_request.request_uri(m_credentials_["auth"] + L"login");

				web::http::http_response rep;
				send(token_request)
                    .then([&rep](const auto& response) { rep = response; })
                    .wait();

				std::wstring test;
			    rep.extract_string()
                   .then([&test](auto b) { test = b; })
                   .wait();
			}
		}
	} catch (const std::exception&) {
		m_status_ = AuthenticationStatus::INVALID_CREDENTIALS;
	}

	// Assumes all operations completed succesfully
	if (m_status_ == UNAUTHENTICATED)
		m_status_ = AUTHENTICATED;
}

void Django::validate(Credentials& credentials) {
	std::vector<std::string> required_fields;
	if (m_authentication_ == TOKEN)
		required_fields = {"token", "validation"};
	else if (m_authentication_ == SESSION)
		required_fields = {"username", "password", "csrf", "auth"};

	for (const auto& field : required_fields)
		if (m_credentials_.find(field) == m_credentials_.end())
			throw std::runtime_error("Incomplete authentication information.");
}

} // namespace asap::network
