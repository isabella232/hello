#include "server.hpp"

#include <iomanip>

#include <boost/endian/conversion.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include "clienthelloparser.hpp"
#include "kaitai/tls_record.h"
#include "utils.hpp"
#include "log.hpp"

// derived from
// https://theboostcpplibraries.com/boost.asio-network-programming#ex.asio_06
class Server {
    private:
        boost::asio::ip::tcp::socket tcp_socket;
        boost::asio::ip::tcp::endpoint tcp_client_endpoint;
        boost::asio::ip::tcp::endpoint tcp_server_endpoint;
        boost::asio::ip::tcp::acceptor tcp_acceptor;
        std::string dataTls;
        std::string dataHello;

    private:
        void read_ClientHello( const boost::system::error_code& /*ec*/,
                               std::size_t bytes_transferred ) {

            LOG( bytes_transferred << "B" );
            LOG( utils::dump( dataHello ) );

            ClientHelloParser parser( dataHello );
            parser.parse();
            parser.dump();

            LOG_DEBUG( "\nFinished" );
            tcp_socket.close();
        }
        void read_TLSRecord( const boost::system::error_code& /*ec*/,
                             std::size_t bytes_transferred ) {

            LOG( bytes_transferred << "B" );
            LOG( utils::dump( dataTls ) );

            kaitai::kstream ks( dataTls );
            tls_record_t kt_record( &ks );

            LOG( "\n" );
            LOG( "Type          : " << std::hex << static_cast<int>( kt_record.type() ) );
            LOG( "TLS Version   : " << std::hex << kt_record.version() );
            LOG( "Length        : " << std::dec << kt_record.length() );

            dataHello.resize( kt_record.length() );
            boost::asio::async_read( tcp_socket,
                                     boost::asio::buffer( dataHello ),
                                     boost::bind( &Server::read_ClientHello, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) );
        }
        void accept_handler( const boost::system::error_code& /*ec*/ ) {
            size_t TLSRecordSize = 5;
            dataTls.resize( TLSRecordSize );
            LOG( tcp_server_endpoint << " << " << tcp_client_endpoint );
            LOG( tcp_socket.available() );
            boost::asio::async_read( tcp_socket,
                                     boost::asio::buffer( dataTls ),
                                     boost::bind( &Server::read_TLSRecord, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) );
        }

    public:
        Server( boost::asio::io_context& iocontext ) :
            tcp_socket{iocontext},
            tcp_server_endpoint{boost::asio::ip::tcp::v4(), 2014},
            tcp_acceptor{iocontext, tcp_server_endpoint} {

            LOG( "Visit https://" << tcp_server_endpoint );
            tcp_acceptor.listen();
            tcp_acceptor.async_accept( tcp_socket,
                                       tcp_client_endpoint,
                                       boost::bind( &Server::accept_handler, this, boost::asio::placeholders::error ) );
        }
};

void server::run() {
    boost::asio::io_context iocontext;
    Server server( iocontext );
    iocontext.run();
}
