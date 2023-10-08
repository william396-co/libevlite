#include "client.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <string>
#include <cstdlib>
#include "random_util.h"

void rand_str( std::string & str, size_t max = 2000 )
{
    str = random_string( max - 500, max );
}

Client::Client( const char * ip, uint16_t port, uint32_t conv )
    : socket_ { nullptr }, md { 0 }
{
#ifndef USE_TCP
    socket_ = std::make_unique<UdpSocket>();
#else
    socket_ = std::make_unique<Socket>();
    socket_->setNonblocking();
#endif
    if ( !socket_->connect( ip, port ) ) {
        throw std::runtime_error( "socket_ connect failed" );
    }
#ifdef USE_TCP
    socket_->setNonblocking();
#endif
#ifndef USE_TCP
    kcp = ikcp_create( conv, socket_.get() );
    ikcp_setoutput( kcp, util::kcp_output );
#endif
}

Client::~Client()
{
#ifndef USE_TCP
    ikcp_release( kcp );
#endif
}

void Client::setmode( int mode )
{
#ifndef USE_TCP
    util::ikcp_set_mode( kcp, mode );
    md = mode;
#endif
}

void Client::send( const char * data, size_t len )
{
    char buff[BUFFER_SIZE] = {};
    ( (uint32_t *)buff )[0] = sn++;
    ( (uint32_t *)buff )[1] = util::iclock();
    ( (uint32_t *)buff )[2] = (uint32_t)len;

    if ( show_info ) {
        printf( "Send idx:%d sn:%u size:%u content {%s}\n", idx, sn - 1, len + 12, data );
    } else {
        printf( "Send idx:%u sn:%u size:%u\n", idx, sn - 1, len + 12 );
    }
    memcpy( &buff[12], data, len );
#ifndef USE_TCP
    ikcp_send( kcp, buff, len + 12 );
    ikcp_update( kcp, util::iclock() );
#else
    socket_->send( buff, len + 12 );
#endif
}

void Client::input()
{
    std::string writeBuffer;
    while ( is_running ) {
        printf( "Please enter a string to send to server(%s:%d):\n", socket_->getRemoteIp(), socket_->getRemotePort() );

        writeBuffer.clear();
        std::getline( std::cin, writeBuffer );
        if ( !writeBuffer.empty() ) {
            send( writeBuffer.data(), writeBuffer.size() );
        }
    }
}

void Client::recv( const char * data, size_t len )
{
    if ( data && len ) {
        m_readBuffer.append( data, len );
        m_recvBytes += len;
    }

    char * ptr_ = m_readBuffer.data();
    while ( m_recvBytes > 0 ) {

        uint32_t sn_ = *(uint32_t *)( ptr_ );
        uint32_t ts_ = *(uint32_t *)( ptr_ + 4 );
        uint32_t sz_ = *(uint32_t *)( ptr_ + 8 ) + 12;

        if ( sz_ > m_recvBytes ) {
            break;
        }

        uint32_t rtt_ = util::iclock() - ts_;
        if ( sn_ != (uint32_t)next ) {
            printf( "ERROR sn %u<-> next=%d\n", sn, next );
            is_running = false;
        }
        ++next;
        sumrtt += rtt_;
        ++count;
        maxrtt = rtt_ > maxrtt ? rtt_ : maxrtt;

        if ( show_info )
            printf( "[RECV] idx:%u mode=%d sn:%d rrt:%d size:%u  content: {%s}\n", idx, md, sn_, rtt_, sz_, (char *)&ptr_[12] );
        else
            printf( "[RECV] idx:%u mode=%d sn:%d rrt:%d size:%u \n", idx, md, sn_, rtt_, sz_ );

        if ( next >= test_count ) {
            printf( "Finished %d times test\n", test_count );
            is_running = false;
        }
        ptr_ += sz_;
        m_recvBytes -= sz_;
    }

    if ( m_recvBytes == 0 ) {
        m_readBuffer.clear();
    } else {
        m_readBuffer.substr( m_readBuffer.size() - m_recvBytes );
    }
}

void Client::run()
{
    auto current_ = util::now_ms();
#ifndef USE_TCP
    char buff[BUFFER_SIZE];
#endif

    while ( is_running ) {
        util::isleep( 1 );
#ifndef USE_TCP
        ikcp_update( kcp, util::iclock() );
#endif

        // auto input test
        if ( auto_test && util::now_ms() - current_ >= send_interval ) {
            if ( sn >= test_count ) {
                printf( "finished auto send times=%d\n", sn );
                auto_test = false;
            }

            current_ = util::now_ms();
            std::string writeBuffer;
            rand_str( writeBuffer, str_max_len );
            if ( !writeBuffer.empty() ) {
                send( writeBuffer.data(), writeBuffer.size() );
            }
        }

        // recv pack
#ifndef USE_TCP
        if ( socket_->recv() < 0 ) {
            continue;
        }
        ikcp_input( kcp, socket_->getRecvBuffer(), socket_->getRecvSize() );
        bzero( buff, sizeof( buff ) );
        int rc = ikcp_recv( kcp, buff, sizeof( buff ) );
        if ( rc < 0 ) continue;
        recv( buff, rc );
#else
        socket_->recv();
        if ( socket_->getRecvSize() <= 0 ) {
            continue;
        }
        recv( socket_->getRecvBuffer(), socket_->getRecvSize() );
#endif
    }

    /* summary */
    if ( count > 0 )
#ifndef USE_TCP
        printf( "\nIDX=[%d] MODE=[%d] DATASIZE=[%d] LOSTRATE=[{%u/%u} = %.5f] avgrtt=%d maxrtt=%d count=%d \n",
            idx,
            md,
            str_max_len,
            kcp->resend_cnt,
            kcp->snd_nxt,
            (double)kcp->resend_cnt / kcp->snd_nxt,
            int( sumrtt / count + 1 ),
            maxrtt,
            count + 1 );
#else
        socket_->close();
    printf( "\nIDX=[%d] MODE=[TCP] DATASIZE=[%d] LOSTRATE=[{%u/%u} = %.5f] avgrtt=%d maxrtt=%d count=%d \n",
        idx,
        str_max_len,
        0,
        0,
        0.0f,
        int( sumrtt / count ),
        maxrtt,
        count );
#endif
}
