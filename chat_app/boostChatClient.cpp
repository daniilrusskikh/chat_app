#include<iostream>
#include<queue>
#include<string>
#include<cstdlib>

#include<boost/thread.hpp>
#include<boost/bind.hpp>
#include<boost/asio.hpp>
#include<boost/asio/ip/tcp.hpp>
#include<boost/algorithm/string.hpp>

//using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

typedef boost::shared_ptr<tcp::socket> socket_ptr;
typedef boost::shared_ptr<std::string> string_ptr;
typedef boost::shared_ptr< std::queue<string_ptr> > messageQueue_ptr;

io_service service; // Boost Asio io_service
messageQueue_ptr messageQueue(new std::queue<string_ptr>); // Queue for producer consumer pattern
tcp::endpoint ep(ip::address::from_string("127.0.0.1"), 8001); // TCP socket for connecting to server
const int inputSize = 256; // Maximum size for input buffer
string_ptr promptCpy; // Terminal prompt displayed to chat users

// Function Prototypes
bool isOwnMessage(string_ptr); // Check, is it own message?
void displayLoop(socket_ptr); // Removes items from messageQueue to display on the client terminal; i.e consumer
void inboundLoop(socket_ptr, string_ptr); // Will push items from the socket to our messageQueue; i.e producer
void writeLoop(socket_ptr, string_ptr);
std::string* buildPrompt();
// End of Function Prototypes

int main(int argc, char** argv)
{
	try
	{
		boost::thread_group threads;
		socket_ptr sock(new tcp::socket(service));

		string_ptr prompt(buildPrompt());
		promptCpy = prompt;

		sock->connect(ep);

		std::cout << "Welcome to the ChatApplication\nType \"exit\" to quit" << std::endl;

		threads.create_thread(boost::bind(displayLoop, sock));
		threads.create_thread(boost::bind(inboundLoop, sock, prompt));
		threads.create_thread(boost::bind(writeLoop, sock, prompt));

		threads.join_all();
	}
	catch (boost::system::system_error& e)
	{
		std::cerr << "EXCEPTION: " << e.code() << std::endl;
	}

	puts("Press any key to continue...");
	getc(stdin);
	return EXIT_SUCCESS;
}

std::string* buildPrompt()
{
	const int inputSize = 256;
	char inputBuf[inputSize] = { 0 };
	char nameBuf[inputSize] = { 0 };
	std::string* prompt = new std::string(": ");

	std::cout << "Please input a new username: ";
	std::cin.getline(nameBuf, inputSize);
	*prompt = (std::string)nameBuf + *prompt;
	boost::algorithm::to_lower(*prompt);

	return prompt;
}

void inboundLoop(socket_ptr sock, string_ptr prompt)
{
	int bytesRead = 0;
	char readBuf[1024] = { 0 };

	for (;;)
	{
		if (sock->available())
		{
			bytesRead = sock->read_some(buffer(readBuf, inputSize));
			string_ptr msg(new std::string(readBuf, bytesRead));

			messageQueue->push(msg);
		}

		/*
		Reading from the socket object is an operation
		which may potentially interfere with writing to the socket
		so we put a one second delay on checks for reading.
		*/
		boost::this_thread::sleep(boost::posix_time::millisec(1000)); 
	}
}

void writeLoop(socket_ptr sock, string_ptr prompt)
{
	char inputBuf[inputSize] = { 0 };
	std::string inputMsg;

	for (;;)
	{
		std::cin.getline(inputBuf, inputSize);
		inputMsg = *prompt + (std::string)inputBuf + '\n';

		if (!inputMsg.empty())
		{
			sock->write_some(buffer(inputMsg, inputSize));
		}

		// The string for quitting the application
		// On the server-side there is also a check for "quit" to terminate the TCP socket
		if (inputMsg.find("exit") != std::string::npos)
			exit(1); // Replace with cleanup code if you want but for this tutorial exit is enough

		inputMsg.clear();
		memset(inputBuf, 0, inputSize);
	}
}

void displayLoop(socket_ptr sock)
{
	for (;;)
	{
		if (!messageQueue->empty())
		{
			// Can you refactor this code to handle multiple users with the same prompt?
			if (!isOwnMessage(messageQueue->front()))
			{
				std::cout << "\n" + *(messageQueue->front());
			}
			messageQueue->pop();
		}
		boost::this_thread::sleep(boost::posix_time::millisec(1000));
	}
}

bool isOwnMessage(string_ptr message)
{
	if (message->find(*promptCpy) == std::string::npos)
		return false;
	else
		return true;
}