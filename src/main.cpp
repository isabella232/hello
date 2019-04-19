#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/endian/conversion.hpp>

#include <string>
#include <iostream>
#include <iomanip>

#define LOG( A ) std::cout << A << std::endl;
#define LOG_DEBUG( A ) LOG( A )

namespace {
boost::asio::io_service ioservice;
boost::asio::ip::tcp::acceptor tcp_acceptor{ioservice, {boost::asio::ip::tcp::v4(), 2014}};
boost::asio::ip::tcp::socket tcp_socket{ioservice};
std::string dataTls;
std::string dataHello;
}

uint32_t toLittleEndianfrom3Bytes( uint32_t size ) {
    uint8_t* arr = reinterpret_cast<uint8_t*>( &size );
    uint32_t result = ( static_cast<uint32_t>( arr[0] ) << 16 ) |
                      ( static_cast<uint32_t>( arr[1] ) << 8 ) |
                      ( static_cast<uint32_t>( arr[2] ) );
    return result;
}

std::string dump( const size_t size, const uint8_t* data ) {
    std::stringstream out;

    for( size_t i = 0; i < size; ) {
        out << std::setw( 2 ) << std::setfill( '0' ) << std::hex << static_cast<int>( data[i] ) << " ";

        if( ++i % 16 == 0 ) {
            out << std::endl;
        }
    }

    return out.str();
}

typedef struct __attribute__( ( packed ) ) {
    uint8_t type; // 0x01
    uint32_t size : 24;
    uint16_t version;
    uint8_t random[32];
} TLSHello;

void read_ClientHello( const boost::system::error_code& /*ec*/,
                       std::size_t bytes_transferred ) {

    LOG( bytes_transferred << "B" );
    LOG( dump( bytes_transferred, reinterpret_cast<const uint8_t*>( dataHello.data() ) ) );

    const TLSHello* hello = reinterpret_cast<const TLSHello*>( dataHello.data() );
    LOG( "\n" );
    LOG( "Type          : " << std::hex << static_cast<int>( hello->type ) );
    LOG( "Hello Size    : " << std::dec << toLittleEndianfrom3Bytes( hello->size ) );
    LOG( "Hello Version : " << std::hex << boost::endian::big_to_native( hello->version ) );
    LOG( "Random        : " << dump( 32, hello->random ) );

    LOG_DEBUG( "\nShutting down connection" );
    tcp_socket.shutdown( boost::asio::ip::tcp::socket::shutdown_send );
}

// https://wiki.osdev.org/TLS_Handshake
typedef struct __attribute__( ( packed ) ) {
    uint8_t content_type;  // 0x16
    uint16_t version;
    uint16_t length;
} TLSRecord;

void read_TLSRecord( const boost::system::error_code& /*ec*/,
                     std::size_t bytes_transferred ) {

    LOG( bytes_transferred << "B" );
    LOG( dump( bytes_transferred, reinterpret_cast<const uint8_t*>( dataTls.data() ) ) );

    const TLSRecord* record = reinterpret_cast<const TLSRecord*>( dataTls.data() );

    LOG( "\n" );
    LOG( "Type        : " << std::hex << static_cast<int>( record->content_type ) );
    LOG( "TLS Version : " << std::hex << boost::endian::big_to_native( record->version ) );
    LOG( "Length      : " << std::dec << boost::endian::big_to_native( record->length ) );

    dataHello.resize( record->length );
    boost::asio::async_read( tcp_socket, boost::asio::buffer( &dataHello[0], dataHello.size() ), read_ClientHello );
}

void accept_handler( const boost::system::error_code& ec ) {
    if( !ec ) {
        dataTls.resize( sizeof( TLSRecord ) );
        boost::asio::async_read( tcp_socket, boost::asio::buffer( &dataTls[0], dataTls.size() ), read_TLSRecord );
    }
}

// trigger with
// openssl s_client -connect localhost:2014
int main() {
    tcp_acceptor.listen();
    tcp_acceptor.async_accept( tcp_socket, accept_handler );
    ioservice.run();
}