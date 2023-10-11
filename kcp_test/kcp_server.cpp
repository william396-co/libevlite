#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>
#include <string>

#include "io.h"

constexpr auto BUFF_SIZE = 1024 * 8;
constexpr auto default_port = 9527;
bool g_running = true;

class ClientSession : public IIOSession
{
public:
    virtual ~ClientSession() override {}

    virtual ssize_t onProcess( const char * buf, size_t nbytes ) override
    {

        if ( buf && nbytes ) {
            read_buffer.append( buf, nbytes );
            recv_bytes += nbytes;
        }

        //printf( "%s size:%lu\n", __FUNCTION__, nbytes );
        char * ptr_ = read_buffer.data();
        while ( recv_bytes > 12 ) {
            uint32_t sn = *(uint32_t *)ptr_;
            uint32_t sz = *(uint32_t *)( ptr_ + 8 ) + 12;

            if ( sz > recv_bytes )
                break;

            if ( show_data ) {
                if ( show_detail )
                    printf( "Recv fd:[%llu] port:[%d] sn:[%d] size:[%u] content:[%s]\n", id(), port(), sn, sz, &ptr_[12] );
                else
                    printf( "Recv fd:[%llu] port:[%d] sn:[%d] size:[%u]\n", id(), port(), sn, sz );
            }
           // printf( "Send sn:[%d] size:[%u]\n", sn, sz );
            send( ptr_, sz );
            ptr_ += sz; // ptr move forward
            recv_bytes -= sz;
        }

        if ( recv_bytes == 0 ) {
            read_buffer.clear();
        } else {
            read_buffer.substr( read_buffer.size() - recv_bytes );
        }
        return nbytes;
    }

    virtual int32_t onTimeout() override
    {
        printf( "the Session (SID=%ld) Start (%s::%d)  \n", id(), host().c_str(), port() );
        return -1;
    }

    virtual int32_t onError( int32_t result ) override
    {
        printf( "the session (SID=%ld): error, code=0x%08x\n", id(), result );
        return -1;
    }

    virtual void onShutdown( int32_t way ) override
    {
        printf( "the session (SID=%ld port:%d): shutdown on way:%d\n", id(), port(), way );
    }

    void set_lost_rate( int lost_rate_ )
    {
        lost_rate = lost_rate_;
    }

    void set_show_data( bool show_ )
    {
        show_data = show_;
    }

    void set_show_detail( bool show_ )
    {
        show_detail = show_;
    }

private:
    int lost_rate = 0;
    bool show_data = false;
    bool show_detail = false;
    std::string read_buffer;
    uint32_t recv_bytes = 0;
};

class ListenSession : public IIOService
{
public:
    ListenSession( uint8_t nthreads, uint32_t nclients )
        : IIOService( nthreads, nclients ) {}

    ~ListenSession() {}

    virtual IIOSession * onAccept( sid_t id, NetType type, uint16_t listen_port, const char * host, uint16_t port ) override
    {
        printf( "sid: %ld,connected by [%s:%d]\n", id, host, port );
        auto s = new ClientSession();
        s->set_lost_rate( 10 );
        s->set_show_data( true );
        //  s->set_show_detail( true );
        //  s->setWindowSize( 128, 128 );
        return s;
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

    if ( argc > 1 ) {
        port = atoi( argv[1] );
    }

    std::unique_ptr<ListenSession> listen = std::make_unique<ListenSession>( 4, 20000 );
    listen->start();

    printf( "Usage: <%s> port:<%d> starting running\n", argv[0], port );

#ifndef USE_TCP
    if ( !listen->listen( NetType::KCP, "0.0.0.0", port, nullptr ) ) {
#else
    if ( !listen->listen( NetType::TCP, "0.0.0.0", port, nullptr ) ) {
#endif

        printf( "server listen [%d] failed\n", port );
        return -2;
    }

    while ( g_running ) {
        std::this_thread::sleep_for( std::chrono::milliseconds { 1 } );
    }

    printf( "%s stoping....\n", argv[0] );
    listen->stop();

    return 0;
}
