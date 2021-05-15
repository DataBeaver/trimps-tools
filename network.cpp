#define _WIN32_WINNT _WIN32_WINNT_VISTA
#include "network.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <future>
#include "http.h"
#include "stringutils.h"

using namespace std;

#ifdef _WIN32
typedef SOCKADDR_STORAGE sockaddr_storage;

int socket_pair(int sv[2])
{
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	int addrlen = sizeof(addr);

	int ls = socket(AF_INET, SOCK_STREAM, 0);
	::bind(ls, reinterpret_cast<sockaddr *>(&addr), addrlen);
	getsockname(ls, reinterpret_cast<sockaddr *>(&addr), &addrlen);
	listen(ls, 1);

	sv[0] = socket(AF_INET, SOCK_STREAM, 0);
	connect(sv[0], reinterpret_cast<sockaddr *>(&addr), addrlen);

	sv[1] = accept(ls, 0, 0);
	closesocket(ls);

	return 0;
}
#endif

#ifndef _WIN32
void closesocket(int s)
{
	close(s);
}

int socket_pair(int sv[2])
{
	return socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
}
#endif

Network::Network():
	listen_sock(-1),
	serve_func(0),
	next_tag(1),
	worker(0)
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(0x0202, &wsa_data);
#endif
}

Network::~Network()
{
	delete serve_func;
#ifdef _WIN32
	WSACleanup();
#endif
}

void Network::serve_(uint16_t port)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0)
		throw runtime_error("Network::serve (socket)");

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = ntohs(port);
	int err = ::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
	if(err<0)
	{
		closesocket(sock);
		throw runtime_error("Network::serve (bind)");
	}

	err = listen(sock, 4);
	if(err<0)
	{
		closesocket(sock);
		throw runtime_error("Network::serve (listen)");
	}

	listen_sock = sock;
	ensure_worker();
}

Network::ConnectionTag Network::connect(const string &host, uint16_t port)
{
	return connect_(host, port, 0);
}

Network::ConnectionTag Network::connect_(const string &host, uint16_t port, ReceiveFunc *func)
{
	addrinfo hints = { 0, AF_INET, SOCK_STREAM, 0, 0, 0, 0, 0 };
	addrinfo *ai = 0;
	int err = getaddrinfo(host.c_str(), 0, &hints, &ai);
	if(err!=0)
		throw runtime_error(format("Network::connect (getaddrinfo, %s)", gai_strerror(err)));
	if(!ai)
		throw runtime_error("Network::connect (unexpected null address)");

	if(ai->ai_addr->sa_family!=AF_INET)
	{
		freeaddrinfo(ai);
		throw runtime_error("Network::connect (address family mismatch)");
	}

	sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(ai->ai_addr);
	addr->sin_port = htons(port);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	err = ::connect(sock, reinterpret_cast<sockaddr *>(addr), ai->ai_addrlen);
	if(err<0)
	{
		closesocket(sock);
		freeaddrinfo(ai);
		throw runtime_error("Network::connect (connect)");
	}

	ConnectionTag tag = add_connection(sock, host, func);

	freeaddrinfo(ai);
	ensure_worker();

	return tag;
}

const string &Network::get_remote_host(ConnectionTag tag) const
{
	lock_guard<mutex> lock(connections_mutex);
	auto i = connections.find(tag);
	static string empty;
	return (i!=connections.end() ? i->second->remote_host : empty);
}

void Network::send_message(ConnectionTag tag, const string &data)
{
	communicate_(tag, data, 0);
}

string Network::communicate(ConnectionTag tag, const string &data)
{
	promise<string> pr;
	communicate(tag, data, [&pr](ConnectionTag, const string &d){ pr.set_value(d); });
	future<string> fut = pr.get_future();
	fut.wait();
	return fut.get();
}

void Network::communicate_(ConnectionTag tag, const string &data, ReceiveFunc *func)
{
	lock_guard<mutex> lock(connections_mutex);
	auto i = connections.find(tag);
	if(i==connections.end())
	{
		delete func;
		return;
	}

	string msg_data = data;
	if(i->second->http_mode==0)
	{
		if(data.find('\n')!=string::npos)
		{
			delete func;
			throw invalid_argument("Network::communicate");
		}

		msg_data += '\n';
	}

	if(i->second->next_recv!=i->second->recv_func)
	{
		i->second->message_queue.emplace_back(tag, msg_data, func);
		return;
	}

	send(i->second->sock, msg_data.data(), msg_data.size(), 0);
	if(func)
		i->second->next_recv = func;
}

void Network::ensure_worker()
{
	if(!worker)
		worker = new Worker(*this);
	else
		worker->wake();
}

Network::ConnectionTag Network::add_connection(int sock, const string &host, ReceiveFunc *func)
{
	lock_guard<mutex> lock(connections_mutex);
	ConnectionTag tag = next_tag++;
	connections[tag] = new Connection(tag, sock, host, func);
	return tag;
}


Network::Message::Message(ConnectionTag c, const string &d, ReceiveFunc *f):
	connection(c),
	data(d),
	recv_func(f)
{ }


Network::Connection::Connection(ConnectionTag t, int s, const string &h, ReceiveFunc *f):
	tag(t),
	sock(s),
	http_mode(-1),
	remote_host(h),
	recv_func(f),
	next_recv(f)
{ }


Network::Worker::Worker(Network &n):
	network(n),
	wake_sock{ -1, -1 },
	thread(&Worker::main, this)
{ }

void Network::Worker::wake()
{
	char c = 0;
	send(wake_sock[1], &c, 1, 0);
}

void Network::Worker::main()
{
	socket_pair(wake_sock);

	fd_set fds;
	while(1)
	{
		send_messages();

		FD_ZERO(&fds);
		FD_SET(wake_sock[0], &fds);
		int max_fd = wake_sock[0]+1;
		if(network.listen_sock>=0)
		{
			FD_SET(network.listen_sock, &fds);
			max_fd = network.listen_sock;
		}

		{
			lock_guard<mutex> lock(network.connections_mutex);
			for(const auto &c: network.connections)
			{
				FD_SET(c.second->sock, &fds);
				max_fd = max(max_fd, c.second->sock);
			}
		}

		int res = select(max_fd+1, &fds, 0, 0, 0);
		if(res>0)
		{
			if(FD_ISSET(wake_sock[0], &fds))
			{
				char buf[16];
				recv(wake_sock[0], buf, sizeof(buf), 0);
			}

			if(network.listen_sock>=0 && FD_ISSET(network.listen_sock, &fds))
				accept_connection();

			lock_guard<mutex> lock(network.connections_mutex);
			for(const auto &c: network.connections)
				if(FD_ISSET(c.second->sock, &fds))
					process_connection(c.second);

			for(auto c: stale_connections)
				network.connections.erase(c->tag);
		}

		for(const auto &m: receive_queue)
		{
			try
			{
				m.recv_func->receive(m.connection, m.data);
			}
			catch(const exception &)
			{ }

			if(m.recv_func->is_one_shot())
				delete m.recv_func;
		}
		receive_queue.clear();

		for(auto c: stale_connections)
		{
			try
			{
				c->next_recv->receive(c->tag, string());
			}
			catch(const exception &)
			{ }
			if(c->next_recv!=c->recv_func)
				delete c->next_recv;
			if(c->recv_func!=network.serve_func)
				delete c->recv_func;
			closesocket(c->sock);
			delete c;
		}
		stale_connections.clear();
	}
}

void Network::Worker::send_messages()
{
	lock_guard<mutex> lock(network.connections_mutex);
	for(const auto &c: network.connections)
		while(c.second->next_recv==c.second->recv_func && !c.second->message_queue.empty())
		{
			const Message &m = c.second->message_queue.front();
			send(c.second->sock, m.data.data(), m.data.size(), 0);
			if(m.recv_func)
				c.second->next_recv = m.recv_func;
			c.second->message_queue.pop_front();
		}
}

void Network::Worker::accept_connection()
{
	sockaddr_storage addr;
	socklen_t addr_len = sizeof(addr);
	int sock = accept(network.listen_sock, reinterpret_cast<sockaddr *>(&addr), &addr_len);

	string host;
	if(addr.ss_family==AF_INET)
	{
		char host_buf[INET6_ADDRSTRLEN];
		const char *host_ptr = inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(&addr)->sin_addr, host_buf, sizeof(host_buf));
		if(host_ptr)
			host = host_ptr;
	}
	if(sock>=0)
		network.add_connection(sock, host, network.serve_func);
}

void Network::Worker::process_connection(Connection *conn)
{
	char buf[16384];
	int res = recv(conn->sock, buf, sizeof(buf), 0);

	if(res>0)
		conn->received_data.append(buf, res);
	else
		stale_connections.push_back(conn);

	if(conn->http_mode<0)
		if(!conn->received_data.compare(0, 4, "GET ") || !conn->received_data.compare(0, 5, "POST "))
			conn->http_mode = 1;

	string::size_type start = 0;
	while(1)
	{
		string::size_type newline = conn->received_data.find('\n', start);
		if(newline==string::npos)
			break;

		if(conn->http_mode<0)
			conn->http_mode = 0;

		string::size_type message_end = newline;
		if(conn->next_recv && newline>0)
		{
			if(conn->http_mode)
			{
				string::size_type headers_end = conn->received_data.find("\r\n\r\n");
				if(headers_end==string::npos)
				{
					headers_end = conn->received_data.find("\n\n");
					if(headers_end==string::npos)
						break;
					else
						headers_end += 2;
				}
				else
					headers_end += 4;

				HttpMessage message(conn->received_data.substr(start, headers_end-start));

				string::size_type content_length = 0;
				auto i = message.headers.find("Content-Length");
				if(i!=message.headers.end())
					content_length = parse_value<string::size_type>(i->second);

				if(headers_end+content_length>conn->received_data.size())
					break;

				message_end = headers_end+content_length;
			}

			receive_queue.emplace_back(conn->tag, conn->received_data.substr(start, message_end-start), conn->next_recv);
			conn->next_recv = conn->recv_func;
		}

		start = message_end+1;
	}

	conn->received_data.erase(0, start);
}
