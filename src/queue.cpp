#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

using ConsumerID = uint64_t;

// Topic is a thread-safe multiple-consumer/multiple-producer queue
// persisted to disk
class Topic {
    struct ConsumerMeta {
        ifstream ifs;
        size_t consumed = 0;
    };

  public:
    Topic(const string &name) : name_(name), writer_(name) {

        thread_ = thread([this]() {
            auto lock = unique_lock<mutex>(qlock_);

            while (true) {
                writercv_.wait(lock, [this]() { return !queue_.empty(); });
                if (!queue_.empty()) {
                    auto payload = move(queue_.front());
                    queue_.pop();

                    // Flush to disk outside of lock and allow public writers to add to queue
                    lock.unlock();
                    // TODO: Make this async
                    write_(move(payload));
                }

                lock.lock();
            }
        });
    }

    ConsumerID registerConsumer() {
        auto id = cur_id_++;
        consumers_[id] = {ifstream(name_), 0};
        return id;
    }

    void write(string &&s) {
        {
            lock_guard<mutex> lock(qlock_);
            queue_.push(move(s));
        }
        writercv_.notify_one();
    }

    string read(ConsumerID cid) {
        // First read the size
        union {
            char buf[sizeof(size_t)];
            size_t n;
        } x;

        auto &consumed = consumers_[cid].consumed;
        while (consumed == written_) {
            sleep(1);
        }

        auto &ifs = consumers_[cid].ifs;
        ifs.read(x.buf, sizeof(x.buf));

        // Then read that many more bytes
        string s(x.n, '\0');
        ifs.read((char *)s.data(), s.size());
        ++consumed;
        return s;
    }

  private:
    void write_(string &&s) {
        // Write size-prefixed payload to file
        auto n = s.size();
        const char *buf = reinterpret_cast<const char *>(&n);
        writer_.write(buf, sizeof(decltype(n)));
        writer_.write(s.data(), n);
        writer_.flush();
        ++written_;
        readercv_.notify_all();
    }

    const string name_;
    ofstream writer_;
    map<ConsumerID, ConsumerMeta> consumers_;
    atomic<ConsumerID> cur_id_{0};

    thread thread_;
    mutex qlock_;
    queue<string> queue_;
    condition_variable writercv_;
    condition_variable readercv_;
    atomic<size_t> written_{0};
};

int main() {
    Topic t("test");

    // Concurrently write and read from queue
    std::vector<std::thread> writers;
    for (int i = 0; i < 10; ++i) {
        writers.push_back(std::thread([&t]() { return t.write("something"); }));
    }

    std::vector<std::thread> readers;
    for (int i = 0; i < 100; ++i) {
        auto cid = t.registerConsumer();
        readers.push_back(std::thread([&t, cid]() {
            for (size_t i = 0; i < 10; ++i) {
                cout << t.read(cid) << endl;
            }
        }));
    }

    for (auto &t : writers) {
        t.join();
    }
    for (auto &t : readers) {
        t.join();
    }
}
