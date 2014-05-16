#include "messages.hpp"

#include <sstream>
#include <sys/time.h>

using namespace std;
using namespace nnc;

static inline double now()
	{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + (tv.tv_usec / 1000000.0);
	}

static inline void serialize_key(stringstream& ss, const key_type& key)
	{
	ss << htonll(key.size()) << " " << key;
	}

static inline void serialize_val(stringstream& ss, const value_type& val)
	{
	ss << htonll(val);
	}

static inline void serialize_kv_pair(stringstream& ss, const key_type& key,
                                     const value_type& val)
	{
	serialize_key(ss, key);
	ss << " ";
	serialize_val(ss, val);
	}

nnc::Request::Request(const string& arg_topic, double arg_timeout)
	: topic(arg_topic), creation_time(now()), timeout(arg_timeout)
	{
	}

bool nnc::Request::DoTimedOut() const
	{
	return now() > creation_time + timeout;
	}

nnc::LookupRequest::LookupRequest(const string& topic, const key_type& arg_key,
                                  double arg_timeout, lookup_cb arg_cb)
	: Request(topic, arg_timeout), key(arg_key), cb(arg_cb)
	{
	stringstream ss;
	ss << topic << " LOOKUP ";
	serialize_key(ss, key);
	SetMsg(ss.str());
	}

nnc::HasKeyRequest::HasKeyRequest(const string& topic, const key_type& arg_key,
                                  double arg_timeout, haskey_cb arg_cb)
	: Request(topic, arg_timeout), key(arg_key), cb(arg_cb)
	{
	stringstream ss;
	ss << topic << " HASKEY ";
	serialize_key(ss, key);
	SetMsg(ss.str());
	}

nnc::SizeRequest::SizeRequest(const string& topic, double arg_timeout,
                              size_cb arg_cb)
	: Request(topic, arg_timeout), cb(arg_cb)
	{
	stringstream ss;
	ss << topic << " SIZE";
	SetMsg(ss.str());
	}

nnc::SnapshotRequest::SnapshotRequest(const string& arg_topic)
	: Request(topic, 0), topic(arg_topic)
	{
	stringstream ss;
	ss << topic << " SNAPSHOT";
	SetMsg(ss.str());
	}

nnc::LookupResponse::LookupResponse(const value_type* val)
	{
	stringstream ss;

	if ( val )
		serialize_val(ss, *val);
	else
		ss << "null";

	SetMsg(ss.str());
	}

nnc::HasKeyResponse::HasKeyResponse(bool exists)
	{
	SetMsg(exists ? "1" : "0");
	}

nnc::SizeResponse::SizeResponse(size_t size)
	{
	stringstream ss;
	ss << htonll(size);
	SetMsg(ss.str());
	}

nnc::SnapshotResponse::SnapshotResponse(const kv_store_type& store,
                                        uint64_t sequence)
	{
	using ittype = kv_store_type::const_iterator;
	stringstream ss;
	ss << htonll(sequence) << " " << htonll(store.size());

	for ( ittype it = store.begin(); it != store.end(); ++it )
		{
		if ( it != store.begin() )
			ss << " ";

		serialize_kv_pair(ss, it->first, it->second);
		}

	SetMsg(ss.str());
	}

nnc::InvalidResponse::InvalidResponse(const string& reason)
	{
	stringstream ss;
	ss << "invalid " << reason;
	SetMsg(ss.str());
	}

nnc::ValUpdatePublication::ValUpdatePublication(const string& topic,
                                                const key_type& key,
                                                const value_type* val,
                                                uint64_t sequence)
	{
	stringstream ss;
	ss << topic << " UPDATE " << htonll(sequence) << " ";
	serialize_key(ss, key);

	if ( val )
		{
		ss << " ";
		serialize_val(ss, *val);
		}

	SetMsg(ss.str());
	}

nnc::ClearPublication::ClearPublication(const string& topic, uint64_t sequence)
	{
	stringstream ss;
	ss << topic << " CLEAR " << htonll(sequence);
	SetMsg(ss.str());
	}

nnc::InsertUpdate::InsertUpdate(const string& topic, const key_type& key,
                                const value_type& val)
	{
	stringstream ss;
	ss << topic << " INSERT ";
	serialize_kv_pair(ss, key, val);
	SetMsg(ss.str());
	}

nnc::RemoveUpdate::RemoveUpdate(const string& topic, const key_type& key)
	{
	stringstream ss;
	ss << topic << " REMOVE ";
	serialize_key(ss, key);
	SetMsg(ss.str());
	}

nnc::IncrementUpdate::IncrementUpdate(const string& topic, const key_type& key,
                                      const value_type& by)
	{
	stringstream ss;
	ss << topic << " += ";
	serialize_kv_pair(ss, key, by);
	SetMsg(ss.str());
	}

nnc::DecrementUpdate::DecrementUpdate(const string& topic, const key_type& key,
                                      const value_type& by)
	{
	stringstream ss;
	ss << topic << " -= ";
	serialize_kv_pair(ss, key, by);
	SetMsg(ss.str());
	}

nnc::ClearUpdate::ClearUpdate(const string& topic)
	{
	stringstream ss;
	ss << topic << " CLEAR";
	SetMsg(ss.str());
	}
