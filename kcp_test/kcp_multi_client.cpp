#include <signal.h>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <functional>
#include <algorithm>

#include "client.h"
#include "joining_thread.h"

constexpr auto default_ip = "172.28.1.13";
constexpr auto default_port = 9527;
constexpr auto default_max_len = 1000;
constexpr auto default_test_times = 1000;
constexpr auto default_lost_rate = 0;
constexpr auto default_send_interval = 60; // ms
constexpr auto default_thread_cnt = 10;
constexpr auto clients_per_thread = 1;

static bool g_running = true;


void signal_handler( int sig )
{
    g_running = false;
}
void handle_signal()
{
    signal( SIGPIPE, SIG_IGN );
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
}

std::unique_ptr<Client> start_client( const char * ip, uint16_t port, int mode, int max_len, int test_times, int lost_rate, int interval = default_send_interval )
{
    std::unique_ptr<Client> client = std::make_unique<Client>( ip, port, conv );
    client->setmode( mode );
    client->setauto( true, test_times, max_len );
    client->setlostrate( lost_rate );
    client->setsendinterval( interval );
    client->set_show_info( true );
    //  client->set_show_detail( true );
    // client->set_delay( true );
    return client;
}

int main( int argc, char ** argv )
{
    handle_signal();

    int mode = 2;
    uint16_t port = default_port;
    std::string ip = default_ip;
    int max_len = default_max_len;
    int test_times = default_test_times;
    int lost_rate = default_lost_rate;
    int send_interval = default_send_interval;
    int thread_cnt = default_thread_cnt;

    if ( argc > 1 ) {
        thread_cnt = atoi( argv[1] );
    }

    printf( "Usage:<%s> ThreadCount:%d\n", argv[0], thread_cnt );

    std::vector<std::unique_ptr<Client>> clients;
    for ( int i = 0; i != thread_cnt; ++i ) {
        clients.emplace_back( start_client( ip.c_str(), port, mode, max_len, test_times, lost_rate, send_interval ) );
    }

    std::vector<joining_thread> threads;
    for ( int i = 0; i != thread_cnt; ++i ) {
        threads.emplace_back( &Client::run, clients[i].get() );
    }

    printf( "client cnt:%llu thread cnt:%llu\n", clients.size(), threads.size() );
    while ( g_running ) {
        std::this_thread::sleep_for( std::chrono::milliseconds { 1 } );
    }

    for ( auto & c : clients ) {
        c->terminate();
    }

    return 0;
}
