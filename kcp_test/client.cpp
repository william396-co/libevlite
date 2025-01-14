#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <string>
#include <cstdlib>
#include "random_util.h"

static int g_sn = 0;

void rand_str( std::string & str, size_t max = 2000 )
{
    size_t sz = random( 8, max );
    str = random_string( sz );
}

Client::Client( const char * ip, uint16_t port, uint32_t conv )
    : client { nullptr }, md { 0 }
{
    client = std::make_unique<UdpSocket>();
    client->setNonblocking();
    if ( !client->connect( ip, port ) ) {
        throw std::runtime_error( "client connect failed" );
    }

    kcp = ikcp_create( conv, client.get() );
    ikcp_setoutput( kcp, util::kcp_output );
}

Client::~Client()
{
    ikcp_release( kcp );
}

void Client::setmode( int mode )
{
    util::ikcp_set_mode( kcp, mode );
    md = mode;
}

void Client::auto_input()
{
    std::string str;
    auto current = util::now_ms();
    while ( is_running ) {
        if ( !auto_test )
            break;

        util::isleep( 1 );

        if ( g_sn >= test_count ) {
            printf( "finished auto send times=%d\n", g_sn );
            break;
        }

        if ( util::now_ms() - current < send_interval )
            continue;

        current = util::now_ms();
        rand_str( str, str_max_len );
        if ( !str.empty() ) {
            char buff[BUFFER_SIZE];
            ( (IUINT32 *)buff )[0] = g_sn++;
            ( (IUINT32 *)buff )[1] = util::iclock();
            ( (uint32_t *)buff )[2] = (uint32_t)str.size();

            if ( auto_test )
                printf( "auto_input sn:%u size:%lu\n", g_sn - 1, str.size() + 12 );
            else
                printf( "auto_input sn:%u content:{%s}\n", g_sn - 1, str.c_str() + 12 );

            memcpy( &buff[12], str.data(), str.size() );
            ikcp_send( kcp, buff, str.size() + 12 );
            ikcp_update( kcp, util::iclock() );
        }
    }
}

void Client::input()
{
    std::string writeBuffer;
    char buff[BUFFER_SIZE];
    while ( is_running ) {
        printf( "Please enter a string to send to server(%s:%d):\n", client->getRemoteIp(), client->getRemotePort() );

        writeBuffer.clear();
        bzero( buff, sizeof( buff ) );
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            ( (IUINT32 *)buff )[0] = g_sn++;
            ( (IUINT32 *)buff )[1] = util::iclock();
            ( (uint32_t *)buff )[2] = (uint32_t)writeBuffer.size();

            memcpy( &buff[12], writeBuffer.data(), writeBuffer.size() );
            ikcp_send( kcp, buff, writeBuffer.size() + 12 );
            ikcp_update( kcp, util::iclock() );
        }
    }
}

void Client::run()
{
    char buff[BUFFER_SIZE];
    int next = 0;
    uint32_t sumrtt = 0;
    uint32_t count = 0;
    uint32_t maxrtt = 0;

    while ( is_running ) {
        util::isleep( 1 );
        ikcp_update( kcp, util::iclock() );

        // udp pack received
        if ( client->recv() < 0 ) {
            continue;
        }
        ikcp_input( kcp, client->getRecvBuffer(), client->getRecvSize() );

        bzero( buff, sizeof( buff ) );
        // user/logic package received
        int rc = ikcp_recv( kcp, buff, BUFFER_SIZE );
        if ( rc < 0 )
            continue;

        char * ptr_ = buff;
        printf( "Recv total size:%d\n", rc );
        while ( int( ptr_ - buff ) < rc ) {
            IUINT32 sn = *(IUINT32 *)( ptr_ );
            IUINT32 ts = *(IUINT32 *)( ptr_ + 4 );
            uint32_t sz = *(uint32_t *)( ptr_ + 8 );
            IUINT32 rtt = util::iclock() - ts;

            if ( sn != (uint32_t)next ) {
                printf( "ERROR sn %u<-> next=%d\n", sn, next );
                is_running = false;
                break;
            }
            ++next;
            sumrtt += rtt;
            ++count;
            maxrtt = rtt > maxrtt ? rtt : maxrtt;

            if ( !auto_test )
                printf( "[RECV] mode=%d sn:%d rrt:%d size:%u  content: {%s}\n", md, sn, rtt, sz + 12, (char *)&ptr_[12] );
            else
                printf( "[RECV] mode=%d sn:%d rrt:%d size:%u \n", md, sn, rtt, sz + 12 );

            if ( next >= test_count ) {
                printf( "Finished %d times test\n", test_count );
                is_running = false;
                break;
            }
            ptr_ += sz + 12;
        }
    }

    /* summary */
    if ( count > 0 )
        printf( "\nMODE=[%d] DATASIZE=[%d] LOSTRATE=[%d%] avgrtt=%d maxrtt=%d count=%d \n", md, str_max_len, lost_rate, int( sumrtt / count ), maxrtt, count );
}
