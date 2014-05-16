#include "frontend.hpp"
#include "backend.hpp"
#include "util.hpp"

#include <assert.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

using namespace std;
using namespace nnc;

nnc::Frontend::Frontend(const string& arg_topic)
	: topic(arg_topic), store(), sequence(0)
	{
	}

const nnc::value_type* nnc::Frontend::LookupSync(const nnc::key_type& key) const
	{
	auto it = store.find(key);

	if ( it == store.end() )
		return nullptr;

	return &it->second;
	}

bool nnc::Frontend::HasKeySync(const key_type& key) const
	{
	return store.find(key) != store.end();
	}

size_t nnc::Frontend::SizeSync() const
	{
	return store.size();
	}

nnc::AuthoritativeFrontend::AuthoritativeFrontend(const string& topic)
	: nnc::Frontend(topic), backends()
	{
	}

bool nnc::AuthoritativeFrontend::AddBackend(AuthoritativeBackend* backend)
	{
	backend->AddFrontend(this);
	return backends.insert(backend).second;
	}

bool nnc::AuthoritativeFrontend::RemBackend(AuthoritativeBackend* backend)
	{
	backend->RemFrontend(this);
	return backends.erase(backend) == 1;
	}

bool nnc::AuthoritativeFrontend::Publish(const Publication* publication) const
	{
	for ( auto b : backends )
		b->Publish(publication);

	return true;
	}

bool nnc::AuthoritativeFrontend::DoInsert(const key_type& key,
                                          const value_type& val)
	{
	store[key] = val;
	++sequence;
	Publish(new ValUpdatePublication(Topic(), key, &val, sequence));
	return true;
	}

bool nnc::AuthoritativeFrontend::DoRemove(const key_type& key)
	{
	auto it = store.find(key);

	if ( it == store.end() )
		return false;

	store.erase(it);
	++sequence;
	Publish(new ValUpdatePublication(Topic(), key, nullptr, sequence));
	return true;
	}

bool nnc::AuthoritativeFrontend::DoIncrement(const key_type& key,
                                             const value_type& by)
	{
	auto it = store.find(key);

	if ( it == store.end() )
		return false;

	it->second += by;
	++sequence;
	Publish(new ValUpdatePublication(Topic(), key, &it->second, sequence));
	return true;
	}

bool nnc::AuthoritativeFrontend::DoDecrement(const key_type& key,
                                             const value_type& by)
	{
	auto it = store.find(key);

	if ( it == store.end() )
		return false;

	it->second -= by;
	++sequence;
	Publish(new ValUpdatePublication(Topic(), key, &it->second, sequence));
	return true;
	}

bool nnc::AuthoritativeFrontend::DoClear()
	{
	store.clear();
	++sequence;
	Publish(new ClearPublication(Topic(), sequence));
	return true;
	}

bool nnc::AuthoritativeFrontend::DoLookupAsync(const key_type& key,
                                               double timeout,
                                               lookup_cb cb) const
	{
	const value_type* val = nullptr;
	auto it = store.find(key);

	if ( it != store.end() )
		val = &it->second;

	cb(key, val, 0);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoHasKeyAsync(const key_type& key,
                                               double timeout,
                                               haskey_cb cb) const
	{
	cb(key, store.find(key) != store.end(), 0);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoSizeAsync(double timeout, size_cb cb) const
	{
	cb(store.size(), 0);
	return true;
	}

nnc::NonAuthoritativeFrontend::NonAuthoritativeFrontend(const string& topic)
	: nnc::Frontend(topic), backend(nullptr)
	{
	}

bool nnc::NonAuthoritativeFrontend::Pair(NonAuthoritativeBackend* arg_backend)
	{
	if ( backend )
		return false;

	// The backend must already be connected so that update publications may
	// be received _before_ the snapshot request is made, guaranteeing that the
	// received state snapshot is newer than oldest update publication.
	assert(arg_backend->Connected());

	if ( ! arg_backend->Connected() )
		return false;

	backend = arg_backend;
	backend->AddFrontend(this);
	return true;
	}

bool nnc::NonAuthoritativeFrontend::Unpair()
	{
	if ( ! backend )
		return false;

	backend = nullptr;
	backend->RemFrontend(this);
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoInsert(const key_type& key,
                                             const value_type& val)
	{
	if ( ! backend )
		return false;

	backend->SendUpdate(new InsertUpdate(Topic(), key, val));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoRemove(const key_type& key)
	{
	if ( ! backend )
		return false;

	backend->SendUpdate(new RemoveUpdate(Topic(), key));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoIncrement(const key_type& key,
                                                const value_type& by)
	{
	if ( ! backend )
		return false;

	backend->SendUpdate(new IncrementUpdate(Topic(), key, by));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoDecrement(const key_type& key,
                                                const value_type& by)
	{
	if ( ! backend )
		return false;

	backend->SendUpdate(new DecrementUpdate(Topic(), key, by));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoClear()
	{
	if ( ! backend )
		return false;

	backend->SendUpdate(new ClearUpdate(Topic()));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoLookupAsync(const key_type& key,
                                                  double timeout,
                                                  lookup_cb cb) const
	{
	if ( ! backend )
		return false;

	backend->SendRequest(new LookupRequest(Topic(), key, timeout, cb));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoHasKeyAsync(const key_type& key,
                                                  double timeout,
                                                  haskey_cb cb) const
	{
	if ( ! backend )
		return false;

	backend->SendRequest(new HasKeyRequest(Topic(), key, timeout, cb));
	return true;
	}

bool nnc::NonAuthoritativeFrontend::DoSizeAsync(double timeout,
                                                size_cb cb) const
	{
	if ( ! backend )
		return false;

	backend->SendRequest(new SizeRequest(Topic(), timeout, cb));
	return true;
	}
