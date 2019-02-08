// Barbershop.cpp
// by Tianchan Xu
// 1/25/2019 
#include <mutex>
#include <queue>
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <atomic>
#include <vector>
#include <stdio.h> //getchar

// Amount of chairs for waiting customers
int nrChairs;
std::atomic<int> emptyChairs;

// thread-safe flag used to break while-loops
std::atomic<bool> a(1);

// shared print function 
std::mutex mtx;          
void shared_print(const std::string& s) {
	while (true) {
		// try to lock mtx 
		if (mtx.try_lock()) {
			std::cout << s << std::endl;
			// if commenting out unlock(), thread will never end.
			mtx.unlock();
			return;
		}
	}
}

std::condition_variable m_cond2;
std::mutex m_mutex2;

// Queue class that has thread synchronisation
template <typename T>
class SynchronisedQueue {
private:
	std::queue<T> m_queue;   // Use stl queue to store data
	std::mutex m_mutex;      // The mutex to synchronise on
	std::condition_variable m_cond; // The condition to wait for

public:
	// Add data to the queue and notify others
	void Enqueue(const T& data)
	{
		// Acquire lock on the queue
		std::unique_lock<std::mutex> lock(m_mutex);

		// Add the data to the queue
		m_queue.push(data);

		// Notify others that data is ready
		m_cond.notify_one();
	} // Lock is released here

	// Get data from the queue. Wait for data if not available
	T Dequeue() {
		// Acquire lock on the queue
		std::unique_lock<std::mutex> lock(m_mutex);

		// When there is no data, wait till someone fills it or wait for 4 seconds to solve the deadlock problem.
		// Lock is automatically released in the wait and active agian after the wait.
		while (m_queue.size() == 0) {
			m_cond.wait_for(lock, std::chrono::seconds(4));
			if (a == 0 and m_queue.size() == 0) {
				std::cout << "thread finished waiting!" << std::endl;
				return 0;
			}
			else {
				// Retrieve the data from the queue
				T result = m_queue.front();
				m_queue.pop();
				return result;
			}
		}

		// Retrieve the data from the queue
		T result = m_queue.front();
		m_queue.pop();
		return result;
	} // Lock is released here
};

// Class that produces objects and puts them in a queue
class Customer {
private:
	int m_id;                                  // The id of the producer
	std::shared_ptr<SynchronisedQueue<std::string>> m_queue;   // The queue to use
public:
	// Constructor with id and the queue to use
	Customer(int id, std::shared_ptr<SynchronisedQueue<std::string>> queue) {
		m_id = id;
		m_queue = queue;
	}
	// The thread function fills the queue with data
	void operator()() {
		// Produce a string and store in the queue
		std::string str = "Customer: " + std::to_string(m_id);
		shared_print(str+" arrived!");
		if (emptyChairs == 0)
			shared_print(str + " left, since no chair available!");
		else if (emptyChairs == nrChairs) {
			emptyChairs--;
			m_queue->Enqueue(str);
			m_cond2.notify_one();
		}
		else {
			emptyChairs--;
			m_queue->Enqueue(str);
		}
	}
};

// Class that consumer objects from a queue
class Barber {
private:
	int m_id;                                 // The id of the consumer       
	std::shared_ptr<SynchronisedQueue<std::string>> m_queue;  // the queue to use

public:
	// Constructor with id and the queue to use
	Barber(int id, std::shared_ptr<SynchronisedQueue<std::string>> queue) {
		m_id = id;
		m_queue = queue;
	}

	// The thread function reads data from the queue
	void operator()() {
		while (a != 0) {
			if (emptyChairs == nrChairs) {
				std::unique_lock<std::mutex> lock2(m_mutex2);
				shared_print("Barber " + std::to_string(m_id) + " begins to sleep!");
				m_cond2.wait(lock2);
				shared_print("Barber " + std::to_string(m_id) + " wakes up!");
			}
			// Get the data from the queue and print it
			emptyChairs++;
			int t = rand() % 5 + 1; // barber needs t seconds to finish hair cutting where t is between 1 and 5
			shared_print("Barber " + std::to_string(m_id) + " is working for ( " + m_queue->Dequeue() + "), it will take " + std::to_string(t)+ " seconds.") ;
			std::this_thread::sleep_for(std::chrono::seconds(t)); 
		}
	}
};

void getNumberOfChairs() {
	std::cout << "Please enter the number of seats in the waiting room: ";
	std::cin >> nrChairs;
	getchar();
	emptyChairs = nrChairs;
}

int main() {
	getNumberOfChairs();
	if (nrChairs == 0) {
		std::cout << "The number of chairs should be an integer(>0)!"<<std::endl;
		return 0; // terminate the program
	}

	// The number of barbers (for this problem, only one barber)
	int nrBarbers = 1;

	// The shared queue
	auto queue = std::make_shared<SynchronisedQueue<std::string>>();

	// Create barbers
	std::vector<std::thread> barbers;
	for (int i = 1; i <= nrBarbers; i++) {
		Barber c(i, queue);
		barbers.push_back(std::thread(c));
	}

	// Create customers
	std::vector<std::thread> customers;
	std::thread Container([&]() {
		int id = 1;
		while (a != 0) {
			Customer p(id, queue);
			customers.push_back(std::thread(p));
			id++;
			std::this_thread::sleep_for(std::chrono::seconds(3)); // Costomers arrived every 3 seconds
		}

		for (auto& thread : customers) {
			thread.join();
		}
	});

	// Wait for enter (signal to exit)
	getchar();
	a = 0;  // set the flag false to break while-loop in both Barber class and Customer class

	for (auto& thread : barbers) {
		thread.join();
	}

	Container.join();
}
