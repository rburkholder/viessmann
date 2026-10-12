#include <thread>

#include <boost/log/trivial.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include <boost/asio/ssl.hpp>

#include <ou/rest/handler.hpp>
//#include <ou/rest/root_certificates.hpp>

#include <boost/asio/strand.hpp>
#include <boost/log/trivial.hpp>

#include <ou/rest/one_shot.hpp>

namespace asio = boost::asio; // from <boost/asio/context.hpp>
namespace ssl  = asio::ssl;   // from <boost/asio/ssl.hpp>

static const std::string c_sHost( "api.telegram.org" );
static const std::string c_sPort( "443" );

int main(int argc, char **argv) {

  std::thread m_thread;

  boost::asio::io_context m_io;
  ssl::context m_ssl_context( ssl::context::tlsv12_client );

  using work_guard_t = asio::executor_work_guard<boost::asio::io_context::executor_type>;
  using pWorkGuard_t = std::unique_ptr<work_guard_t>;

  pWorkGuard_t m_pWorkGuard;

  m_pWorkGuard = std::make_unique<work_guard_t>( asio::make_work_guard( m_io ) );
  //m_thread = std::move( std::thread( [&m_io](){ m_io.run(); } ) );

  auto request = std::make_shared<ou::rest::one_shot>( asio::make_strand( m_io ), m_ssl_context );
  request->get(
      c_sHost, c_sPort
    , "/url/command"
    , [&m_pWorkGuard]( bool bStatus, int ec, const std::string& message ){
        BOOST_LOG_TRIVIAL(info) << "status=" << bStatus << ", message=" << message;
        if ( bStatus ) {
        }
        m_pWorkGuard->reset();
      }
    );

  m_io.run();

  return EXIT_SUCCESS;
}