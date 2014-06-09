#include "messages.hpp"

#include <sstream>
#include <cstdlib>
#include <utility>
#include <cmath>
#include <sys/time.h>

using namespace std;
using namespace nnc;

using kv_pair = pair<key_type, value_type>;

static inline double now()
	{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + (tv.tv_usec / 1000000.0);
	}

static const char* find_space(const char* msg, size_t size)
	{
	while ( size > 0 )
		{
		if ( msg[0] == ' ' )
			return msg;

		++msg;
		++size;
		}

	return nullptr;
	}

static inline void serialize_key(stringstream& ss, const key_type& key)
	{
	ss << key.size() << " " << key;
	}

static key_type unserialize_key(const char** msg, size_t* size)
	{
	const char* p = find_space(*msg, *size);

	if ( ! p )
		throw parse_error();

	size_t n = p - *msg;
	string size_str(*msg, n);
	*msg += n + 1;
	*size -= n + 1;
	size_t key_size;

	try
		{
		key_size = stoull(size_str);
		}
	catch ( invalid_argument& ) { throw parse_error(); }
	catch ( out_of_range& ) { throw parse_error(); }

	if ( *size < key_size )
		throw parse_error();

	key_type rval(*msg, key_size);
	*msg += key_size;
	*size -= key_size;
	return rval;
	}

static inline void serialize_val(stringstream& ss, const value_type& val)
	{
	ss << val;
	}

static uint64_t unserialize_uint64(const char** msg, size_t* size)
	{
	const char* p = find_space(*msg, *size);
	string val_str(*msg, p ? p - *msg : *size);
	uint64_t rval;

	try
		{
		rval = stoull(val_str);
		}
	catch ( invalid_argument& ) { throw parse_error(); }
	catch ( out_of_range& ) { throw parse_error(); }

	*msg += val_str.size();
	*size -= val_str.size();
	return rval;
	}

static value_type unserialize_val(const char** msg, size_t* size)
	{
	const char* p = find_space(*msg, *size);
	string val_str(*msg, p ? p - *msg : *size);
	value_type rval;

	try
		{
		rval = stoll(val_str);
		}
	catch ( invalid_argument& ) { throw parse_error(); }
	catch ( out_of_range& ) { throw parse_error(); }

	*msg += val_str.size();
	*size -= val_str.size();
	return rval;
	}

static inline void serialize_kv_pair(stringstream& ss, const key_type& key,
                                     const value_type& val)
	{
	serialize_key(ss, key);
	ss << " ";
	serialize_val(ss, val);
	}

static inline kv_pair unserialize_kv_pair(const char** msg, size_t* size)
	{
	key_type k = unserialize_key(msg, size);

	if ( *size > 0 && *msg[0] == ' ' )
		{
		*msg += 1;
		*size += 1;
		}
	else
		throw parse_error();

	value_type v = unserialize_val(msg, size);
	return kv_pair(k, v);
	}

nnc::Request::Request(const string& arg_topic, double arg_timeout)
	: sent(false), topic(arg_topic), creation_time(now()), timeout(arg_timeout)
	{
	}

timeval nnc::Request::UntilTimedOut() const
	{
	timeval rval;
	rval.tv_sec = rval.tv_usec = 0;
	double seconds_left = creation_time + timeout - now();

	if ( seconds_left < 0 )
		return rval;

	double intp, fractp;
	fractp = modf(seconds_left, &intp);
	rval.tv_sec = intp;
	rval.tv_usec = fractp * 1000000;
	return rval;
	}

bool nnc::Request::DoTimedOut() const
	{
	return now() > creation_time + timeout;
	}

unique_ptr<Request> nnc::Request::Parse(const char* msg, size_t size)
	{
	const char* p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	size_t n = p - msg;
	string topic(msg, n);
	msg += n + 1;
	size -= n + 1;

	p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	n = p - msg;
	string type(msg, n);
	msg += n + 1;
	size -= 1;

	if ( type == "SIZE" )
		return unique_ptr<Request>(new SizeRequest(topic, 0, nullptr));

	if ( type == "SNAPSHOT" )
		return unique_ptr<Request>(new SnapshotRequest(topic));

	key_type key;

	try
		{
		key = unserialize_key(&msg, &size);
		}
	catch ( parse_error& ) { return nullptr; }

	if ( type == "LOOKUP" )
		return unique_ptr<Request>(new LookupRequest(topic, key, 0, nullptr));

	if ( type == "HASKEY" )
		return unique_ptr<Request>(new HasKeyRequest(topic, key, 0, nullptr));

	return nullptr;
	}

void nnc::LookupRequest::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " LOOKUP ";
	serialize_key(ss, key);
	SetMsg(ss.str());
	}

bool nnc::LookupRequest::DoTimedOut() const
	{
	if ( Request::DoTimedOut() )
		{
		cb(key, nullptr, ASYNC_TIMEOUT);
		return true;
		}

	return false;
	}

unique_ptr<Response>
nnc::LookupRequest::DoProcess(const AuthoritativeFrontend* frontend) const
	{
	return unique_ptr<Response>(new LookupResponse(frontend->LookupSync(key)));
	}

bool nnc::LookupRequest::DoProcess(unique_ptr<Response> response,
                                   NonAuthoritativeFrontend* frontend) const
	{
	LookupResponse* r = dynamic_cast<LookupResponse*>(response.get());

	if ( ! r )
		{
		if ( dynamic_cast<InvalidRequestResponse*>(response.get()) )
			cb(key, nullptr, ASYNC_INVALID_REQUEST);
		else
			cb(key, nullptr, ASYNC_INVALID_RESPONSE);

		return false;
		}

	cb(key, move(r->Val()), ASYNC_SUCCESS);
	return true;
	}

void nnc::HasKeyRequest::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " HASKEY ";
	serialize_key(ss, key);
	SetMsg(ss.str());
	}

bool nnc::HasKeyRequest::DoTimedOut() const
	{
	if ( Request::DoTimedOut() )
		{
		cb(key, false, ASYNC_TIMEOUT);
		return true;
		}

	return false;
	}

unique_ptr<Response>
nnc::HasKeyRequest::DoProcess(const AuthoritativeFrontend* frontend) const
	{
	return unique_ptr<Response>(new HasKeyResponse(frontend->HasKeySync(key)));
	}

bool nnc::HasKeyRequest::DoProcess(std::unique_ptr<Response> response,
                                   NonAuthoritativeFrontend* frontend) const
	{
	HasKeyResponse* r = dynamic_cast<HasKeyResponse*>(response.get());

	if ( ! r )
		{
		if ( dynamic_cast<InvalidRequestResponse*>(response.get()) )
			cb(key, false, ASYNC_INVALID_REQUEST);
		else
			cb(key, false, ASYNC_INVALID_RESPONSE);

		return false;
		}

	cb(key, r->Exists(), ASYNC_SUCCESS);
	return true;
	}

void nnc::SizeRequest::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " SIZE ";
	SetMsg(ss.str());
	}

bool nnc::SizeRequest::DoTimedOut() const
	{
	if ( Request::DoTimedOut() )
		{
		cb(0, ASYNC_TIMEOUT);
		return true;
		}

	return false;
	}

unique_ptr<Response>
nnc::SizeRequest::DoProcess(const AuthoritativeFrontend* frontend) const
	{
	return unique_ptr<Response>(new SizeResponse(frontend->SizeSync()));
	}

bool nnc::SizeRequest::DoProcess(std::unique_ptr<Response> response,
                                 NonAuthoritativeFrontend* frontend) const
	{
	SizeResponse* r = dynamic_cast<SizeResponse*>(response.get());

	if ( ! r )
		{
		if ( dynamic_cast<InvalidRequestResponse*>(response.get()) )
			cb(0, ASYNC_INVALID_REQUEST);
		else
			cb(0, ASYNC_INVALID_RESPONSE);

		return false;
		}

	cb(r->Size(), ASYNC_SUCCESS);
	return true;
	}

void nnc::SnapshotRequest::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " SNAPSHOT ";
	SetMsg(ss.str());
	}

unique_ptr<Response>
nnc::SnapshotRequest::DoProcess(const AuthoritativeFrontend* frontend) const
	{
	return frontend->Snapshot();
	}

bool nnc::SnapshotRequest::DoProcess(std::unique_ptr<Response> response,
                                     NonAuthoritativeFrontend* frontend) const
	{
	return frontend->ApplySnapshot(move(response));
	}

unique_ptr<Response> nnc::Response::Parse(const char* msg, size_t size)
	{
	const char* p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	size_t n = p - msg;
	string type(msg, n);
	msg += n + 1;
	size -= n + 1;

	if ( type == "LOOKUP" )
		{
		if ( size == 0 )
			return unique_ptr<Response>(new LookupResponse(nullptr));

		value_type val;

		try
			{
			val = unserialize_val(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		return unique_ptr<Response>(new LookupResponse(&val));
		}

	if ( type == "HASKEY" )
		{
		if ( size > 0 )
			{
			if ( msg[0] == '0' )
				return unique_ptr<Response>(new HasKeyResponse(false));
			else
				return unique_ptr<Response>(new HasKeyResponse(true));
			}

		return nullptr;
		}

	if ( type == "SIZE" )
		{
		uint64_t s;

		try
			{
			s = unserialize_uint64(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		return unique_ptr<Response>(new SizeResponse(s));
		}

	if ( type == "SNAPSHOT" )
		{
		kv_store_type store;
		uint64_t seq;
		uint64_t store_size;

		try
			{
			seq = unserialize_uint64(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		if ( size == 0 || msg[0] != ' ' )
			return nullptr;

		++msg;
		--size;

		try
			{
			store_size = unserialize_uint64(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		for ( uint64_t i = 0; i < store_size; ++i )
			{
			if ( size == 0 || msg[0] != ' ' )
				return nullptr;

			msg += 1;
			size -= 1;

			kv_pair kvp;

			try
				{
				kvp = unserialize_kv_pair(&msg, &size);
				}
			catch ( parse_error& ) { return nullptr; }

			store[kvp.first] = kvp.second;
			}

		return unique_ptr<Response>(new SnapshotResponse(move(store), seq));
		}

	return nullptr;
	}

void nnc::LookupResponse::DoPrepare()
	{
	stringstream ss;
	ss << "LOOKUP ";

	if ( val )
		serialize_val(ss, *val);

	SetMsg(ss.str());
	}

void nnc::HasKeyResponse::DoPrepare()
	{
	SetMsg(exists ? "HASKEY 1" : "HASKEY 0");
	}

void nnc::SizeResponse::DoPrepare()
	{
	stringstream ss;
	ss << "SIZE " << size;
	SetMsg(ss.str());
	}

void nnc::SnapshotResponse::DoPrepare()
	{
	using ittype = kv_store_type::const_iterator;
	stringstream ss;
	ss << "SNAPSHOT " << sequence << " " << store.size();

	for ( ittype it = store.begin(); it != store.end(); ++it )
		{
		ss << " ";
		serialize_kv_pair(ss, it->first, it->second);
		}

	ss << " ";

	SetMsg(ss.str());
	}

void nnc::InvalidRequestResponse::DoPrepare()
	{
	stringstream ss;
	ss << "INVALID " << reason;
	SetMsg(ss.str());
	}

unique_ptr<Publication> nnc::Publication::Parse(const char* msg, size_t size)
	{
	const char* p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	size_t n = p - msg;
	string topic(msg, n);
	msg += n + 1;
	size -= n + 1;

	p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	n = p - msg;
	string type(msg, n);
	msg += n + 1;
	size -= n + 1;

	uint64_t seq;

	try
		{
		seq = unserialize_uint64(&msg, &size);
		}
	catch ( parse_error& ) { return nullptr; }

	if ( type == "UPDATE" )
		{
		if ( size == 0 || msg[0] != ' ' )
			return nullptr;

		++msg;
		--size;

		key_type key;

		try
			{
			key = unserialize_key(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		if ( size == 0 )
			return unique_ptr<Publication>(
			            new ValUpdatePublication(topic, key, nullptr, seq));

		if ( msg[0] != ' ' )
			return nullptr;

		++msg;
		--size;

		value_type val;

		try
			{
			val = unserialize_val(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		return unique_ptr<Publication>(
		            new ValUpdatePublication(topic, key, &val, seq));
		}

	if ( type == "CLEAR" )
		return unique_ptr<Publication>(new ClearPublication(topic, seq));

	return nullptr;
	}

void nnc::ValUpdatePublication::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " UPDATE " << Sequence() << " ";
	serialize_key(ss, key);

	if ( val )
		{
		ss << " ";
		serialize_val(ss, *val);
		}

	SetMsg(ss.str());
	}

void nnc::ClearPublication::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " CLEAR " << Sequence();
	SetMsg(ss.str());
	}

unique_ptr<Update> nnc::Update::Parse(const char* msg, size_t size)
	{
	const char* p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	size_t n = p - msg;
	string topic(msg, n);
	msg += n + 1;
	size -= n + 1;

	p = find_space(msg, size);

	if ( ! p )
		return nullptr;

	n = p - msg;
	string type(msg, n);
	msg += n + 1;
	size -= n + 1;

	if ( type == "CLEAR" )
		return unique_ptr<Update>(new ClearUpdate(topic));

	if ( type == "REMOVE" )
		{
		key_type key;

		try
			{
			key = unserialize_key(&msg, &size);
			}
		catch ( parse_error& ) { return nullptr; }

		return unique_ptr<Update>(new RemoveUpdate(topic, key));
		}

	kv_pair kv;

	try
		{
		kv = unserialize_kv_pair(&msg, &size);
		}
	catch ( parse_error& ) { return nullptr; }

	if ( type == "INSERT" )
		return unique_ptr<Update>(new InsertUpdate(topic, kv.first, kv.second));

	if ( type == "+=" )
		return unique_ptr<Update>(new IncrementUpdate(topic, kv.first,
		                                              kv.second));

	if ( type == "-=" )
		return unique_ptr<Update>(new DecrementUpdate(topic, kv.first,
		                                              kv.second));

	return nullptr;
	}

void nnc::InsertUpdate::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " INSERT ";
	serialize_kv_pair(ss, key, val);
	SetMsg(ss.str());
	}

void nnc::RemoveUpdate::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " REMOVE ";
	serialize_key(ss, key);
	SetMsg(ss.str());
	}

void nnc::IncrementUpdate::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " += ";
	serialize_kv_pair(ss, key, by);
	SetMsg(ss.str());
	}

void nnc::DecrementUpdate::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " -= ";
	serialize_kv_pair(ss, key, by);
	SetMsg(ss.str());
	}

void nnc::ClearUpdate::DoPrepare()
	{
	stringstream ss;
	ss << Topic() << " CLEAR";
	SetMsg(ss.str());
	}
