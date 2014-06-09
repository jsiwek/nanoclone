#include "client.hpp"
#include "frontend.hpp"
#include "backend.hpp"

#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <cinttypes>

using namespace std;
using namespace nnc;

static string get_addr(unsigned long p)
	{
	stringstream ss;
	ss << "tcp://127.0.0.1:" << p;
	return ss.str();
	}

static vector<string> get_addrs(unsigned long sp)
	{
	return {get_addr(sp), get_addr(sp + 1), get_addr(sp + 2)};
	}

void lookup_callback(const key_type& key, unique_ptr<value_type> val,
                     AsyncResultCode res)
	{
	if ( val )
		printf("lookup(%s): %d, %" PRIi64 "\n", key.c_str(), res, *val.get());
	else
		printf("lookup(%s): %d, null\n", key.c_str(), res);
	}

int run_client(unsigned long start_port, const string& name)
	{
	NonAuthoritativeFrontend frontend("example0");
	NonAuthoritativeBackend backend;
	vector<string> addrs = get_addrs(start_port);
	int64_t io_count = 0;
	int io_count_throttle = 10;
	string io_count_key = "io_count_" + name;

	if ( ! backend.Connect(addrs[0], addrs[1], addrs[2]) )
		{
		printf("Failed to connect on ports %lu - %lu\n", start_port,
		       start_port + 2);
		return 1;
		}

	frontend.Pair(&backend);
	frontend.Insert(io_count_key, io_count);

	for ( ; ; )
		{
		int nfds = 0;
		fd_set rfds;
		fd_set wfds;
		timeval timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		unique_ptr<timeval> to(new timeval(timeout));
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		bool res = backend.GetSelectParams(&nfds, &rfds, &wfds, nullptr, &to);

		if ( ! res )
			{
			printf("Failed to get select() params\n");
			return 1;
			}

		int num_ready = select(nfds, &rfds, &wfds, nullptr, to.get());

		if ( num_ready < 0 )
			{
			printf("Error in select()\n");
			return 1;
			}

		backend.ProcessIO();
		++io_count;

		if ( io_count % io_count_throttle == 0 )
			{
			frontend.Increment(io_count_key, io_count_throttle);
			frontend.LookupAsync("io_count_server", 5, lookup_callback);
			}

		frontend.DumpDebug(stdout);
		}

	return 0;
	}
