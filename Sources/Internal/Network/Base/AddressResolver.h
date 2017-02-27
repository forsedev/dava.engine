#pragma once

#include <libuv/uv.h>

#include "Base/BaseTypes.h"
#include "Base/Platform.h"
#include "Functional/Function.h"
#include "Network/Base/Endpoint.h"
#include "Network/NetEventsDispatcher.h"

namespace DAVA
{
namespace Net
{
class IOLoop;

class AddressResolver
{
public:
    using ResolverCallbackFn = Function<void(const Endpoint&, int32)>;

public:
    explicit AddressResolver(IOLoop* loop, NetEventsDispatcher* netEventsDispatcher);
    ~AddressResolver();

    void AsyncResolve(const char8* address, uint16 port, ResolverCallbackFn cbk);
    void Cancel();

private:
    void DoAsyncResolve(const char8* address, uint16 port, ResolverCallbackFn cbk);
    static void DoCancel(uv_getaddrinfo_t* handle);
    static void GetAddrInfoCallback(uv_getaddrinfo_t* handle, int status, addrinfo* response);

    void GotAddrInfo(int status, addrinfo* response);

private:
    IOLoop* loop = nullptr;
    NetEventsDispatcher* netEventsDispatcher = nullptr;
    Mutex handleMutex;
    uv_getaddrinfo_t* handle = nullptr;
    ResolverCallbackFn resolverCallbackFn = nullptr;
};
}
}
