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

#ifndef CPU_STATS_H
#define CPU_STATS_H

#include <utils/globals.h>

#include <fstream>
#include <iomanip>
#include <numeric>
#include <optional>
#include <sstream>
#include <vector>

namespace metrics {

struct CoreStats {

    CoreStats()
        : mUser(0), mNice(0), mSystem(0), mIdle(0), mIowait(0), mIrq(0), mSoftirq(0), mStolen(0),
          mTotal(1){};

    CoreStats(std::vector<size_t> const& cpu_times) {
        populateValues(cpu_times);
    }

    void printValues() const {
        std::cout << mUser << " " << mUser << " " << mSystem << " " << mIdle << " " << mIowait
                  << " " << mIrq << " " << mSoftirq << " " << mStolen << " " << mTotal << std::endl;
    }

    void setXpathValues(sysrepo::S_Session session,
                        libyang::S_Data_Node& parent,
                        std::optional<size_t> index) {
        std::string cpuPath;
        if (index) {
            cpuPath = "/cpu[id='" + std::to_string(index.value()) + "']";
        }
        std::stringstream stream;
        stream << std::fixed << std::setprecision(2)
               << mUser / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/user-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mSystem / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/sys-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mNice / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/nice-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mIdle / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/idle-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mIowait / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/wait-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mIrq / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/irq-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mSoftirq / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/softirq-perc",
                 stream.str());
        stream = std::stringstream();
        stream << std::fixed << std::setprecision(2)
               << mStolen / static_cast<long double>(mTotal) * 100.0;
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state" + cpuPath + "/stolen-perc",
                 stream.str());
    }

    void populateValues(std::vector<size_t> const& cpu_times) {
        mUser = cpu_times[0];
        mNice = cpu_times[1];
        mSystem = cpu_times[2];
        mIdle = cpu_times[3];
        mIowait = cpu_times[4];
        mIrq = cpu_times[5];
        mSoftirq = cpu_times[6];
        mStolen = cpu_times[7];

        mTotal = accumulate(cpu_times.begin(), cpu_times.end(), 0);
    }

protected:
    size_t mUser;
    size_t mNice;
    size_t mSystem;
    size_t mIdle;
    size_t mIowait;
    size_t mIrq;
    size_t mSoftirq;
    size_t mStolen;
    size_t mTotal;
};

struct CpuStats : public CoreStats {

    CpuStats() = default;

    CpuStats(std::vector<size_t> const& cpu_times) : CoreStats(cpu_times){};

    void printValues() const {
        CoreStats::printValues();
        for (auto const& c : mCoreTimes) {
            c.printValues();
        }
    }

    void setXpathValues(sysrepo::S_Session session, libyang::S_Data_Node& parent) {
        CoreStats::setXpathValues(session, parent, std::nullopt);
        setXpath(session, parent, "/dt-metrics:metrics/cpu-state/num-cpus",
                 std::to_string(mCoreTimes.size()));
        for (size_t i = 0; i < mCoreTimes.size(); i++) {
            mCoreTimes[i].setXpathValues(session, parent, i);
        }
    }

    void readCpuTimes() {
        std::ifstream proc_stat("/proc/stat");
        std::string line;
        std::vector<size_t> cpu_times;
        std::getline(proc_stat, line);
        std::istringstream stream(line);
        stream.ignore(5, ' ');  // ignore cpu keyword
        for (size_t time; stream >> time; cpu_times.push_back(time))
            ;
        CoreStats::populateValues(cpu_times);

        std::getline(proc_stat, line);
        while (std::string(line).find("cpu") != std::string::npos) {
            stream = std::istringstream(line);
            stream.ignore(5, ' ');  // ignore cpu keyword
            std::vector<size_t> cpu_times;
            for (size_t time; stream >> time; cpu_times.push_back(time))
                ;
            CpuStats coreStats(cpu_times);
            mCoreTimes.emplace_back(coreStats);
            std::getline(proc_stat, line);
        }
    }

    std::vector<CoreStats> mCoreTimes;
};

}  // namespace metrics

#endif  // CPU_STATS_H
