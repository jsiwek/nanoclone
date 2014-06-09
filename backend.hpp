#ifndef NANOCLONE_BACKEND_HPP
#define NANOCLONE_BACKEND_HPP

#include "messages.hpp"

#include <sys/select.h>
#include <memory>
#include <string>
#include <queue>
#include <list>
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

	bool ProcessIO()
		{ return DoProcessIO(); }

	bool HasPendingOutput() const
		{ return DoHasPendingOutput(); }

	virtual bool SetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                             fd_set* errorfds, timeval* timeout) const
		{ return DoSetSelectParams(nfds, readfds, writefds, errorfds, timeout);}

	// May block.
	bool Close()
		{ return DoClose(); }

private:

	virtual bool DoProcessIO() = 0;
	virtual bool DoHasPendingOutput() const = 0;
	virtual bool DoClose() = 0;
	virtual bool
	DoSetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                  fd_set* errorfds, timeval* timeout) const = 0;
};


class AuthoritativeBackend : public Backend {
public:

	AuthoritativeBackend();

	virtual ~AuthoritativeBackend();

	bool Listen(const std::string& reply_addr, const std::string& pub_addr,
	            const std::string& pull_addr);

	bool Listening() const
		{ return listening; }

	bool AddFrontend(AuthoritativeFrontend* frontend);

	bool RemFrontend(AuthoritativeFrontend* frontend);

	bool Publish(std::shared_ptr<Publication> publication);

private:

	virtual bool DoProcessIO() override;
	virtual bool DoHasPendingOutput() const override;
	virtual bool DoClose() override;
	virtual bool
	DoSetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                  fd_set* errorfds, timeval* timeout) const override;

	bool listening;
	int rep_socket;
	int pub_socket;
	int pul_socket;
	std::unordered_map<std::string, AuthoritativeFrontend*> frontends;
	std::queue<std::shared_ptr<Publication>> publications;
	std::unique_ptr<Response> pending_response;
};


class NonAuthoritativeBackend : public Backend {
public:

	NonAuthoritativeBackend();

	virtual ~NonAuthoritativeBackend();

	bool Connect(const std::string& request_addr, const std::string& sub_addr,
	             const std::string& push_addr);

	bool Connected() const
		{ return connected; }

	bool AddFrontend(NonAuthoritativeFrontend* frontend);

	bool RemFrontend(NonAuthoritativeFrontend* frontend);

	bool SendRequest(Request* request);

	bool SendUpdate(Update* update);

private:

	virtual bool DoProcessIO() override;
	virtual bool DoHasPendingOutput() const override;
	virtual bool DoClose() override;
	virtual bool
	DoSetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                  fd_set* errorfds, timeval* timeout) const override;

	bool connected;
	int req_socket;
	int sub_socket;
	int psh_socket;
	std::unordered_map<std::string, NonAuthoritativeFrontend*> frontends;
	std::list<std::unique_ptr<Request>> requests;
	std::queue<std::unique_ptr<Update>> updates;
};

} // namespace nnc

#endif // NANOCLONE_BACKEND_HPP
