#include "util.hpp"

#include <stdexcept>
#include <nanomsg/nn.h>

using namespace std;
using namespace nnc;

bool nnc::safe_nn_close(int socket)
	{
	int rc;

	while ( (rc = nn_close(socket)) == -1 && errno == EINTR );

	if ( rc == 0 )
		return true;

	// EBADF
	return false;
	}

vector<bool> nnc::safe_nn_close(const vector<int>& sockets)
	{
	vector<bool> rval;
	auto saved_errno = errno;

	for ( auto s : sockets )
		rval.push_back(safe_nn_close(s));

	errno = saved_errno;
	return rval;
	}

vector<int> nnc::nn_sockets(const vector<int>& protocols)
	{
	vector<int> rval;

	for ( auto p : protocols )
		{
		int s = nn_socket(AF_SP, p);

		if ( s == -1 )
			return rval;

		rval.push_back(s);
		}

	return rval;
	}

vector<int> nnc::add_endpoints(const vector<int>& sockets,
                               const vector<string>& addrs,
                               const function<int(int, const char*)>& f)
	{
	vector<int> rval;

	if ( sockets.size() != addrs.size() )
		throw invalid_argument("sockets.size() != addrs.size()");

	for ( auto i = 0; i < sockets.size(); ++i )
		{
		const string& a = addrs[i];
		const char* addr = a.c_str();
		int eid = f(sockets[i], addr);

		if ( eid == -1 )
			return rval;

		rval.push_back(eid);
		}

	return rval;
	}
