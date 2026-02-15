#include <iostream>
#include <system_error>
#include <fstream>
#include <memory>
#include <string>
#include <future>
#include <unordered_map>
#include <vector>
#include <thread>
#include <algorithm>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <iconv.h>
#include <thread>
#include <sstream>

#include <boost/lockfree/queue.hpp>
#include <boost/property_tree/detail/rapidxml.hpp>
#include <boost/property_tree/detail/xml_parser_read_rapidxml.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/config.hpp>

#include "connection_manager.hpp"
#include "epoll.hpp"
#include "async_operations.hpp"
#include "service_function.hpp"

std::string win1251_to_utf8_impl(const std::string& input) {
    iconv_t cd = iconv_open("UTF-8", "CP1251");
    if (cd == (iconv_t)-1) return input;
    
    char* in_ptr = const_cast<char*>(input.data());
    size_t in_bytes = input.size();
    
    std::string output(input.size() * 2, '\0');
    char* out_ptr = &output[0];
    size_t out_bytes = output.size();
    
    iconv(cd, &in_ptr, &in_bytes, &out_ptr, &out_bytes);
    output.resize(output.size() - out_bytes);
    iconv_close(cd);
    return output;
}

std::string get_ip_from_string(const std::string &domain)
{
    struct addrinfo hints, *result, *p;
    int status;
    std::array<char, INET6_ADDRSTRLEN> ip_string;
    std::fill(ip_string.begin(), ip_string.end(), '!');

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(domain.c_str(), NULL, &hints, &result);
    if (status != 0)
    {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return std::string();
    }

    for (p = result; p != NULL; p = p->ai_next)
    {
        void *addr;
        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        }

        if (inet_ntop(p->ai_family, addr, ip_string.data(), ip_string.size()) == NULL)
        {
            std::cerr << "inet_ntop error" << std::endl;
            continue;
        }
    }

    freeaddrinfo(result);
    const auto count = std::count_if(ip_string.cbegin(), ip_string.cend(), [](const char &element)
                                     { return element != '!'; });

    return std::string(ip_string.data(), count);
};

static const std::string URL = "www.cbr.ru";
static const std::string IP_ADDRES = get_ip_from_string(URL);
static const int PORT = 80;
static const std::string QUERY = "/scripts/XML_daily.asp?date_req=06/11/2025";

struct Valute
{
    std::string ID;
    int NumCode;
    std::string CharCode;
    int Nominal;
    std::string Name;
    float Value;
    float VunitRate;
};

using promise_type = std::promise<std::unordered_map<std::string, Valute>>;
// using promise_type = std::promise<std::string>;
using promsie_pointer = std::shared_ptr<promise_type>;

void getExchangeRates(promise_type &&promise)
{
    static IOContext context;
    EndpointIPv4 EndpointIPv4(IP_ADDRES, PORT);
    TCPAsyncSocket socket(context);

    std::vector<char> bufferArray(30'000);
    std::fill(bufferArray.begin(), bufferArray.end(), '!');
    std::string source;

    socket.async_connect(EndpointIPv4, [&socket, &bufferArray, &source, &promise](const std::error_code &error)
                         {
                            ConsoleLogger::getLogger().loggingMessage(LogLevel::INFO, "async_connect_handler start()");
        if(!error)
        {
            std::string GET = "";
	        GET += "GET " + QUERY + " HTTP/1.1\r\n";
	        GET += "Accept-language: ru, en\r\n";
	        GET += "Cache-Control: no - cache\r\n";
	        GET += "Content-Type: application/xml\r\n";
	        GET += "Host: " + URL + "\r\n";
	        GET += "Connection: close\r\n\r\n";

            std::vector<char> bufferWrite(GET.cbegin(), GET.cend());

            socket.async_write(bufferWrite, [&socket, &bufferArray, &source, &promise](const std::error_code& error, size_t butesTranfered)
            {
                ConsoleLogger::getLogger().loggingMessage(LogLevel::INFO, "async_write_handler start");   
                if(!error)             
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    socket.async_read(bufferArray, [&bufferArray, &source, &promise](const std::error_code& error, size_t bytesRead)
                    {
                        ConsoleLogger::getLogger().loggingMessage(LogLevel::INFO, "async_read_handler start");
                        if(!error)
                        {
                            ConsoleLogger::getLogger().loggingMessage(LogLevel::INFO, " error != fail");
                            int count = std::count_if(bufferArray.cbegin(), bufferArray.cend(), [](const char& element)
                            {
                                return element != '!';
                            });                            
                            source = std::string(bufferArray.data(), count);

                            std::vector<std::string> strings;
                            std::unordered_map<std::string, Valute> fieldValutes;
                            std::string xmlText;
                            //std::cout << "source = " << source << std::endl;
                            //promise.set_value(source);  

                            int startIndex{0};
			                int endIndex{0};
			                int currentIndex{0};
			                count = 0;

                            while (currentIndex < source.size())
			                {
				               startIndex = currentIndex;
				               while (source[currentIndex] != '\r' && source[currentIndex + 1] != '\n' &&
					           currentIndex + 1 < source.size())
				               {
					               ++currentIndex;
				               }
				               endIndex = currentIndex;

				            const auto strLength = endIndex - startIndex + 1;
				            strings.push_back(std::string(&source[startIndex], strLength));

				            currentIndex += 2;
			            }

			            auto is_empty = [](const std::string &line) -> bool
			{
				auto isEmpty = true;
				for (int idx = 0; idx < line.size(); ++idx)
				{
					if (line[idx] != ' ' && line[idx] != '\r' && line[idx] != '\n')
					{
						isEmpty = false;
					}
				}
				return isEmpty;
			};

			            for (int idx = 0; idx < strings.size(); ++idx)
			{
				if (is_empty(strings[idx]))
				{
					xmlText = strings[idx + 1];
					break;
				}
			}

			            auto clear_text = [](const std::string &text) -> std::string
			{
				std::vector<char> buffer;
				for (int idx = 0; idx < text.size(); ++idx)
				{
					if (text[idx] == 1 && text[idx + 1] == 1 && text[idx + 2] == 1)
					{
						break;
					}
					buffer.push_back(text[idx]);
				}
				buffer.push_back('\0');
				buffer.shrink_to_fit();
				return std::string(buffer.data(), buffer.size());
			};

			            xmlText = clear_text(xmlText);

			            using namespace boost::property_tree::detail::rapidxml;
			            xml_document<> doc;
			            xml_node<> *root_node;
			            std::vector<char> buffer(xmlText.begin(), xmlText.end());
			            buffer.shrink_to_fit();
			            doc.parse<0>(&buffer[0]);
			            root_node = doc.first_node("ValCurs");

			            using pointer = xml_node<> *;

			            Valute valute;

			            for (pointer valute_node = root_node->first_node("Valute"); valute_node; valute_node = valute_node->next_sibling())
			{
				valute.ID = std::string(valute_node->first_attribute("ID")->value(), valute_node->first_attribute("ID")->value_size());

				pointer num_code = valute_node->first_node("NumCode");
				valute.NumCode = std::stoi(std::string(num_code->value(), num_code->value_size()));

				pointer char_node = valute_node->first_node("CharCode");
				valute.CharCode = std::string(char_node->value(), char_node->value_size());

				pointer nominal_node = valute_node->first_node("Nominal");
				valute.Nominal = std::stoi(std::string(nominal_node->value(), nominal_node->value_size()));

				pointer name_node = valute_node->first_node("Name");
				valute.Name = std::string(name_node->value(), name_node->value_size());

				pointer value_node = valute_node->first_node("Value");
				valute.Value = std::stof(std::string(value_node->value(), value_node->value_size()));

				pointer vunit_rate_node = valute_node->first_node("VunitRate");
				valute.VunitRate = std::stof(std::string(vunit_rate_node->value(), vunit_rate_node->value_size()));

				fieldValutes.insert(std::pair<std::string, Valute>(valute.CharCode, valute));
			}

                        promise.set_value(fieldValutes);

                        context.stop();                          
                        ConsoleLogger::getLogger().loggingMessage(LogLevel::INFO, " End programm");

                        }else
                        {
                            ConsoleLogger::getLogger().loggingMessage(LogLevel::ERROR, "async_read failed " + error.message());
                        }
                    });
                }else
                {
                    ConsoleLogger::getLogger().loggingMessage(LogLevel::ERROR, "async_write failed " + error.message());
                }
            });
        }else
        {
            ConsoleLogger::getLogger().loggingMessage(LogLevel::ERROR, " async_connect failed " + error.message());
        } });    

    std::vector<std::thread> threads;
    for(int idx = 0; idx < 3; ++idx)
    {
        threads.emplace_back([]{context.run();});
    }

    for(auto& thread : threads)
    {
        thread.join();
    }
};

int main(int, char **)
{
    //std::cout << "Hello world\n";
    std::promise<std::unordered_map<std::string, Valute>> promise;
    auto future = promise.get_future();
   
    getExchangeRates(std::move(promise));   

    while(future.wait_for(std::chrono::nanoseconds(0)) != std::future_status::ready)
    {
        std::cout << "Sleep" << std::endl;
        std::this_thread::sleep_for(std::chrono::nanoseconds(1'000'000));
    }

    const auto result = future.get();

    std::string realName;

    for (const auto &iter : result)
    {
        //std::cout << iter.second.CharCode << " = " << iter.second.Value << std::endl;
        realName = win1251_to_utf8_impl(iter.second.Name);
        std::cout << iter.second.Nominal << " " << realName << " = " << iter.second.Value << " Рублей" << std::endl;
    }  
    
}
