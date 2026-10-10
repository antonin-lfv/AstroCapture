#pragma once
#include <astro/processing/Planetary.hpp>
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace astro::processing {
struct StorageError : std::runtime_error { using std::runtime_error::runtime_error; };
// One anonymous/deleted-on-close archive per job, even for 500 exposures.
// The OS releases its payload when the process closes or crashes.
class WorkingFiles {
public:
    WorkingFiles(const std::filesystem::path& parent,std::uint64_t required,std::uint64_t reserve) {
        std::error_code ec;
        const auto base=parent.empty()?std::filesystem::temp_directory_path(ec):parent;
        if(ec || !std::filesystem::is_directory(base,ec) || ec)
            throw StorageError("The temporary directory is unavailable. Choose an existing local folder.");
        const auto space=std::filesystem::space(base,ec);
        if(ec || space.available==std::uintmax_t(-1))throw StorageError("Cannot check free space in the temporary directory.");
        if(required>space.available || reserve>space.available-required)
            throw StorageError("Not enough temporary disk space for integration. Choose another temporary folder or fewer exposures.");
        parent_=base;reserve_=reserve;
        std::random_device random;
        for(int n=0;n<16;++n) {
            const auto candidate=base/("astrocapture-work-"+std::to_string(random())+"-"+std::to_string(random()));
            if(!std::filesystem::create_directory(candidate,ec)) {
                if(ec)throw StorageError("Cannot create private integration working files.");
                continue;
            }
            directory_=candidate;
            std::filesystem::permissions(candidate,std::filesystem::perms::owner_all,std::filesystem::perm_options::replace,ec);
            if(ec){cleanup();throw StorageError("Cannot create private integration working files.");}
            const auto path=candidate/"samples.float";
#ifdef _WIN32
            const int descriptor=_wopen(path.c_str(),_O_CREAT|_O_EXCL|_O_RDWR|_O_BINARY|_O_TEMPORARY|_O_NOINHERIT,_S_IREAD|_S_IWRITE);
            if(descriptor>=0){file_.reset(_fdopen(descriptor,"w+b"));if(!file_)_close(descriptor);}
#else
            const int descriptor=::open(path.c_str(),O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);
            if(descriptor>=0){file_.reset(fdopen(descriptor,"w+b"));if(!file_)::close(descriptor);}
#endif
            if(!file_){cleanup();throw StorageError("Cannot open integration working files.");}
#ifndef _WIN32
            if(!std::filesystem::remove(path,ec) || ec){file_.reset();cleanup();throw StorageError("Cannot create private integration working files.");}
            // With the open payload unlinked, a crash leaves no large files.
            std::filesystem::remove(directory_,ec);
            if(!ec)directory_.clear();
#endif
            return;
        }
        throw StorageError("Cannot create private integration working files.");
    }
    ~WorkingFiles(){file_.reset();cleanup();}
    WorkingFiles(const WorkingFiles&)=delete;
    WorkingFiles& operator=(const WorkingFiles&)=delete;
    std::uint64_t write(const std::vector<float>& samples,const std::vector<float>* weights,const CheckCancel& check) {
        const auto bytes=(std::uint64_t(samples.size())+(weights?weights->size():0))*sizeof(float);
        checkSpace(bytes);
        seek(next_);const auto start=next_;
        auto append=[&](const std::vector<float>& data) {
            for(std::size_t at=0;at<data.size();at+=65536) {
                if(check)check();const auto size=std::min<std::size_t>(65536,data.size()-at);
                if(next_-lastSpaceCheck_>=16ULL*1024*1024){checkSpace(bytes-(next_-start));lastSpaceCheck_=next_;}
                if(std::fwrite(data.data()+at,sizeof(float),size,file_.get())!=size)
                    throw StorageError("Not enough temporary disk space for integration.");
                next_+=size*sizeof(float);
            }
        };
        append(samples);if(weights)append(*weights);
        if(std::fflush(file_.get()))throw StorageError("Cannot finish writing integration working files.");
        return start;
    }
    void read(float* target,std::size_t count,std::uint64_t offset) {
        if(offset>next_ || count>(next_-offset)/sizeof(float))throw StorageError("Cannot read temporary integration data.");
        seek(offset);
        if(std::fread(target,sizeof(float),count,file_.get())!=count)throw StorageError("Cannot read temporary integration data.");
    }
private:
    void checkSpace(std::uint64_t remaining) {
        std::error_code ec;const auto space=std::filesystem::space(parent_,ec);
        if(ec || space.available==std::uintmax_t(-1))throw StorageError("Cannot check free space in the temporary directory.");
        if(remaining>space.available || reserve_>space.available-remaining)
            throw StorageError("Not enough temporary disk space for integration. Choose another temporary folder or fewer exposures.");
    }
    void seek(std::uint64_t offset) {
#ifdef _WIN32
        const int result=_fseeki64(file_.get(),offset,SEEK_SET);
#else
        const int result=fseeko(file_.get(),off_t(offset),SEEK_SET);
#endif
        if(result)throw StorageError("Cannot read temporary integration data.");
    }
    void cleanup(){if(!directory_.empty()){std::error_code ec;std::filesystem::remove_all(directory_,ec);}}
    std::filesystem::path directory_;
    std::filesystem::path parent_;
    std::unique_ptr<FILE,decltype(&std::fclose)> file_{nullptr,&std::fclose};
    std::uint64_t next_{},lastSpaceCheck_{},reserve_{};
};
}
