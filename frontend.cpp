#include "frontend.hpp"
#include "backend.hpp"
#include "messages.hpp"
#include "util.hpp"

#include <memory>
#include <cinttypes>
#include <assert.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

using namespace std;
using namespace nnc;

void nnc::Frontend::DumpDebug(FILE* out) const
	{
	string header = "========== Store " + topic + " Contents ==========";
	fprintf(out, "%s\n", header.c_str());

	for ( auto it = store.begin(); it != store.end(); ++it )
		// TODO: technically keys aren't always printable c-strings
		fprintf(out, "%s: %" PRIi64 "\n", it->first.c_str(), it->second);

	for ( size_t i = 0; i < header.size(); ++i )
		fprintf(out, "=");
	fprintf(out, "\n");
	}

const nnc::value_type* nnc::Frontend::LookupSync(const nnc::key_type& key) const
	{
	auto it = store.find(key);

	if ( it == store.end() )
		return nullptr;

	return &it->second;
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

unique_ptr<Response> nnc::AuthoritativeFrontend::Snapshot() const
	{
	return unique_ptr<Response>(new SnapshotResponse(store, sequence));
	}

bool nnc::AuthoritativeFrontend::DoInsert(const key_type& key,
                                          const value_type& val)
	{
	store[key] = val;
	++sequence;
	auto p = make_shared<ValUpdatePublication>(Topic(), key, &val, sequence);
	for ( auto b : backends ) b->Publish(p);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoRemove(const key_type& key)
	{
	auto it = store.find(key);

	if ( it == store.end() )
		return false;

	store.erase(it);
	++sequence;
	auto p = make_shared<ValUpdatePublication>(Topic(), key, nullptr, sequence);
	for ( auto b : backends ) b->Publish(p);
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
	auto p = make_shared<ValUpdatePublication>(Topic(), key, &it->second,
	                                           sequence);
	for ( auto b : backends ) b->Publish(p);
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
	auto p = make_shared<ValUpdatePublication>(Topic(), key, &it->second,
	                                           sequence);
	for ( auto b : backends ) b->Publish(p);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoClear()
	{
	store.clear();
	++sequence;
	auto p = make_shared<ClearPublication>(Topic(), sequence);
	for ( auto b : backends ) b->Publish(p);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoLookupAsync(const key_type& key,
                                               double timeout,
                                               lookup_cb cb) const
	{
	unique_ptr<value_type> val(nullptr);
	auto it = store.find(key);

	if ( it != store.end() )
		val.reset(new value_type(it->second));

	cb(key, move(val), ASYNC_SUCCESS);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoHasKeyAsync(const key_type& key,
                                               double timeout,
                                               haskey_cb cb) const
	{
	cb(key, store.find(key) != store.end(), ASYNC_SUCCESS);
	return true;
	}

bool nnc::AuthoritativeFrontend::DoSizeAsync(double timeout, size_cb cb) const
	{
	cb(store.size(), ASYNC_SUCCESS);
	return true;
	}

bool
nnc::NonAuthoritativeFrontend::ApplySnapshot(std::unique_ptr<Response> snapshot)
	{
	SnapshotResponse* r = dynamic_cast<SnapshotResponse*>(snapshot.get());

	if ( ! r )
		return false;

	sequence = r->Sequence();
	store = r->Store();

	while ( ! pub_backlog.empty() )
		{
		unique_ptr<Publication> pub = move(pub_backlog.front());
		pub_backlog.pop();

		if ( pub->Sequence() == sequence + 1 )
			pub->Apply(store);
		}

	synchronized = true;
	return true;
	}

bool nnc::NonAuthoritativeFrontend::ProcessPublication(
        std::unique_ptr<Publication> pub)
	{
	if ( ! synchronized )
		{
		pub_backlog.push(move(pub));
		return false;
		}

	if ( pub->Sequence() == sequence + 1 )
		{
		pub->Apply(store);
		return true;
		}

	pub_backlog = {};
	synchronized = false;
	backend->SendRequest(new SnapshotRequest(topic));
	return false;
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
