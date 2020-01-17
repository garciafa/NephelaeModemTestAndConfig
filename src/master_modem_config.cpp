#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <regex>

namespace asio = boost::asio;
const std::string okString("OK");
const std::string errorString("ERROR");
const std::string endOfLinestring("\r\n");

class modem_error : public std::runtime_error {
public:
    modem_error(std::string const &what):std::runtime_error(what) {};
};

std::string sendCommand (asio::serial_port& serialPort ,std::string command)
{
    command += endOfLinestring;

    serialPort.write_some(asio::buffer(command.c_str(), command.length()));

    static std::string stringBuffer;
    boost::asio::streambuf b;
    std::regex re("(?:" + okString + "|" + errorString + ")" + endOfLinestring);

    std::string result;

    boost::system::error_code error;
    int i = 0;
    bool got_answer = false;
    do
    {
        auto n = asio::read_until(serialPort, b, endOfLinestring, error);
        if (error)
        {
            std::cerr << error << std::endl;
            exit(EXIT_FAILURE);
        }

        std::istream is(&b);
        is >> stringBuffer;

        result += stringBuffer.substr(0, n);
        got_answer = std::regex_match(stringBuffer.substr(0, n), re);
        stringBuffer.erase(0, n);
    } while (!got_answer);

    if (result.find(okString + endOfLinestring) != std::string::npos)
    {
        if (result.find(command) != std::string::npos)
        {
            result = result.substr(result.find(command) + command.length());
        }
        result = result.substr(0, result.find(okString));
    }
    else
    {
        result = result.substr(0, result.find(errorString));
        throw modem_error(std::string("\n")+result);
    }

    return result;
}

int main (int argc, char ** argv)
{
    if (argc!=3)
    {
        std::cerr << "usage :" << argv[0] << " <device file (e.g.: /dev/ttyUSB0)> <ATS109 parameter>\n";
        exit (EXIT_FAILURE);
    }
    uint16_t ATS109;
    {
        std::stringstream sstr(argv[2]);
        sstr >> ATS109;
        if (sstr.fail() || ATS109 < 0 || ATS109 > 61)
        {
            std::cerr << "Second argument must be an integer between 0 and 61.\n";
            exit(EXIT_FAILURE);
        }
    }

    asio::io_service ioService;
    asio::serial_port serialPort(ioService,argv[1]);
    if (!serialPort.is_open())
    {
        std::cerr << "Could not open serial port " << argv[1] << std::endl;
        exit(EXIT_FAILURE);
    }
    boost::system::error_code error;
    serialPort.set_option(boost::asio::serial_port_base::baud_rate(9600),error);
    if (!error)
    {
        std::cout << "Set speed to 9600 bauds on " << argv[1] << std::endl;
    }
    else
    {
        std::cerr << "Could not set speed to 9600 bauds on " << argv[1] << std::endl;
        exit(EXIT_FAILURE);
    }
    // Set 8N1 mode
    serialPort.set_option(asio::serial_port_base::character_size(8));
    serialPort.set_option(asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serialPort.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
    // No flow control
    serialPort.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

    // Set mode of operation => Master
    std::string result = sendCommand(serialPort,"ATS101=0");

    // Set serial baud rate => 57,6kbps
    result = sendCommand(serialPort,"ATS102=2");

    // Set wireless link rate => 57,6kbps
    result = sendCommand(serialPort,"ATS103=8");

    // Set network address => 1234567890
    result = sendCommand(serialPort,"ATS104=1234567890");

    // Set output power => 1W
    result = sendCommand(serialPort,"ATS105=1");

    // Set output power => 1W
    result = sendCommand(serialPort,"ATS108=30");

    // Set hop interval
    std::stringstream sstr;
    sstr << "ATS109=" << ATS109;
    result = sendCommand(serialPort,sstr.str());

    // Set Data format => 8N1
    result = sendCommand(serialPort,"ATS110=1");

    // Set maximum packet size => 255 bytes
    result = sendCommand(serialPort,"ATS112=255");

    // Set packet retransmission => none
    result = sendCommand(serialPort,"ATS113=0");

    // Set network type => Point to multipoint
    result = sendCommand(serialPort,"ATS133=0");

    // Set destination address => 65535 (bcast)
    result = sendCommand(serialPort,"ATS140=65535");

    // Set FEC => reed-solomon (15,11)
    result = sendCommand(serialPort,"ATS158=7");

    // Set Protocol type => transparent
    result = sendCommand(serialPort,"ATS217=0");

    result = sendCommand(serialPort,"AT&V");
    std::cout << "Modem configuration:\n\"" << result << "\"" << std::endl;

    std::cout << "Write this configuration (y/N) ?";
    std::cout.flush();
    std::string line;
    std::getline(std::cin,line);
    if (line=="y")
    {
        std::cout << "Writing configuration to modem memory." << std::endl;
        result = sendCommand(serialPort,"AT&W");
    }

    serialPort.close();

    return EXIT_SUCCESS;
}
