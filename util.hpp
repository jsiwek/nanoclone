#ifndef NANOCLONE_UTIL_H
#define NANOCLONE_UTIL_H

#include <functional>
#include <string>
#include <vector>

namespace nnc {

bool safe_nn_close(int socket);

std::vector<bool> safe_nn_close(const std::vector<int>& sockets);

std::vector<int> nn_sockets(const std::vector<int>& protocols);

std::vector<int> add_endpoints(const std::vector<int>& sockets,
                               const std::vector<std::string>& addrs,
                               const std::function<int(int, const char*)>& f);

} // namespace nnc

#endif
