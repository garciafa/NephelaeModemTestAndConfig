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
    //std::cout << "Signal " << signal << std::endl;
    end=true;
}

template <typename SyncReadStream, typename MutableBufferSequence>
size_t readWithTimeout(SyncReadStream& s, const MutableBufferSequence& buffers, const boost::asio::deadline_timer::duration_type& expiry_time,
boost::system::error_code &error)
{
    size_t nb_read=0;
    std::optional<boost::system::error_code> timer_result;
    asio::deadline_timer timer(s.get_io_service());
    timer.expires_from_now(expiry_time);
    timer.async_wait([&timer_result,&nb_read] (const boost::system::error_code& error) { timer_result=error;});

    std::optional<boost::system::error_code> read_result;
    asio::async_read(s, buffers, asio::transfer_at_least(1), [&read_result,&nb_read] (const boost::system::error_code& error, size_t s) { read_result=error;nb_read=s; });

    s.get_io_service().reset();
    while (s.get_io_service().run_one())
    {
        if (read_result)
        {
            if (read_result.value()!=boost::system::errc::errc_t::operation_canceled)
            {
                timer.cancel();
                error = *read_result;
            }
        }
        else if (timer_result)
        {
            s.cancel();
            error.assign(boost::system::errc::errc_t::success,timer_result->category());
        }
    }
    return nb_read;
}

int main (int argc, char ** argv)
{
    std::string dev;
    const unsigned int baudRate = 57600;
    auto duration = std::chrono::seconds(30);
    if (argc != 3)
    {
        std::cerr << "usage :" << argv[0]
                  << " <device file (e.g.: /dev/ttyUSB0)> <duration of test in seconds (default=30s)>\n";
        exit(EXIT_FAILURE);
    }
    dev = argv[1];
    int d;
    std::stringstream sstr(argv[2]);
    sstr >> d;
    if (sstr.fail())
    {
        std::cerr << "Last argument must be an integer number of seconds.\n";
        exit(EXIT_FAILURE);
    }
    duration = std::chrono::seconds(d);

    asio::io_service ioService;
    asio::serial_port serialPort(ioService, dev);
    if (!serialPort.is_open())
    {
        std::cerr << "Could not open serial port " << dev << std::endl;
        exit(EXIT_FAILURE);
    }
    boost::system::error_code error;
    serialPort.set_option(boost::asio::serial_port_base::baud_rate(baudRate), error);
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

    signal(SIGINT, &sighandler);
    signal(SIGTERM, &sighandler);

    constexpr unsigned int payload_size = 1;

    unsigned char buffer[payload_size];

    unsigned long nb_sent = 0;
    unsigned long nb_recv = 0;
    std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
    std::chrono::high_resolution_clock::time_point last = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point first = last;
    unsigned char buff[payload_size];
    boost::asio::mutable_buffer recvBuffer(buff,payload_size);
    unsigned long rtt_sum = 0;
    unsigned long rtt_min = std::numeric_limits<unsigned long>::max();
    unsigned long rtt_max = 0;

    while (!end && (std::chrono::system_clock::now() - begin) < duration)
    {
        serialPort.write_some(asio::buffer(buffer, payload_size));
        auto sent_time = std::chrono::high_resolution_clock::now();
        std::cout << "Sent : " << payload_size << " Byte(s) ... ";
        std::cout.flush();
        nb_sent++;

        int n = readWithTimeout(serialPort,recvBuffer,boost::posix_time::seconds(2),error);
        auto recv_time = std::chrono::high_resolution_clock::now();
        if (error)
        {
            std::cerr << error << std::endl;
            exit(EXIT_FAILURE);
        }
        if (n)
        {
            unsigned long rtt = std::chrono::duration_cast<std::chrono::microseconds>(recv_time-sent_time).count();
            rtt_sum+=rtt;
            if (rtt>rtt_max)
                rtt_max=rtt;
            if (rtt<rtt_min)
                rtt_min=rtt;
            std::cout << "Received : " << n << " Byte(s) (RTT " << rtt/1000.0 << " ms)\n";
            nb_recv++;
        }
        else
        {
            std::cout << "Timeout waiting reply !\n";
        }
    }

    std::cout << "Sent " << nb_sent << " / Received " << nb_recv << " (" << (100.0*nb_recv)/nb_sent << "%)" << std::endl;
    std::cout << "RTT (min/av/max) = " << rtt_min/1000.0 << " / " << (rtt_sum/nb_recv)/1000.0 << " / " << rtt_max/1000.0 << " ms" << std::endl;
    serialPort.close();
    ioService.stop();

    return EXIT_SUCCESS;
}
