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

#ifndef FILESYSTEM_STATS_H
#define FILESYSTEM_STATS_H

#include <utils/globals.h>

#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <sys/statvfs.h>

namespace metrics {

struct Filesystem {
    std::string name;
    std::string mountPoint;
    std::string type;
    uint64_t totalBlocks = 0;
    uint64_t usedBlocks = 0;
    uint64_t availableBlocks = 0;
    uint64_t blocksize = 1;  // KB
    long double inodeUsed = 0;
    long double spaceUsed = 0;

    void printValues() const {
        std::cout << "name: " << name << std::endl;
        std::cout << "mountPoint: " << mountPoint << std::endl;
        std::cout << "type: " << type << std::endl;
        std::cout << "totalBlocks: " << totalBlocks << std::endl;
        std::cout << "usedBlocks: " << usedBlocks << std::endl;
        std::cout << "availableBlocks: " << availableBlocks << std::endl;
        std::cout << "blocksize: " << blocksize << std::endl;
        std::cout << "inodeUsed: " << inodeUsed << std::endl;
        std::cout << "spaceUsed: " << spaceUsed << std::endl;
    }

    void setXpathValues(sysrepo::S_Session session, libyang::S_Data_Node& parent) const {
        std::string filesystemPath(
            "/dt-metrics:system-metrics/filesystems/filesystem[mount-point='" + mountPoint +
            "']/statistics/");
        setXpath(session, parent, filesystemPath + "name", name);
        setXpath(session, parent, filesystemPath + "type", type);
        setXpath(session, parent, filesystemPath + "total-blocks", std::to_string(totalBlocks));
        setXpath(session, parent, filesystemPath + "used-blocks", std::to_string(usedBlocks));
        setXpath(session, parent, filesystemPath + "avail-blocks", std::to_string(availableBlocks));
        setXpath(session, parent, filesystemPath + "blocksize", std::to_string(blocksize));

        {
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << spaceUsed;
            setXpath(session, parent, filesystemPath + "space-used", stream.str());
        }
        {
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << inodeUsed;
            setXpath(session, parent, filesystemPath + "inode-used", stream.str());
        }
    }
};

struct FilesystemStats {

    static FilesystemStats& getInstance() {
        static FilesystemStats instance;
        return instance;
    }

    FilesystemStats(FilesystemStats const&) = delete;
    void operator=(FilesystemStats const&) = delete;

    void readFilesystemStats() {
        int rc = system("/bin/df -T > " FILESYSTEM_STATS_LOCATION
                        "&& /bin/df -i > " FILESYSTEM_STATS_LOCATION2);
        if (rc == -1) {
            logMessage(SR_LL_ERR, "df command failed");
            return;
        }
        logMessage(SR_LL_DBG, "df command returned:" + std::to_string(rc));
        std::string token, inodesToken;
        std::ifstream file(FILESYSTEM_STATS_LOCATION), fileinodes(FILESYSTEM_STATS_LOCATION2);
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        fileinodes.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        while (file >> token || fileinodes >> inodesToken) {
            Filesystem fs;
            uint64_t inodesTotal;
            uint64_t inodesUsed;
            fs.name = token;
            file >> fs.type >> fs.totalBlocks >> fs.usedBlocks >> fs.availableBlocks >> token >>
                fs.mountPoint;
            fileinodes >> token >> inodesTotal >> inodesUsed;

            struct statvfs buf;
            if (statvfs(fs.mountPoint.c_str(), &buf) == 0) {
                fs.blocksize = buf.f_bsize / 1024;  // KB
                fs.totalBlocks = buf.f_blocks;
                fs.availableBlocks = buf.f_bfree;
                fs.usedBlocks = fs.totalBlocks - fs.availableBlocks;
                inodesTotal = buf.f_files;
                inodesUsed = inodesTotal - buf.f_ffree;
            }
            if (inodesTotal == 0) {
                fs.inodeUsed = 0;
            } else {
                fs.inodeUsed = inodesUsed * 100.0 / static_cast<long double>(inodesTotal);
            }
            if (fs.totalBlocks == 0) {
                fs.spaceUsed = 0;
            } else {
                fs.spaceUsed = fs.usedBlocks * 100.0 / static_cast<long double>(fs.totalBlocks);
            }
            fsMap.emplace(std::make_pair(fs.mountPoint, fs));
            // ignore rest of the line
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            fileinodes.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    std::optional<long double> getUsage(std::string mountPoint) {
        readFilesystemStats();
        std::unordered_map<std::string, Filesystem>::iterator itr;
        if ((itr = fsMap.find(mountPoint)) != fsMap.end()) {
            return itr->second.spaceUsed;
        } else {
            return std::nullopt;
        }
    }

    void printValues() {
        for (auto const& v : fsMap) {
            v.second.printValues();
            std::cout << std::endl;
        }
    }

    void setXpathValues(sysrepo::S_Session session, libyang::S_Data_Node& parent) {
        for (auto const& v : fsMap) {
            v.second.setXpathValues(session, parent);
        }
    }

private:
    FilesystemStats() = default;
    std::unordered_map<std::string, Filesystem> fsMap;
};

}  // namespace metrics

#endif  // FILESYSTEM_STATS_H
