#ifndef NANOCLONE_BACKEND_HPP
#define NANOCLONE_BACKEND_HPP

#include "messages.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace nnc {

class Frontend;
class AuthoritativeFrontend;
class NonAuthoritativeFrontend;

class Backend {
public:

	Backend();

	virtual ~Backend() {}

	// May block.
	bool Close()
		{ return DoClose(); }

private:

	virtual bool DoClose() = 0;
};


class AuthoritativeBackend : public Backend {
public:

	AuthoritativeBackend();

	bool Listen(const std::string& reply_addr, const std::string& pub_addr,
	            const std::string& pull_addr);

	bool Listening() const
		{ return listening; }

	bool AddFrontend(AuthoritativeFrontend* frontend);

	bool RemFrontend(AuthoritativeFrontend* frontend);

	bool Publish(const Publication* publication);

private:

	virtual bool DoClose() override;

	bool listening;
	int rep_socket;
	int pub_socket;
	int pul_socket;
	std::unordered_map<std::string, AuthoritativeFrontend*> frontends;
};


class NonAuthoritativeBackend : public Backend {
public:

	NonAuthoritativeBackend();

	bool Connect(const std::string& request_addr, const std::string& sub_addr,
	             const std::string& push_addr);

	bool Connected() const
		{ return connected; }

	bool AddFrontend(NonAuthoritativeFrontend* frontend);

	bool RemFrontend(NonAuthoritativeFrontend* frontend);

	bool SendRequest(const Request* request);

	bool SendUpdate(const Update* update);

private:

	virtual bool DoClose() override;

	bool connected;
	int req_socket;
	int sub_socket;
	int psh_socket;
	std::unordered_map<std::string, NonAuthoritativeFrontend*> frontends;
};

} // namespace nnc

#endif // NANOCLONE_BACKEND_HPP
