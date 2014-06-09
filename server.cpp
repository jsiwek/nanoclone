#include "server.hpp"
#include "frontend.hpp"
#include "backend.hpp"

#include <sstream>
#include <vector>
#include <string>
#include <cstdio>

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

int run_server(unsigned long start_port, const string& name)
	{
	AuthoritativeFrontend frontend("example0");
	AuthoritativeBackend backend;
	vector<string> addrs = get_addrs(start_port);
	frontend.AddBackend(&backend);
	int64_t io_count = 0;
	int io_count_throttle = 10;
	string io_count_key = "io_count_" + name;
	frontend.Insert(io_count_key, io_count);

	if ( ! backend.Listen(addrs[0], addrs[1], addrs[2]) )
		{
		printf("Failed to listen on ports %lu - %lu\n", start_port,
		       start_port + 2);
		return 1;
		}

	for ( ; ; )
		{
		int nfds = 0;
		fd_set rfds;
		fd_set wfds;
		unique_ptr<timeval> to(nullptr);
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
			frontend.Increment(io_count_key, io_count_throttle);

		frontend.DumpDebug(stdout);
		}

	return 0;
	}
