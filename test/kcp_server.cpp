#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>

#include "io.h"

constexpr auto default_port = 9527;
bool g_running = true;

class ClientSession : public IIOSession
{
};

class ListenSession : public IIOService
{
public:
    ListenSession( uint8_t nthreads, uint32_t nclients )
        : IIOService( nthreads, nclients ) {}

    ~ListenSession() {}

    IIOSession * onAccept( sid_t id, uint16_t listen_port, const char * host, uint16_t port )
    {
        return new ClientSession();
    }
};

void signal_handle( int sign )
{
    g_running = false;
}

int main( int argc, char ** argv )
{

    uint16_t port = default_port;

    signal( SIGPIPE, SIG_IGN );
    signal( SIGINT, signal_handle );

    std::unique_ptr<ListenSession> listen = std::make_unique<ListenSession>( 4, 20000 );
    listen->start();

    if ( !listen->listen( NetType::KCP, "127.0.0.1", port, nullptr ) ) {

        printf( "server listen [%d] failed\n", port );
        return -2;
    }

    while ( g_running ) {
        std::this_thread::sleep_for( std::chrono::milliseconds { 1 } );
    }
    listen->stop();

    return 0;
}
