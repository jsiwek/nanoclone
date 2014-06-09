#ifndef NANOCLONE_MESSAGES
#define NANOCLONE_MESSAGES

#include "type_aliases.hpp"
#include "frontend.hpp"

#include <string>
#include <exception>
#include <memory>
#include <sys/time.h>

namespace nnc {

class Response;

class parse_error : public std::exception {
};

class Message {
public:

	virtual ~Message() {}

	const std::string& Msg()
		{ if ( message.empty() ) Prepare(); return message; }

	void SetMsg(const std::string& arg_message)
		{ message = arg_message; }

	void Prepare()
		{ DoPrepare(); }

private:

	virtual void DoPrepare() = 0;

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

	double Timeout() const
		{ return timeout; }

	timeval UntilTimedOut() const;

	void MarkAsSent()
		{ sent = true; }

	bool Sent() const
		{ return sent; }

	std::unique_ptr<Response>
	Process(const AuthoritativeFrontend* frontend) const
		{ return DoProcess(frontend); }

	bool Process(std::unique_ptr<Response> response,
	             NonAuthoritativeFrontend* frontend) const
		{ return DoProcess(move(response), frontend); }

	static std::unique_ptr<Request> Parse(const char* msg, size_t size);

protected:

	virtual bool DoTimedOut() const;

private:

	virtual std::unique_ptr<Response>
	        DoProcess(const AuthoritativeFrontend* frontend) const = 0;

	virtual bool DoProcess(std::unique_ptr<Response> response,
	                       NonAuthoritativeFrontend* frontend) const = 0;

	bool sent;
	std::string topic;
	double creation_time;
	double timeout;
};

class LookupRequest : public Request {
public:

	LookupRequest(const std::string& topic, const key_type& arg_key,
	              double timeout, lookup_cb arg_cb)
		: Request(topic, timeout), key(arg_key), cb(arg_cb) {}

private:

	virtual void DoPrepare() override;
	virtual bool DoTimedOut() const override;
	virtual std::unique_ptr<Response>
	        DoProcess(const AuthoritativeFrontend* frontend) const override;
	virtual bool DoProcess(std::unique_ptr<Response> response,
	                       NonAuthoritativeFrontend* frontend) const override;

	key_type key;
	lookup_cb cb;
};

class HasKeyRequest : public Request {
public:

	HasKeyRequest(const std::string& topic, const key_type& arg_key,
	              double timeout, haskey_cb arg_cb)
		: Request(topic, timeout), key(arg_key), cb(arg_cb) {}

private:

	virtual void DoPrepare() override;
	virtual bool DoTimedOut() const override;
	virtual std::unique_ptr<Response>
	        DoProcess(const AuthoritativeFrontend* frontend) const override;
	virtual bool DoProcess(std::unique_ptr<Response> response,
	                       NonAuthoritativeFrontend* frontend) const override;

	key_type key;
	haskey_cb cb;
};

class SizeRequest : public Request {
public:

	SizeRequest(const std::string& topic, double timeout, size_cb arg_cb)
		: Request(topic, timeout), cb(arg_cb) {}

private:

	virtual void DoPrepare() override;
	virtual bool DoTimedOut() const override;
	virtual std::unique_ptr<Response>
	        DoProcess(const AuthoritativeFrontend* frontend) const override;
	virtual bool DoProcess(std::unique_ptr<Response> response,
	                       NonAuthoritativeFrontend* frontend) const override;

	size_cb cb;
};

class SnapshotRequest : public Request {
public:

	SnapshotRequest(const std::string& topic)
		: Request(topic, 0) {}

private:

	virtual void DoPrepare() override;
	virtual bool DoTimedOut() const override
		{ return false; }

	virtual std::unique_ptr<Response>
	        DoProcess(const AuthoritativeFrontend* frontend) const override;
	virtual bool DoProcess(std::unique_ptr<Response> response,
	                       NonAuthoritativeFrontend* frontend) const override;

	std::string topic;
};

// Sent on reply socket of authoritative backend, and read from request socket
// of non-authoritative backend
class Response : public Message {
public:

	virtual ~Response() {}

	static std::unique_ptr<Response> Parse(const char* msg, size_t size);
};

class LookupResponse : public Response {
public:

	LookupResponse(const value_type* arg_val)
		: val(arg_val ? std::unique_ptr<value_type>(new value_type(*arg_val))
	                  : nullptr) {}

	std::unique_ptr<value_type> Val()
		{ return std::move(val); }

private:

	virtual void DoPrepare() override;

	std::unique_ptr<value_type> val;
};

class HasKeyResponse : public Response {
public:

	HasKeyResponse(bool arg_exists)
		: exists(arg_exists) {}

	bool Exists() const
		{ return exists; }

private:

	virtual void DoPrepare() override;

	bool exists;
};

class SizeResponse : public Response {
public:

	SizeResponse(uint64_t arg_size)
		: size(arg_size) {}

	uint64_t Size() const
		{ return size; }

private:

	virtual void DoPrepare() override;

	uint64_t size;
};

class SnapshotResponse : public Response {
public:

	SnapshotResponse(const kv_store_type& arg_store, uint64_t arg_sequence)
		: store(arg_store), sequence(arg_sequence) {}

	SnapshotResponse(kv_store_type&& arg_store, uint64_t arg_sequence)
		: store(arg_store), sequence(arg_sequence) {}

	kv_store_type&& Store()
		{ return std::move(store); }

	uint64_t Sequence() const
		{ return sequence; }

private:

	virtual void DoPrepare() override;

	kv_store_type store;
	uint64_t sequence;
};

class InvalidRequestResponse : public Response {
public:

	InvalidRequestResponse(const std::string& arg_reason = "malformed")
		: reason(arg_reason) {}

private:

	virtual void DoPrepare() override;

	std::string reason;
};

// Published on pub socket of authoritative backend, and read from sub socket
// of non-authoritative backends.
class Publication : public Message {
public:

	Publication(const std::string& arg_topic, uint64_t arg_sequence)
		: topic(arg_topic), sequence(arg_sequence) {}

	virtual ~Publication() {}

	const std::string& Topic() const
		{ return topic; }

	uint64_t Sequence() const
		{ return sequence; }

	virtual bool Apply(kv_store_type& store) const
		{ return DoApply(store); }

	static std::unique_ptr<Publication> Parse(const char* msg, size_t size);

private:

	virtual bool DoApply(kv_store_type& store) const = 0;

	std::string topic;
	uint64_t sequence;
};

// TODO: This could mirror the different types of updates, but for now it's
// easier to just send the full values after they're updated.
class ValUpdatePublication : public Publication {
public:

	ValUpdatePublication(const std::string& topic, const key_type& arg_key,
	                     const value_type* arg_val, uint64_t sequence)
		: Publication(topic, sequence), key(arg_key),
	      val(arg_val ? std::unique_ptr<value_type>(new value_type(*arg_val))
	                  : nullptr) {}

private:

	virtual void DoPrepare() override;
	virtual bool DoApply(kv_store_type& store) const override
		{ if ( val ) store[key] = *val.get(); else store.erase(key);
		  return true; }

	key_type key;
	std::unique_ptr<value_type> val;
};

class ClearPublication : public Publication {
public:

	ClearPublication(const std::string& topic, uint64_t sequence)
		: Publication(topic, sequence) {}

	virtual bool DoApply(kv_store_type& store) const override
		{ store.clear(); return true; }

private:

	virtual void DoPrepare() override;
};

// Pushed on to pipeline socket by non-authoritative backend, pulled from
// an authoritative backend.
class Update : public Message {
public:

	Update(const std::string& arg_topic)
		: topic(arg_topic) {}

	virtual ~Update() {}

	virtual bool Process(AuthoritativeFrontend* frontend) const
		{ return DoProcess(frontend); }

	const std::string& Topic() const
		{ return topic; }

	static std::unique_ptr<Update> Parse(const char* msg, size_t size);

private:

	virtual bool DoProcess(AuthoritativeFrontend* frontend) const = 0;

	std::string topic;
};

class InsertUpdate : public Update {
public:

	InsertUpdate(const std::string& topic, const key_type& arg_key,
	             const value_type& arg_val)
	    : Update(topic), key(arg_key), val(arg_val) {}

private:

	virtual void DoPrepare() override;

	virtual bool DoProcess(AuthoritativeFrontend* frontend) const override
		{ return frontend->Insert(key, val); }

	key_type key;
	value_type val;
};

class RemoveUpdate : public Update {
public:

	RemoveUpdate(const std::string& topic, const key_type& arg_key)
	    : Update(topic), key(arg_key) {}

private:

	virtual void DoPrepare() override;

	virtual bool DoProcess(AuthoritativeFrontend* frontend) const override
		{ return frontend->Remove(key); }

	key_type key;
};

class IncrementUpdate : public Update {
public:

	IncrementUpdate(const std::string& topic, const key_type& arg_key,
	                const value_type& arg_by)
	    : Update(topic), key(arg_key), by(arg_by) {}

private:

	virtual void DoPrepare() override;

	virtual bool DoProcess(AuthoritativeFrontend* frontend) const override
		{ return frontend->Increment(key, by); }

	key_type key;
	value_type by;
};

class DecrementUpdate : public Update {
public:

	DecrementUpdate(const std::string& topic, const key_type& arg_key,
	                const value_type& arg_by)
		: Update(topic), key(arg_key), by(arg_by) {}

private:

	virtual void DoPrepare() override;

	virtual bool DoProcess(AuthoritativeFrontend* frontend) const override
		{ return frontend->Decrement(key, by); }

	key_type key;
	value_type by;
};

class ClearUpdate : public Update {
public:

	ClearUpdate(const std::string& topic)
		: Update(topic) {}

private:

	virtual void DoPrepare() override;

	virtual bool DoProcess(AuthoritativeFrontend* frontend) const override
		{ return frontend->Clear(); }
};

} // namespace nnc

#endif // NANOCLONE_MESSAGES
