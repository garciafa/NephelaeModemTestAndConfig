//
// Created by garciafa on 09/12/2019.
//
#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <regex>

namespace asio = boost::asio;

bool end = false;
asio::io_service ioService;
asio::serial_port *serialPort;
unsigned long total_recv=0;
unsigned long through_recv=0;
constexpr unsigned int max_payload = 255;
std::chrono::high_resolution_clock::time_point first;
std::chrono::high_resolution_clock::time_point last;

//boost::asio::streambuf recvBuffer;
unsigned char buff[max_payload];
boost::asio::mutable_buffer recvBuffer(buff,max_payload);

void sighandler(int signal)
{
    std::cout << "Signal " << signal << std::endl;
    if (end)
    {
        exit(EXIT_FAILURE);
    }
    end=true;
}

void readFollowing(const boost::system::error_code& e, std::size_t size)
{
    if (e)
    {
        std::cerr << e << std::endl;
    }
    else
    {
        total_recv += size;
        through_recv += size;
        std::chrono::high_resolution_clock::time_point current = std::chrono::high_resolution_clock::now();
        auto iat = std::chrono::duration_cast<std::chrono::microseconds>(current - last);
        double through = 1000000.0 * size / iat.count();

        //recvBuffer.consume(size);
        std::cout << "Received " << size << " bytes in " << iat.count()/1000.0 << " ms => " << through << " B/s (" << through * 8.0 << " b/s)"
                  << std::endl;
        last = current;
        asio::async_read(*serialPort,recvBuffer, asio::transfer_at_least(1), readFollowing);
    }
}

void readFirst(const boost::system::error_code& e, std::size_t size)
{
    if (e)
    {
        std::cerr << e << std::endl;
    }
    else
    {
        total_recv += size;
        first = std::chrono::high_resolution_clock::now();
        std::cout << "Received " << size << " bytes" << std::endl;
        //recvBuffer.consume(size);
        last = first = std::chrono::high_resolution_clock::now();
        asio::async_read(*serialPort, recvBuffer, asio::transfer_at_least(1), readFollowing);
    }
}

int main (int argc, char ** argv)
{
    std::string dev="/dev/ttyUSB0";
    const unsigned int baudRate = 57600;

    if (argc==2)
    {
        dev=argv[1];
    }

    serialPort= new asio::serial_port(ioService,dev);
    if (!serialPort->is_open())
    {
        std::cerr << "Could not open serial port " << dev << std::endl;
        exit(EXIT_FAILURE);
    }
    boost::system::error_code error;
    serialPort->set_option(boost::asio::serial_port_base::baud_rate(baudRate),error);
    if (!error)
    {
        std::cout << "Set speed to " << baudRate << " bauds on " << dev << std::endl;
    }
    else
    {
        std::cerr << "Could not set speed to " << baudRate << " bauds on " << dev << std::endl;
        exit(EXIT_FAILURE);
    }
    // Set 8N1 mode
    serialPort->set_option(asio::serial_port_base::character_size(8));
    serialPort->set_option(asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serialPort->set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
    // No flow control
    serialPort->set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

    signal(SIGINT,&sighandler);
    signal(SIGTERM,&sighandler);

    asio::async_read(*serialPort, recvBuffer, asio::transfer_at_least(1), readFirst);

    while(!end)
    {
        ioService.run_for(std::chrono::milliseconds(100));
    }

    serialPort->cancel(error);
    if (error)
    {
        std::cout << error << std::endl;
    }
    serialPort->close(error);
    if (error)
    {
        std::cout << error << std::endl;
    }
    delete(serialPort);
    ioService.stop();

    auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(last-first);
    double av_through = 1000000.0*total_recv/total_time.count();
    std::cout << "Received " << total_recv << " Bytes in " << total_time.count()/1000.0 << " ms" << std::endl;
    std::cout << "Average throughtput " << av_through << " B/s" << std::endl;

    return EXIT_SUCCESS;
}
