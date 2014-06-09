#ifndef NANOCLONE_TYPE_ALIASES
#define NANOCLONE_TYPE_ALIASES

#include <unordered_map>
#include <string>
#include <functional>
#include <cstdint>
#include <memory>

namespace nnc {

using key_type = std::string;
// TODO: int64_t as values is arbitrary for now.
using value_type = int64_t;
// TODO: A more robust key-value store implementation is desirable.
//       e.g. leverage an external library that provides at least a persistence
//       mechanism.
using kv_store_type = std::unordered_map<key_type, value_type>;

enum AsyncResultCode {
	ASYNC_TIMEOUT = -1,
	ASYNC_SUCCESS = 0,
	ASYNC_INVALID_REQUEST = 1,
	ASYNC_INVALID_RESPONSE = 2,
};

using lookup_cb = std::function<void(const key_type&,
                                     std::unique_ptr<value_type>,
                                     AsyncResultCode)>;
using haskey_cb = std::function<void(const key_type&, bool, AsyncResultCode)>;
using size_cb = std::function<void(uint64_t, AsyncResultCode)>;

} // namespace nnc

#endif // NANOCLONE_TYPE_ALIASES
