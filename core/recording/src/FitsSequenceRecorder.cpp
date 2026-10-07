#include <astro/recording/FitsSequenceRecorder.hpp>
#include <iomanip>
#include <sstream>
namespace astro::recording {
using namespace domain;
Result<void> FitsSequenceRecorder::open(const std::string& path,const FrameDescriptor& d,double) {
    try {
        directory_=path; descriptor_=d; count_=0;
        if(!std::filesystem::create_directory(directory_))throw std::runtime_error("FITS sequence directory already exists.");
        return Result<void>::success();
    }catch(const std::exception& e){return Result<void>::failure({ErrorCode::RecordingOpenFailed,e.what(),path});}
}
Result<void> FitsSequenceRecorder::writeFrame(const FrameBuffer& frame) {
    try {
        if(frame.descriptor.width!=descriptor_.width||frame.descriptor.height!=descriptor_.height||frame.descriptor.storage!=descriptor_.storage)
            throw std::runtime_error("Camera format changed during FITS acquisition.");
        auto image=imageFromFrame(frame);
        image.exposure=metadata.exposure; image.gain=metadata.gain; image.hasGain=metadata.hasGain;
        image.imageType=metadata.imageType; image.filter=metadata.filter; image.object=metadata.object;
        const auto now=std::chrono::system_clock::now();
        const auto seconds=std::chrono::system_clock::to_time_t(now);
        std::tm time{};
#ifdef _WIN32
        gmtime_s(&time,&seconds);
#else
        gmtime_r(&seconds,&time);
#endif
        std::ostringstream date; date<<std::put_time(&time,"%Y-%m-%dT%H:%M:%S"); image.date=date.str();
        std::ostringstream name; name<<"frame-"<<std::setw(6)<<std::setfill('0')<<count_+1<<".fits";
        writeAstroFits(directory_/name.str(),image); ++count_;
        return Result<void>::success();
    }catch(const std::exception& e){return Result<void>::failure({ErrorCode::DiskWriteFailed,e.what(),directory_.string()});}
}
Result<RecordingSummary> FitsSequenceRecorder::close(){return Result<RecordingSummary>::success({count_});}
}
