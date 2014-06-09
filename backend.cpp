#include "backend.hpp"
#include "frontend.hpp"
#include "util.hpp"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

#include <cstdio>
#include <cstdlib>
#include <algorithm>

using namespace std;
using namespace nnc;

static bool setup_sockets(const vector<int>& protocols,
                          const vector<string>& addrs,
                          const vector<int*>& socket_pointers,
                          const function<int(int, const char*)> how)
	{
	auto sockets = nn_sockets(protocols);

	if ( sockets.size() != protocols.size() )
		{
		safe_nn_close(sockets);
		return false;
		}

	auto endpoints = add_endpoints(sockets, addrs, how);

	if ( endpoints.size() != sockets.size() )
		{
		safe_nn_close(sockets);
		return false;
		}

	for ( auto i = 0; i < sockets.size(); ++i )
		*socket_pointers[i] = sockets[i];

	return true;
	}

static void handle_nn_error(const string& msg)
	{
	// TODO: not quite sure the right way to handle errors.  None seem
	// seem suitable to be thrown as exception, the ones not ignored here
	// indicate broken code in nanoclone.

	int e = nn_errno();

	if ( e == EAGAIN || e == EINTR )
		return;

	fprintf(stderr, msg.c_str(), nn_strerror(e));
	exit(1);
	}

static bool set_nn_fds(int socket, int option, fd_set* fds, int* maxfd)
	{
	int fd;
	size_t sz = sizeof(fd);
	int res = nn_getsockopt(socket, option, NN_SOL_SOCKET, &fd, &sz);

	if ( res != 0 )
		return false;

	FD_SET(fd, fds);
	*maxfd = max(*maxfd, fd);
	return true;
	}

nnc::Backend::Backend()
	{
	}

nnc::AuthoritativeBackend::AuthoritativeBackend()
	: nnc::Backend(), listening(false), rep_socket(-1), pub_socket(-1),
	  pul_socket(-1), frontends(), publications(), pending_response(nullptr)
	{
	}

nnc::AuthoritativeBackend::~AuthoritativeBackend()
	{
	// If there are unsent publications, that means any subscribers are
	// going to be out of sync and have to request a snapshot if an equivalent
	// backend ever comes back up.
	}

bool nnc::AuthoritativeBackend::AddFrontend(AuthoritativeFrontend* frontend)
	{
	using vt = decltype(frontends)::value_type;
	return frontends.insert(vt(frontend->Topic(), frontend)).second;
	}

bool nnc::AuthoritativeBackend::RemFrontend(AuthoritativeFrontend* frontend)
	{
	return frontends.erase(frontend->Topic()) == 1;
	}

bool nnc::AuthoritativeBackend::Listen(const string& reply_addr,
                                       const string& pub_addr,
                                       const string& pull_addr)
	{
	if ( listening )
		return false;

	if ( ! setup_sockets({NN_REP, NN_PUB, NN_PULL},
	                     {reply_addr, pub_addr, pull_addr},
	                     {&rep_socket, &pub_socket, &pul_socket},
	                     nn_bind) )
		return false;

	listening = true;
	return true;
	}

bool nnc::AuthoritativeBackend::Publish(shared_ptr<Publication> publication)
	{
	publications.push(publication);
	return true;
	}

bool nnc::AuthoritativeBackend::DoProcessIO()
	{
	// Try to read an update and process it.
	char* buf = nullptr;
	int n = nn_recv(pul_socket, &buf, NN_MSG, NN_DONTWAIT);

	if ( n < 0 )
		handle_nn_error("Failed to pull and update: %s\n");
	else
		{
		auto update = Update::Parse(buf, n);
		decltype(frontends)::const_iterator it;
		it = update ? frontends.find(update->Topic()) : frontends.end();

		if ( it != frontends.end() )
			update->Process(it->second);

		nn_freemsg(buf);
		}

	// Try to handle requests.
	if ( pending_response )
		{
		const string& msg = pending_response->Msg();
		int n = nn_send(rep_socket, msg.data(), msg.size(), NN_DONTWAIT);

		if ( n < 0 )
			handle_nn_error("Failed sending response: %s\n");
		else
			pending_response = nullptr;
		}

	if ( ! pending_response )
		{
		char* buf = nullptr;
		int n = nn_recv(rep_socket, &buf, NN_MSG, NN_DONTWAIT);

		if ( n < 0 )
			handle_nn_error("Failed to receive request: %s\n");
		else
			{
			auto request = Request::Parse(buf, n);

			if ( ! request )
				pending_response =
				        unique_ptr<Response>(new InvalidRequestResponse());
			else
				{
				decltype(frontends)::const_iterator it;
				it = frontends.find(request->Topic());

				if ( it != frontends.end() )
					pending_response = request->Process(it->second);
				}

			nn_freemsg(buf);
			}
		}

	// Try to write all publications.
	while ( ! publications.empty() )
		{
		const string& msg = publications.front()->Msg();
		int n = nn_send(pub_socket, msg.data(), msg.size(), NN_DONTWAIT);

		if ( n < 0 )
			{
			handle_nn_error("Failed to send publication: %s\n");
			break;
			}

		publications.pop();
		}

	return HasPendingOutput();
	}

bool nnc::AuthoritativeBackend::DoHasPendingOutput() const
	{
	return ! publications.empty() || pending_response;
	}

bool nnc::AuthoritativeBackend::DoSetSelectParams(int* nfds, fd_set* readfds,
                                                  fd_set* writefds,
                                                  fd_set* errorfds,
                                                  timeval* timeout) const
	{
	if ( ! listening )
		return false;

	int maxfd = *nfds - 1;

	if ( readfds )
		{
		if ( ! set_nn_fds(rep_socket, NN_RCVFD, readfds, &maxfd) )
			return false;

		if ( ! set_nn_fds(pul_socket, NN_RCVFD, readfds, &maxfd) )
			return false;
		}

	if ( writefds )
		{
		if ( ! publications.empty() )
			{
			if ( ! set_nn_fds(pub_socket, NN_SNDFD, writefds, &maxfd) )
				return false;
			}

		if ( pending_response )
			{
			if ( ! set_nn_fds(rep_socket, NN_SNDFD, writefds, &maxfd) )
				return false;
			}
		}

	if ( maxfd >= 0 )
		*nfds = maxfd + 1;

	return true;
	}

bool nnc::AuthoritativeBackend::DoClose()
	{
	if ( ! listening )
		return true;

	safe_nn_close({rep_socket, pub_socket, pul_socket});
	rep_socket = pub_socket = pul_socket = -1;
	listening = false;
	return true;
	}

nnc::NonAuthoritativeBackend::NonAuthoritativeBackend()
	: nnc::Backend(), connected(), req_socket(-1), sub_socket(-1),
	  psh_socket(-1), frontends(), requests(), updates()
	{
	}

nnc::NonAuthoritativeBackend::~NonAuthoritativeBackend()
	{
	}

bool nnc::NonAuthoritativeBackend::AddFrontend(NonAuthoritativeFrontend* fe)
	{
	using vt = decltype(frontends)::value_type;
	string t = fe->Topic();
	nn_setsockopt(sub_socket, NN_SUB, NN_SUB_SUBSCRIBE, t.c_str(), t.size());
	SendRequest(new SnapshotRequest(t));
	return frontends.insert(vt(t, fe)).second;
	}

bool nnc::NonAuthoritativeBackend::RemFrontend(NonAuthoritativeFrontend* fe)
	{
	string t = fe->Topic();
	nn_setsockopt(sub_socket, NN_SUB, NN_SUB_UNSUBSCRIBE, t.c_str(), t.size());
	return frontends.erase(t) == 1;
	}

bool nnc::NonAuthoritativeBackend::Connect(const string& request_addr,
                                           const string& sub_addr,
                                           const string& push_addr)
	{
	if ( connected )
		return false;

	if ( ! setup_sockets({NN_REQ, NN_SUB, NN_PUSH},
	                     {request_addr, sub_addr, push_addr},
	                     {&req_socket, &sub_socket, &psh_socket},
	                     nn_connect) )
		return false;

	connected = true;
	return true;
	}

bool nnc::NonAuthoritativeBackend::SendRequest(Request* request)
	{
	requests.push_back(unique_ptr<Request>(request));
	return true;
	}

bool nnc::NonAuthoritativeBackend::SendUpdate(Update* update)
	{
	updates.push(unique_ptr<Update>(update));
	return true;
	}

bool nnc::NonAuthoritativeBackend::DoProcessIO()
	{
	// Try to send all updates.
	while ( ! updates.empty() )
		{
		const string& msg = updates.front()->Msg();
		int n = nn_send(psh_socket, msg.data(), msg.size(), NN_DONTWAIT);

		if ( n < 0 )
			{
			handle_nn_error("Failed sending updates: %s\n");
			break;
			}

		updates.pop();
		}

	// Drop any requests that have timed out.
	for ( auto it = requests.begin(); it != requests.end(); )
		{
		if ( (*it)->TimedOut() )
			it = requests.erase(it);
		else
			++it;
		}

	if ( ! requests.empty() )
		{
		if ( requests.front()->Sent() )
			{
			// Try to read a response.
			char* buf = nullptr;
			int n = nn_recv(req_socket, &buf, NN_MSG, NN_DONTWAIT);

			if ( n < 0 )
				handle_nn_error("Failed to receive response: %s\n");
			else
				{
				auto response = Response::Parse(buf, n);

				if ( response )
					{
					NonAuthoritativeFrontend* frontend = nullptr;
					decltype(frontends)::const_iterator it;
					it = frontends.find(requests.front()->Topic());

					if ( it != frontends.end() )
						frontend = it->second;

					requests.front()->Process(move(response), frontend);
					}

				requests.pop_front();
				nn_freemsg(buf);
				}
			}
		else
			{
			// Try to send a request.
			const string& msg = requests.front()->Msg();
			int n = nn_send(req_socket, msg.data(), msg.size(), NN_DONTWAIT);

			if ( n < 0 )
				handle_nn_error("Failed sending request: %s\n");
			else
				requests.front()->MarkAsSent();
			}
		}

	// Try to read a publication.
	char* buf = nullptr;
	int n = nn_recv(sub_socket, &buf, NN_MSG, NN_DONTWAIT);

	if ( n < 0 )
		handle_nn_error("Failed to receive subscription: %s\n");
	else
		{
		auto pub = Publication::Parse(buf, n);
		decltype(frontends)::const_iterator it;
		it = pub ? frontends.find(pub->Topic()) : frontends.end();

		if ( it != frontends.end() )
			it->second->ProcessPublication(move(pub));

		nn_freemsg(buf);
		}

	return HasPendingOutput();
	}

bool nnc::NonAuthoritativeBackend::DoHasPendingOutput() const
	{
	if ( ! updates.empty() )
		return true;

	if ( requests.empty() )
		return false;

	if ( requests.front()->Sent() )
		return false;

	return true;
	}

bool nnc::NonAuthoritativeBackend::DoSetSelectParams(int* nfds, fd_set* readfds,
                                                     fd_set* writefds,
                                                     fd_set* errorfds,
                                                     timeval* timeout) const
	{
	if ( ! connected )
		return false;

	int maxfd = *nfds - 1;

	if ( readfds )
		{
		if ( ! set_nn_fds(req_socket, NN_RCVFD, readfds, &maxfd) )
			return false;

		if ( ! set_nn_fds(sub_socket, NN_RCVFD, readfds, &maxfd) )
			return false;
		}

	if ( writefds )
		{
		if ( ! updates.empty() )
			{
			if ( ! set_nn_fds(psh_socket, NN_SNDFD, writefds, &maxfd) )
				return false;
			}

		if ( ! requests.empty() && ! requests.front()->Sent() )
			{
			if ( ! set_nn_fds(req_socket, NN_SNDFD, writefds, &maxfd) )
				return false;
			}
		}

	if ( maxfd >= 0 )
		*nfds = maxfd + 1;

	return true;
	}

bool nnc::NonAuthoritativeBackend::DoClose()
	{
	if ( ! connected )
		return true;

	safe_nn_close({req_socket, sub_socket, psh_socket});
	req_socket = sub_socket = psh_socket = -1;
	connected = false;
	return true;
	}
