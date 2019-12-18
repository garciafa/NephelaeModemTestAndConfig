//
// Created by garciafa on 09/12/2019.
//
#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <regex>

namespace asio = boost::asio;

bool end = false;

void sighandler(int signal)
{
    std::cout << "Signal " << signal << std::endl;
    end=true;
}

int main (int argc, char ** argv)
{
    std::string dev;
    const unsigned int baudRate = 57600;
    auto duration = std::chrono::seconds(30);
    if (argc<3)
    {
        std::cerr << "usage :" << argv[0] << " <device file (e.g.: /dev/ttyUSB0)> <generated data rate (B/s)> [<duration of test in seconds (default=30s)>]\n";
        exit (EXIT_FAILURE);
    }
    dev=argv[1];
    if (argc==4)
    {
        int d;
        std::stringstream sstr(argv[3]);
        sstr >> d;
        if (sstr.fail())
        {
            std::cerr << "Last argument must be an integer number of seconds.\n";
            exit(EXIT_FAILURE);
        }
        duration=std::chrono::seconds(d);
    }
    unsigned int throughput;
    {
        std::stringstream sstr(argv[2]);
        sstr >> throughput;
        if (sstr.fail())
        {
            std::cerr << "Second argument must be an integer rate (B/s).\n";
            exit(EXIT_FAILURE);
        }
    }

    asio::io_service ioService;
    asio::serial_port serialPort(ioService,dev);
    if (!serialPort.is_open())
    {
        std::cerr << "Could not open serial port " << dev << std::endl;
        exit(EXIT_FAILURE);
    }
    boost::system::error_code error;
    serialPort.set_option(boost::asio::serial_port_base::baud_rate(baudRate),error);
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
    serialPort.set_option(asio::serial_port_base::character_size(8));
    serialPort.set_option(asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serialPort.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
    // No flow control
    serialPort.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

    signal(SIGINT,&sighandler);
    signal(SIGTERM,&sighandler);

    constexpr unsigned int max_payload = 255;
    double wait_sec = (double)max_payload / (double)throughput;
    unsigned int wait_usec = (int)(wait_sec*1000000.0);

    unsigned char buffer[max_payload];
    for (int i=0;i<max_payload;++i)
    {
        buffer[i]= 'a' + (i % 26);
    }
    buffer[max_payload-1]='\n';
    unsigned long total_sent=0;
    unsigned long nb_sent=0;
    std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
    std::chrono::high_resolution_clock::time_point last = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point first = last;

    while (!end && (std::chrono::system_clock::now()-begin)<duration)
    {
        serialPort.write_some(asio::buffer(buffer, max_payload));
        nb_sent++;
        total_sent += max_payload;
        std::chrono::high_resolution_clock::time_point current = std::chrono::high_resolution_clock::now();
        auto iat = std::chrono::duration_cast<std::chrono::microseconds>(current - last);
        last = current;

        double through = 1000000.0 * max_payload / iat.count();
        std::cout << max_payload << " bytes in " << iat.count() << " us => " << through << " B/s (" << through * 8
                  << " b/s)" << std::endl;

        std::chrono::microseconds usec = std::chrono::duration_cast<std::chrono::microseconds>((nb_sent*std::chrono::microseconds(wait_usec)) - (std::chrono::high_resolution_clock::now()-first));
        usleep(usec.count());
    }

    std::cout << "Sent " << total_sent << " Bytes in " << nb_sent << " packets" << std::endl;
    try
    {
        serialPort.cancel();

        serialPort.close();
    } catch (boost::system::system_error e)
    {
        std::cout << e.what() << std::endl;
    }
    return EXIT_SUCCESS;
}
