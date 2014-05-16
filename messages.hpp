#ifndef NANOCLONE_MESSAGES
#define NANOCLONE_MESSAGES

#include "type_aliases.hpp"

namespace nnc {

class Message {
public:

	virtual ~Message() {}

	const std::string& Msg() const
		{ return message; }

	void SetMsg(const std::string& arg_message)
		{ message = arg_message; }

private:

	std::string message;
};

// Sent on request socket of non-authoritative backend, and read from reply
// socket of authoritative backend.
class Request : public Message {
public:

	Request(const std::string& topic, double timeout);

	virtual ~Request() {}

	const std::string& Topic() const
		{ return topic; }

	bool TimedOut() const
		{ return DoTimedOut(); }

	double CreationTime() const
		{ return creation_time; }

private:

	virtual bool DoTimedOut() const;

	std::string topic;
	double creation_time;
	double timeout;
};

class LookupRequest : public Request {
public:

	LookupRequest(const std::string& topic, const key_type& key, double timeout,
	              lookup_cb cb);

private:

	key_type key;
	lookup_cb cb;
};

class HasKeyRequest : public Request {
public:

	HasKeyRequest(const std::string& topic, const key_type& key,
	              double timeout, haskey_cb cb);

private:

	key_type key;
	haskey_cb cb;
};

class SizeRequest : public Request {
public:

	SizeRequest(const std::string& topic, double timeout, size_cb cb);

private:

	size_cb cb;
};

class SnapshotRequest : public Request {
public:

	SnapshotRequest(const std::string& topic);

private:

	virtual bool DoTimedOut() const override
		{ return false; }

	std::string topic;
};

// Sent on reply socket of authoritative backend, and read from request socket
// of non-authoritative backend
class Response : public Message {
public:

	virtual ~Response() {}
};

class LookupResponse : public Response {
public:

	LookupResponse(const value_type* val);
};

class HasKeyResponse : public Response {
public:

	HasKeyResponse(bool exists);
};

class SizeResponse : public Response {
public:

	SizeResponse(size_t size);
};

class SnapshotResponse : public Response {

	SnapshotResponse(const kv_store_type& store, uint64_t sequence);
};

class InvalidResponse : public Response {
public:

	InvalidResponse(const std::string& reason);
};

// Published on pub socket of authoritative backend, and read from sub socket
// of non-authoritative backends.
class Publication : public Message {
public:

	virtual ~Publication() {}
};

// TODO: This could mirror the different types of updates, but for now it's
// easier to just send the full values after they're updated.
class ValUpdatePublication : public Publication {
public:

	ValUpdatePublication(const std::string& topic, const key_type& key,
	                     const value_type* val, uint64_t sequence);
};

class ClearPublication : public Publication {
public:

	ClearPublication(const std::string& topic, uint64_t sequence);
};

// Pushed on to pipeline socket by non-authoritative backend, pulled from
// an authoritative backend.
class Update : public Message {
public:

	virtual ~Update() {}
};

class InsertUpdate : public Update {
public:

	InsertUpdate(const std::string& topic, const key_type& key,
	             const value_type& val);
};

class RemoveUpdate : public Update {
public:

	RemoveUpdate(const std::string& topic, const key_type& key);
};

class IncrementUpdate : public Update {
public:

	IncrementUpdate(const std::string& topic, const key_type& key,
	                const value_type& by);
};

class DecrementUpdate : public Update {
public:

	DecrementUpdate(const std::string& topic, const key_type& key,
	                const value_type& by);
};

class ClearUpdate : public Update {
public:

	ClearUpdate(const std::string& topic);
};

} // namespace nnc

#endif // NANOCLONE_MESSAGES
