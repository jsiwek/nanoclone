#ifndef NANOCLONE_TYPE_ALIASES
#define NANOCLONE_TYPE_ALIASES

#include "config.h"

#include <unordered_map>
#include <string>
#include <functional>
#include <cstdint>

namespace nnc {

using key_type = std::string;
// TODO: uint64_t as values is arbitrary for now.
using value_type = uint64_t;
// TODO: A more robust key-value store implementation is desirable.
//       e.g. leverage an external library that provides at least a persistence
//       mechanism.
using kv_store_type = std::unordered_map<key_type, value_type>;

// Last param of callbacks are an error code.
// -1 == timeout, 0 == success, 1 == invalid request, 2 == invalid response.
using lookup_cb = std::function<void(const key_type&, const value_type*, int)>;
using haskey_cb = std::function<void(const key_type&, bool, int)>;
using size_cb = std::function<void(size_t, int)>;

#ifdef NNC_BIGENDIAN
inline uint64_t ntohll(uint64_t i) { return i };
inline uint64_t htonll(uint64_t i) { return i };
#else
inline uint64_t ntohll(uint64_t i)
	{
	unsigned char c;
	union {
		uint64_t i;
		unsigned char c[8];
	} x;

	x.i = i;
	c = x.c[0]; x.c[0] = x.c[7]; x.c[7] = c;
	c = x.c[1]; x.c[1] = x.c[6]; x.c[6] = c;
	c = x.c[2]; x.c[2] = x.c[5]; x.c[5] = c;
	c = x.c[3]; x.c[3] = x.c[4]; x.c[4] = c;
	return x.i;
	}

inline uint64_t htonll(uint64_t i) { return ntohll(i); }
#endif // NNC_BIGENDIAN

} // namespace nnc

#endif // NANOCLONE_TYPE_ALIASES
