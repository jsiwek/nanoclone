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

	Backend() = default;

	virtual ~Backend() {}

	bool ProcessIO()
		{ return DoProcessIO(); }

	bool HasPendingOutput() const
		{ return DoHasPendingOutput(); }

	virtual bool GetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                             fd_set* errorfds,
	                             std::unique_ptr<timeval>* timeout) const
		{ return DoGetSelectParams(nfds, readfds, writefds, errorfds, timeout);}

	// May block.
	bool Close()
		{ return DoClose(); }

private:

	virtual bool DoProcessIO() = 0;
	virtual bool DoHasPendingOutput() const = 0;
	virtual bool DoClose() = 0;
	virtual bool
	DoGetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                  fd_set* errorfds,
	                  std::unique_ptr<timeval>* timeout) const = 0;
};


class AuthoritativeBackend : public Backend {
public:

	AuthoritativeBackend() = default;

	// If there are unsent publications, that means any subscribers are
	// going to be out of sync and have to request a snapshot if an equivalent
	// backend ever comes back up.
	virtual ~AuthoritativeBackend() {}

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
	DoGetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                  fd_set* errorfds,
	                  std::unique_ptr<timeval>* timeout) const override;

	bool listening = false;
	int rep_socket = -1;
	int pub_socket = -1;
	int pul_socket = -1;
	std::unordered_map<std::string, AuthoritativeFrontend*> frontends;
	std::queue<std::shared_ptr<Publication>> publications;
	std::unique_ptr<Response> pending_response = nullptr;
};


class NonAuthoritativeBackend : public Backend {
public:

	NonAuthoritativeBackend() = default;

	virtual ~NonAuthoritativeBackend() {}

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
	DoGetSelectParams(int* nfds, fd_set* readfds, fd_set* writefds,
	                  fd_set* errorfds,
	                  std::unique_ptr<timeval>* timeout) const override;

	bool connected = false;
	int req_socket = -1;
	int sub_socket = -1;
	int psh_socket = -1;
	std::unordered_map<std::string, NonAuthoritativeFrontend*> frontends;
	std::list<std::unique_ptr<Request>> requests;
	std::queue<std::unique_ptr<Update>> updates;
};

} // namespace nnc

#endif // NANOCLONE_BACKEND_HPP
