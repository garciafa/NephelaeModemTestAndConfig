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

void readPing(const boost::system::error_code& e, std::size_t size)
{
    if (e)
    {
        std::cerr << e << std::endl;
    }
    else
    {
        total_recv += size;
        std::cout << ".";
        std::cout.flush();
        serialPort->write_some(asio::buffer(recvBuffer, size));

        asio::async_read(*serialPort, recvBuffer, asio::transfer_at_least(1), readPing);
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

    asio::async_read(*serialPort, recvBuffer, asio::transfer_at_least(1), readPing);

    while(!end)
    {
        ioService.run_for(std::chrono::milliseconds(100));
    }
    std::cout << std::endl;
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

    return EXIT_SUCCESS;
}
