#ifndef NETWORK_H_
#define NETWORK_H_

#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Network
{
public:
	typedef unsigned ConnectionTag;

private:
	class ReceiveFunc
	{
	protected:
		ReceiveFunc() { }
	public:
		virtual ~ReceiveFunc() { }

		virtual void receive(unsigned, const std::string &) = 0;
	};

	template<typename F>
	class TypedReceiveFunc: public ReceiveFunc
	{
	private:
		F func;

	public:
		TypedReceiveFunc(const F &f): func(f) { }

		virtual void receive(unsigned t, const std::string &m) { func(t, m); }
	};

	struct Message
	{
		ConnectionTag connection;
		std::string data;
		ReceiveFunc *recv_func;

		Message(ConnectionTag, const std::string &, ReceiveFunc *);
	};

	struct Connection
	{
		ConnectionTag tag;
		int sock;
		std::string remote_host;
		std::string received_data;
		ReceiveFunc *recv_func;
		std::list<Message> message_queue;

		Connection(ConnectionTag, int, const std::string &);
	};

	class Worker
	{
	private:
		Network &network;
		std::thread thread;
		std::list<Message> receive_queue;
		std::vector<Connection *> stale_connections;

	public:
		Worker(Network &);

	private:
		void main();
		void send_messages();
		void accept_connection();
		void process_connection(Connection *);
	};

	int listen_sock;
	ReceiveFunc *serve_func;
	std::map<ConnectionTag, Connection *> connections;
	ConnectionTag next_tag;
	mutable std::mutex connections_mutex;
	Worker *worker;

public:
	Network();
	~Network();

	template<typename F>
	void serve(std::uint16_t, const F &);
private:
	void serve_(std::uint16_t);

public:
	ConnectionTag connect(const std::string &, std::uint16_t);

	const std::string &get_remote_host(ConnectionTag) const;

	void send_message(ConnectionTag, const std::string &);
	std::string communicate(ConnectionTag, const std::string &);

	template<typename F>
	void communicate(ConnectionTag, const std::string &, const F &);
private:
	void communicate_(ConnectionTag, const std::string &, ReceiveFunc *);

	void ensure_worker();
	ConnectionTag add_connection(int, const std::string &);
};

template<typename F>
inline void Network::serve(std::uint16_t port, const F &func)
{
	if(listen_sock>=0)
		throw std::logic_error("Network::serve");

	serve_func = new TypedReceiveFunc<F>(func);
	serve_(port);
}

template<typename F>
inline void Network::communicate(ConnectionTag tag, const std::string &data, const F &func)
{
	communicate_(tag, data, new TypedReceiveFunc<F>(func));
}

#endif
