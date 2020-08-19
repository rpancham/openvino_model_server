//*****************************************************************************
// Copyright 2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************
#include <chrono>
#include <future>
#include <queue>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

#include "../threadsafequeue.hpp"

using ovms::ThreadSafeQueue;

class NonCopyableInt {
public:
    NonCopyableInt(int i) :
        value(i) {}
    NonCopyableInt(NonCopyableInt&) = delete;
    NonCopyableInt(NonCopyableInt&& rhs) {
        this->value = std::move(rhs.value);
    }
    NonCopyableInt operator=(NonCopyableInt&) = delete;
    NonCopyableInt& operator=(NonCopyableInt&& rhs) {
        value = std::move(rhs.value);
        return *this;
    }
    bool operator==(const NonCopyableInt& rhs) const {
        return this->value == rhs.value;
    }

private:
    int value;
};

TEST(TestThreadSafeQueue, PushElement) {
    ThreadSafeQueue<int> queue;
    int i = 1;
    queue.push(i);
}

TEST(TestThreadSafeQueue, SeveralElementsInFIFOOrder) {
    const std::vector<int> elements = {1, 2, 3, 4, 5, 6};
    ThreadSafeQueue<int> queue;
    for (auto& e : elements) {
        queue.push(e);
    }
    for (auto& e : elements) {
        EXPECT_EQ(e, queue.waitAndPull());
    }
}

TEST(TestThreadSafeQueue, DISABLED_NoElementsPushed) {
    // TODO handle timeout
    const std::vector<int> elements = {};
    ThreadSafeQueue<int> queue;
    EXPECT_EQ(0, queue.waitAndPull());
}

const uint ELEMENTS_TO_INSERT = 500;

void producer(ThreadSafeQueue<int>& queue, std::future<void> startSignal) {
    startSignal.get();
    uint counter = 0;
    while (counter < ELEMENTS_TO_INSERT) {
        queue.push(counter);
        ++counter;
    }
}

void consumer(ThreadSafeQueue<int>& queue, std::future<void> startSignal, std::vector<int>& consumed, const uint elementsToPull) {
    startSignal.get();
    uint counter = 0;
    while (counter < elementsToPull) {
        consumed[counter] = queue.waitAndPull();
        counter++;
    }
}

TEST(TestThreadSafeQueue, SeveralThreadsAllElementsPresent) {
    using std::thread;
    using std::vector;
    const uint NUMBER_OF_PRODUCERS = 80;
    const uint NUMBER_OF_CONSUMERS = 1;

    vector<thread> producers;
    vector<thread> consumers;

    vector<int> consumed(NUMBER_OF_PRODUCERS * ELEMENTS_TO_INSERT);
    vector<std::promise<void>> startProduceSignal(NUMBER_OF_PRODUCERS);
    std::promise<void> startConsumeSignal;

    ThreadSafeQueue<int> queue;
    for (auto i = 0u; i < NUMBER_OF_PRODUCERS; ++i) {
        producers.emplace_back(thread(producer, std::ref(queue), startProduceSignal[i].get_future()));
    }
    std::thread consumerThread(consumer, std::ref(queue), startConsumeSignal.get_future(), std::ref(consumed), (const uint)(NUMBER_OF_PRODUCERS * ELEMENTS_TO_INSERT));
    for (auto& signal : startProduceSignal) {
        signal.set_value();
    }
    startConsumeSignal.set_value();
    for (auto& t : producers) {
        t.join();
    }
    consumerThread.join();
    std::map<int, int> counts;
    for (auto e : consumed) {
        counts[e]++;
    }
    std::cout << std::endl;
    for (auto [key, counter] : counts) {
        EXPECT_EQ(NUMBER_OF_PRODUCERS, counter);
    }
}

TEST(TestThreadSafeQueue, NonCopyableType) {
    const std::vector<int> elements = {1, 2, 3, 4, 5, 6};
    ThreadSafeQueue<NonCopyableInt> queue;
    for (auto& e : elements) {
        queue.push(std::move(NonCopyableInt(e)));
    }
    for (auto& e : elements) {
        NonCopyableInt ncpyInt{queue.waitAndPull()};
        EXPECT_EQ(NonCopyableInt(e), ncpyInt);
    }
}