// (C) 2020 Deutsche Telekom AG.
//
// Deutsche Telekom AG and all other contributors /
// copyright owners license this file to you under the Apache
// License, Version 2.0 (the "License"); you may not use this
// file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef MEMORY_STATS_H
#define MEMORY_STATS_H

#include <utils/globals.h>

#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>

namespace metrics {

struct MemoryStats {

    MemoryStats()
        : mFree(0), mSwapFree(0), mSwapTotal(0), mSwapUsed(0), mTotal(0), mUsable(0),
          mUsedBuffers(0), mUsedCached(0), mUsedShared(0), mHugePagesTotal(0), mHugePagesFree(0),
          mHugePageSize(0) {
        assignMap = {{"MemTotal:", [this](uint64_t value) { mTotal = value; }},
                     {"MemFree:", [this](uint64_t value) { mFree = value; }},
                     {"MemAvailable:", [this](uint64_t value) { mUsable = value; }},
                     {"SwapTotal:", [this](uint64_t value) { mSwapTotal = value; }},
                     {"SwapFree:", [this](uint64_t value) { mSwapFree = value; }},
                     {"Shmem:", [this](uint64_t value) { mUsedShared = value; }},
                     {"Cached:", [this](uint64_t value) { mUsedCached = value; }},
                     {"Buffers:", [this](uint64_t value) { mUsedBuffers = value; }},
                     {"HugePages_Total:", [this](uint64_t value) { mHugePagesTotal = value; }},
                     {"HugePages_Free:", [this](uint64_t value) { mHugePagesFree = value; }},
                     {"Hugepagesize:", [this](uint64_t value) { mHugePageSize = value; }}};
    }

    void setXpathValues(sysrepo::S_Session session, libyang::S_Data_Node& parent) {
        setXpath(session, parent, "/dt-metrics:metrics/memory/free",
                 std::to_string(mFree / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/swap-free",
                 std::to_string(mSwapFree / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/swap-total",
                 std::to_string(mSwapTotal / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/swap-used",
                 std::to_string(mSwapUsed / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/total",
                 std::to_string(mTotal / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/usable",
                 std::to_string(mUsable / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/used-buffers",
                 std::to_string(mUsedBuffers / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/used-cached",
                 std::to_string(mUsedCached / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/used-shared",
                 std::to_string(mUsedShared / 1024ULL));
        setXpath(session, parent, "/dt-metrics:metrics/memory/hugepages-total",
                 std::to_string(mHugePagesTotal));
        setXpath(session, parent, "/dt-metrics:metrics/memory/hugepages-free",
                 std::to_string(mHugePagesFree));
        setXpath(session, parent, "/dt-metrics:metrics/memory/hugepage-size",
                 std::to_string(mHugePageSize));

        if (mTotal != 0) {
            long double usable = mUsable / static_cast<long double>(mTotal) * 100.0;
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << usable;
            setXpath(session, parent, "/dt-metrics:metrics/memory/usable-perc", stream.str());
        }
        if (mSwapTotal != 0) {
            long double swapFree = mSwapFree / static_cast<long double>(mSwapTotal) * 100.0;
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << swapFree;
            setXpath(session, parent, "/dt-metrics:metrics/memory/swap-free-perc", stream.str());
        }
    }

    void readMemoryStats() {
        std::string token;
        std::ifstream file("/proc/meminfo");
        while (file >> token) {
            auto const& itr = assignMap.find(token);
            if (itr != assignMap.end()) {
                uint64_t mem;
                if (file >> mem) {
                    assignMap[token](mem);
                }
            }
            // ignore rest of the line
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        mSwapUsed = mSwapTotal - mSwapFree;
    }

    void printValues() {
        std::cout << "MemTotal:" << mTotal << std::endl;
        std::cout << "MemFree:" << mFree << std::endl;
        std::cout << "MemAvailable:" << mUsable << std::endl;
        std::cout << "SwapTotal:" << mSwapTotal << std::endl;
        std::cout << "SwapFree:" << mSwapFree << std::endl;
        std::cout << "SwapUsed:" << mSwapUsed << std::endl;
        std::cout << "Shmem:" << mUsedShared << std::endl;
        std::cout << "Cached:" << mUsedCached << std::endl;
        std::cout << "Buffers:" << mUsedBuffers << std::endl;
        std::cout << "HugePages_Total:" << mHugePagesTotal << std::endl;
        std::cout << "HugePages_Free:" << mHugePagesFree << std::endl;
        std::cout << "Hugepagesize:" << mHugePageSize << std::endl;
    }

    uint64_t mFree;
    uint64_t mSwapFree;
    uint64_t mSwapTotal;
    uint64_t mSwapUsed;
    uint64_t mTotal;
    uint64_t mUsable;
    uint64_t mUsedBuffers;
    uint64_t mUsedCached;
    uint64_t mUsedShared;
    uint64_t mHugePagesTotal;
    uint64_t mHugePagesFree;
    uint64_t mHugePageSize;

    std::unordered_map<std::string, std::function<void(uint64_t)>> assignMap;
};

}  // namespace metrics

#endif  // MEMORY_STATS_H