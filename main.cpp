#include <cstdio>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include "server.hpp"
#include "client.hpp"

using namespace std;

static void usage(const string& program)
	{
	fprintf(stderr, "%s --server|--client [options]\n", program.c_str());
	fprintf(stderr, "    -s|--server      | server/authoritative mode\n");
	fprintf(stderr, "    -c|--client      | client/non-authoritative mode\n");
	fprintf(stderr, "    -p|--port        | starting TCP port for 3 sockets\n");
	fprintf(stderr, "    -n|--name        | name for the instance\n");
	}

static option long_options[] = {
    {"server",       no_argument,          0, 's'},
    {"client",       no_argument,          0, 'c'},
    {"port",         required_argument,    0, 'p'},
    {"name",         required_argument,    0, 'n'},
};

static const char* opt_string = "p:n:sc";

int main(int argc, char** argv)
	{
	pid_t pid = getpid();
	bool is_server = false;
	string starting_port = "10000";
	stringstream ss;
	ss << pid;
	string instance_name = ss.str();

	for ( ; ; )
		{
		int o = getopt_long(argc, argv, opt_string, long_options, 0);

		if ( o == -1 )
			break;

		switch ( o ) {
		case 's':
			is_server = true;
			break;
		case 'c':
			is_server = false;
			break;
		case 'p':
			starting_port = optarg;
			break;
		case 'n':
			instance_name = optarg;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
		}

	if ( is_server )
		return run_server(stoul(starting_port), instance_name);
	else
		return run_client(stoul(starting_port), instance_name);
	}
