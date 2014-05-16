#ifndef NANOCLONE_FRONTEND_HPP
#define NANOCLONE_FRONTEND_HPP

#include "type_aliases.hpp"
#include "messages.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace nnc {

class Backend;
class AuthoritativeBackend;
class NonAuthoritativeBackend;

class Frontend {
public:

	Frontend(const std::string& topic);

	virtual ~Frontend() {}

	const std::string& Topic() const
		{ return topic; }

	// TODO: add a param for expiry time of this key.
	bool Insert(const key_type& key, const value_type& val)
		{ return DoInsert(key, val); }

	bool Remove(const key_type& key)
		{ return DoRemove(key); }

	bool Increment(const key_type& key, const value_type& by)
		{ return DoIncrement(key, by); }

	bool Decrement(const key_type& key, const value_type& by)
		{ return DoDecrement(key, by); }

	bool Clear()
		{ return DoClear(); }

	const value_type* LookupSync(const key_type& key) const;

	bool HasKeySync(const key_type& key) const;

	size_t SizeSync() const;

	bool LookupAsync(const key_type& key, double timeout, lookup_cb cb) const
		{ return DoLookupAsync(key, timeout, cb); }

	bool HasKeyAsync(const key_type& key, double timeout, haskey_cb cb) const
		{ return DoHasKeyAsync(key, timeout, cb); }

	bool SizeAsync(double timeout, size_cb cb) const
		{ return DoSizeAsync(timeout, cb); }

protected:

	std::string topic;
	kv_store_type store;
	uint64_t sequence; // TODO: what can be done about overflows?

private:

	virtual bool DoInsert(const key_type& key, const value_type& val) = 0;
	virtual bool DoRemove(const key_type& key) = 0;
	virtual bool DoIncrement(const key_type& key, const value_type& by) = 0;
	virtual bool DoDecrement(const key_type& key, const value_type& by) = 0;
	virtual bool DoClear() = 0;

	virtual bool DoLookupAsync(const key_type& key, double timeout,
	                           lookup_cb cb) const = 0;
	virtual bool DoHasKeyAsync(const key_type& key, double timeout,
	                           haskey_cb cb) const = 0;
	virtual bool DoSizeAsync(double timeout, size_cb cb) const = 0;
};


class AuthoritativeFrontend : public Frontend {
public:

	AuthoritativeFrontend(const std::string& topic);

	bool AddBackend(AuthoritativeBackend* backend);
	bool RemBackend(AuthoritativeBackend* backend);

private:

	virtual bool DoInsert(const key_type& key, const value_type& val) override;
	virtual bool DoRemove(const key_type& key) override;
	virtual bool DoIncrement(const key_type& key,const value_type& by) override;
	virtual bool DoDecrement(const key_type& key,const value_type& by) override;
	virtual bool DoClear() = 0;

	virtual bool DoLookupAsync(const key_type& key, double timeout,
	                           lookup_cb cb) const override;
	virtual bool DoHasKeyAsync(const key_type& key, double timeout,
	                           haskey_cb cb) const override;
	virtual bool DoSizeAsync(double timeout, size_cb cb) const override;

	bool Publish(const Publication* publications) const;

	std::unordered_set<AuthoritativeBackend*> backends;
};


class NonAuthoritativeFrontend : public Frontend {
public:

	NonAuthoritativeFrontend(const std::string& topic);

	bool Pair(NonAuthoritativeBackend* backend);
	bool Unpair();

private:

	virtual bool DoInsert(const key_type& key, const value_type& val) override;
	virtual bool DoRemove(const key_type& key) override;
	virtual bool DoIncrement(const key_type& key,const value_type& by) override;
	virtual bool DoDecrement(const key_type& key,const value_type& by) override;
	virtual bool DoClear() = 0;

	virtual bool DoLookupAsync(const key_type& key, double timeout,
	                           lookup_cb cb) const override;
	virtual bool DoHasKeyAsync(const key_type& key, double timeout,
	                           haskey_cb cb) const override;
	virtual bool DoSizeAsync(double timeout, size_cb cb) const override;

	NonAuthoritativeBackend* backend;
};

} // namespace nnc

#endif // NANOCLONE_FRONTEND_HPP
