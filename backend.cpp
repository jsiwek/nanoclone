#include "backend.hpp"
#include "frontend.hpp"
#include "util.hpp"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

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

nnc::Backend::Backend()
	{
	}

nnc::AuthoritativeBackend::AuthoritativeBackend()
	: nnc::Backend(), listening(false), rep_socket(-1), pub_socket(-1),
	  pul_socket(-1), frontends()
	{
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

bool nnc::AuthoritativeBackend::Publish(const Publication* publication)
	{
	// TODO add to publish queue
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
	  psh_socket(-1), frontends()
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
	// TODO: flush request queues associated w/ this topic?  Or just check
	// when message is processed and drop if no frontend exists.
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

bool nnc::NonAuthoritativeBackend::SendRequest(const Request* request)
	{
	// TODO add to request queue
	return true;
	}

bool nnc::NonAuthoritativeBackend::SendUpdate(const Update* update)
	{
	// TODO add to update queue
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
