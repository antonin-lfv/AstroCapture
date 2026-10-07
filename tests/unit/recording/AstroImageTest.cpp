#include <astro/recording/AstroImage.hpp>
#include <astro/recording/FitsSequenceRecorder.hpp>
#include <fitsio.h>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif
#include "Check.hpp"
using namespace astro;
void writeBayerDng(const std::filesystem::path& path) {
    struct Tag{std::uint16_t id,type;std::uint32_t count,value;};std::vector<Tag> tags;std::vector<unsigned char> external;
    const unsigned externalOffset=8+2+21*12+4;
    auto u16=[](std::vector<unsigned char>& out,unsigned value){out.push_back(value&255);out.push_back((value>>8)&255);};
    auto u32=[](std::vector<unsigned char>& out,unsigned value){for(int i=0;i<4;++i)out.push_back((value>>(i*8))&255);};
    auto text=[&](unsigned tag,const std::string& value){tags.push_back({std::uint16_t(tag),2,unsigned(value.size()+1),externalOffset+unsigned(external.size())});external.insert(external.end(),value.begin(),value.end());external.push_back(0);};
    tags={{256,4,1,128},{257,4,1,96},{258,3,1,16},{259,3,1,1},{262,3,1,32803}};
    text(271,"AstroCapture");text(272,"Synthetic Bayer");
    tags.push_back({273,4,1,0});tags.push_back({274,3,1,1});tags.push_back({277,3,1,1});tags.push_back({278,4,1,96});tags.push_back({279,4,1,128*96*2});tags.push_back({284,3,1,1});
    tags.push_back({33421,3,2,0x00020002});tags.push_back({33422,1,4,0x02010100});tags.push_back({50706,1,4,0x00000401});tags.push_back({50707,1,4,0x00000101});text(50708,"AstroCapture Bayer");tags.push_back({50717,4,1,65535});
    while(external.size()%4)external.push_back(0);
    tags.push_back({50721,10,9,externalOffset+unsigned(external.size())});for(int i=0;i<9;++i){u32(external,i%4==0?1:0);u32(external,1);}
    tags.push_back({50778,3,1,21});ASTRO_TEST_CHECK(tags.size()==21);
    for(auto& tag:tags)if(tag.id==273)tag.value=externalOffset+unsigned(external.size());
    std::sort(tags.begin(),tags.end(),[](const Tag& a,const Tag& b){return a.id<b.id;});
    std::vector<unsigned char> bytes{'I','I'};u16(bytes,42);u32(bytes,8);u16(bytes,tags.size());
    for(const auto& tag:tags){u16(bytes,tag.id);u16(bytes,tag.type);u32(bytes,tag.count);u32(bytes,tag.value);}u32(bytes,0);
    bytes.insert(bytes.end(),external.begin(),external.end());for(int i=0;i<128*96;++i)u16(bytes,40000+i%1000);
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
int main(){
    auto directory=std::filesystem::temp_directory_path()/"astro-fits-roundtrip-tests";
    std::filesystem::remove_all(directory);std::filesystem::create_directory(directory);
    recording::AstroImage image;image.width=5;image.height=4;image.channels=1;image.bits=16;image.bayer=domain::BayerPattern::RGGB;image.exposure=120;image.filter="Ha";
    image.samples.resize(20);for(int i=0;i<20;++i)image.samples[i]=float(1000+i)/65535;
    image.samples[0]=-.0003;image.samples[1]=1.25;image.samples[2]=NAN;
    const auto path=directory/"float.fits";recording::writeAstroFits(path,image);
    auto read=recording::readAstroImage(path);
    ASTRO_TEST_CHECK(read.bayer==image.bayer && read.filter=="Ha" && read.exposure==120);
    ASTRO_TEST_CHECK(read.samples[0]==image.samples[0]&&read.samples[1]==1.25&&std::isnan(read.samples[2]));
    for(int i=3;i<20;++i)ASTRO_TEST_CHECK(read.samples[i]==image.samples[i]);
    bool refused=false;try{recording::writeAstroFits(path,image);}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
    // Independent unsigned 16-bit CCD FITS fixture checks BZERO and physical ADU scaling.
    fitsfile* f=nullptr;int status=0;long axes[2]{5,4};std::vector<unsigned short> adu(20);for(int i=0;i<20;++i)adu[i]=40000+i;
    const auto unsignedPath=directory/"unsigned.fits";
    fits_create_diskfile(&f,unsignedPath.string().c_str(),&status);fits_create_img(f,USHORT_IMG,2,axes,&status);fits_write_img(f,TUSHORT,1,20,adu.data(),&status);fits_close_file(f,&status);ASTRO_TEST_CHECK(!status);
    read=recording::readAstroImage(unsignedPath);for(int i=0;i<20;++i)ASTRO_TEST_CHECK(std::abs(read.samples[i]-adu[i]/65535.f)<1e-7);
    // Compressed image extension with an empty primary HDU.
    const auto compressedPath=directory/"compressed.fits.fz";
    status=0;fits_create_diskfile(&f,compressedPath.string().c_str(),&status);fits_create_img(f,BYTE_IMG,0,nullptr,&status);fits_set_compression_type(f,RICE_1,&status);fits_create_img(f,USHORT_IMG,2,axes,&status);fits_write_img(f,TUSHORT,1,20,adu.data(),&status);fits_close_file(f,&status);ASTRO_TEST_CHECK(!status);
    read=recording::readAstroImage(compressedPath);for(int i=0;i<20;++i)ASTRO_TEST_CHECK(std::abs(read.samples[i]-adu[i]/65535.f)<1e-7);
    // Padded raw camera frame: adjacent 16-bit levels must not collapse into preview precision.
    domain::FrameBuffer frame;frame.descriptor={5,4,12,domain::PixelStorage::Bayer16,domain::BayerPattern::RGGB,16};
    std::vector<std::byte> bytes(48);for(int y=0;y<4;++y)for(int x=0;x<5;++x){std::uint16_t v=1000+y*5+x;std::memcpy(bytes.data()+y*12+x*2,&v,2);}frame.storage=std::make_shared<const domain::BufferStorage>(std::move(bytes));
    auto camera=recording::imageFromFrame(frame);ASTRO_TEST_CHECK(camera.samples[1]>camera.samples[0]);
    recording::FitsSequenceRecorder sequence;sequence.metadata.exposure=120;sequence.metadata.filter="R";
    ASTRO_TEST_CHECK(sequence.open((directory/"sequence").string(),frame.descriptor,1));ASTRO_TEST_CHECK(sequence.writeFrame(frame));ASTRO_TEST_CHECK(sequence.close().value().framesWritten==1);
    read=recording::readAstroImage(directory/"sequence/frame-000001.fits");ASTRO_TEST_CHECK(read.exposure==120&&read.filter=="R"&&read.bayer==domain::BayerPattern::RGGB&&read.samples==camera.samples);
    const auto rawPath=directory/"sensor.dng";writeBayerDng(rawPath);
    const auto raw=recording::readAstroImage(rawPath);
    ASTRO_TEST_CHECK(raw.width==128&&raw.height==96&&raw.channels==1&&raw.bayer==domain::BayerPattern::RGGB);
    ASTRO_TEST_CHECK(raw.samples[1]>raw.samples[0]&&std::abs(raw.samples[0]-40000.f/65535)<1e-6);
#if defined(__unix__) || defined(__APPLE__)
    struct WorkerRead {std::filesystem::path fits,raw;bool success=false;} context{compressedPath,rawPath};
    pthread_attr_t attributes;ASTRO_TEST_CHECK(pthread_attr_init(&attributes)==0);
    ASTRO_TEST_CHECK(pthread_attr_setstacksize(&attributes,256*1024)==0);
    pthread_t thread;
    ASTRO_TEST_CHECK(pthread_create(&thread,&attributes,[](void* pointer)->void* {
        auto& state=*static_cast<WorkerRead*>(pointer);
        try {state.success=!recording::readAstroImage(state.fits).samples.empty()&&!recording::readAstroImage(state.raw).samples.empty();}catch(...){}
        return nullptr;
    },&context)==0);
    pthread_attr_destroy(&attributes);ASTRO_TEST_CHECK(pthread_join(thread,nullptr)==0);
    ASTRO_TEST_CHECK(context.success);
#endif
    // Truncated file must fail rather than display fabricated data.
    std::filesystem::resize_file(unsignedPath,2881);refused=false;try{recording::readAstroImage(unsignedPath);}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
    std::filesystem::remove_all(directory);std::cout<<"OK\n";
}
