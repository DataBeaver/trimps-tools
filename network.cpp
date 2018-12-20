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
#include "network.h"

using namespace std;

#ifdef _WIN32
typedef SOCKADDR_STORAGE sockaddr_storage;
#endif

#ifndef _WIN32
void closesocket(int s)
{
	close(s);
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
	int err = bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
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
	addrinfo hints;
	addrinfo *ai;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;
	int err = getaddrinfo(host.c_str(), 0, &hints, &ai);
	if(err<0)
		throw runtime_error("Network::connect (getaddrinfo)");

	if(ai->ai_addr->sa_family!=AF_INET)
	{
		freeaddrinfo(ai);
		throw runtime_error("Network::connect");
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

	ConnectionTag tag = add_connection(sock, host);

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
	if(data.find('\n')!=string::npos)
	{
		delete func;
		throw invalid_argument("Network::communicate");
	}

	string msg_data = data+"\n";

	lock_guard<mutex> lock(connections_mutex);
	auto i = connections.find(tag);
	if(i==connections.end())
		return;

	if(i->second->recv_func)
	{
		i->second->message_queue.emplace_back(tag, msg_data, func);
		return;
	}

	send(i->second->sock, msg_data.data(), msg_data.size(), 0);
	i->second->recv_func = func;
}

void Network::ensure_worker()
{
	if(!worker)
		worker = new Worker(*this);
}

Network::ConnectionTag Network::add_connection(int sock, const string &host)
{
	lock_guard<mutex> lock(connections_mutex);
	ConnectionTag tag = next_tag++;
	connections[tag] = new Connection(tag, sock, host);
	return tag;
}


Network::Message::Message(ConnectionTag c, const string &d, ReceiveFunc *f):
	connection(c),
	data(d),
	recv_func(f)
{ }


Network::Connection::Connection(ConnectionTag t, int s, const string &h):
	tag(t),
	sock(s),
	remote_host(h),
	recv_func(0)
{ }


Network::Worker::Worker(Network &n):
	network(n),
	thread(&Worker::main, this)
{ }

void Network::Worker::main()
{
	fd_set fds;
	while(1)
	{
		send_messages();

		int max_fd = 0;
		FD_ZERO(&fds);
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
			if(FD_ISSET(network.listen_sock, &fds))
				accept_connection();

			lock_guard<mutex> lock(network.connections_mutex);
			for(const auto &c: network.connections)
				if(FD_ISSET(c.second->sock, &fds))
					process_connection(c.second);

			for(auto c: stale_connections)
			{
				network.connections.erase(c->tag);
				delete c;
			}
			stale_connections.clear();
		}

		for(const auto &m: receive_queue)
		{
			m.recv_func->receive(m.connection, m.data);
			if(m.recv_func!=network.serve_func)
				delete m.recv_func;
		}
		receive_queue.clear();
	}
}

void Network::Worker::send_messages()
{
	lock_guard<mutex> lock(network.connections_mutex);
	for(const auto &c: network.connections)
		while(!c.second->recv_func && !c.second->message_queue.empty())
		{
			const Message &m = c.second->message_queue.front();
			send(c.second->sock, m.data.data(), m.data.size(), 0);
			c.second->recv_func = m.recv_func;
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
		host = inet_ntoa(reinterpret_cast<const sockaddr_in *>(&addr)->sin_addr);
	if(sock>=0)
		network.add_connection(sock, host);
}

void Network::Worker::process_connection(Connection *conn)
{
	char buf[16384];
	int res = recv(conn->sock, buf, sizeof(buf), 0);
	if(res<=0)
	{
		if(conn->recv_func)
			receive_queue.emplace_back(conn->tag, string(), conn->recv_func);

		closesocket(conn->sock);
		stale_connections.push_back(conn);

		return;
	}

	conn->received_data.append(buf, res);
	while(1)
	{
		string::size_type newline = conn->received_data.find('\n');
		if(newline==string::npos)
			break;

		ReceiveFunc *func = (conn->recv_func ? conn->recv_func : network.serve_func);
		if(func)
			receive_queue.emplace_back(conn->tag, conn->received_data.substr(0, newline), func);
		conn->received_data.erase(0, newline+1);
		conn->recv_func = 0;
	}
}
