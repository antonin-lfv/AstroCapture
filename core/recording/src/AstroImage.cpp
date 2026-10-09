#include <astro/recording/AstroImage.hpp>
#include <fitsio.h>
#include <libraw/libraw.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace astro::recording {
namespace {
void fail(int status) {
    if (!status) return;
    char message[FLEN_STATUS]; fits_get_errstatus(status, message);
    throw std::runtime_error(std::string("FITS: ") + message);
}
struct Fits {
    fitsfile* file{};
    ~Fits(){ if(file){int status=0; fits_close_file(file,&status);} }
};
std::string stringKey(fitsfile* f,const char* key) {
    char value[FLEN_VALUE]{}; int status=0;
    fits_read_key(f,TSTRING,key,value,nullptr,&status);
    return status ? std::string{} : std::string(value);
}
double numberKey(fitsfile* f,const char* key,double fallback=0) {
    double value=fallback; int status=0;
    fits_read_key(f,TDOUBLE,key,&value,nullptr,&status);
    return status || !std::isfinite(value) ? fallback : value;
}
domain::BayerPattern pattern(std::string text) {
    if(text=="RGGB")return domain::BayerPattern::RGGB;
    if(text=="BGGR")return domain::BayerPattern::BGGR;
    if(text=="GRBG")return domain::BayerPattern::GRBG;
    if(text=="GBRG")return domain::BayerPattern::GBRG;
    return domain::BayerPattern::None;
}
void validate(int w,int h,int c) {
    if(w<=0||h<=0||(c!=1&&c!=3)||std::uint64_t(w)*h>32000000)
        throw std::runtime_error("Images must be mono or RGB and at most 32 megapixels.");
}
AstroImage readFits(const std::filesystem::path& path,double inputScale) {
    Fits f; int status=0;
    fits_open_diskfile(&f.file,path.string().c_str(),READONLY,&status); fail(status);
    int count=0; fits_get_num_hdus(f.file,&count,&status); fail(status);
    long axes[3]{}; int dim=0,bitpix=0; bool found=false;
    for(int hdu=1;hdu<=count;++hdu) {
        int type=0; fits_movabs_hdu(f.file,hdu,&type,&status); fail(status);
        if(type!=IMAGE_HDU && !fits_is_compressed_image(f.file,&status))continue;
        fits_get_img_param(f.file,3,&bitpix,&dim,axes,&status); fail(status);
        if(dim>=2&&dim<=3&&axes[0]>0&&axes[1]>0) {found=true;break;}
    }
    if(!found)throw std::runtime_error("No supported image HDU in this FITS file.");
    if(axes[0]>32000000||axes[1]>32000000)throw std::runtime_error("FITS dimensions are too large.");
    const int channels=dim==2?1:int(axes[2]);
    validate(int(axes[0]),int(axes[1]),channels);
    AstroImage image; image.width=int(axes[0]); image.height=int(axes[1]); image.channels=channels;
    image.fitsBitpix=bitpix; image.bits=std::abs(bitpix); image.bayer=pattern(stringKey(f.file,"BAYERPAT"));
    if(channels!=1)image.bayer=domain::BayerPattern::None;
    image.exposure=numberKey(f.file,"EXPTIME",numberKey(f.file,"EXPOSURE"));
    image.gain=numberKey(f.file,"GAIN",std::numeric_limits<double>::quiet_NaN()); image.hasGain=std::isfinite(image.gain);
    image.temperature=numberKey(f.file,"CCD-TEMP",std::numeric_limits<double>::quiet_NaN()); image.hasTemperature=std::isfinite(image.temperature);
    image.imageType=stringKey(f.file,"IMAGETYP"); image.filter=stringKey(f.file,"FILTER"); image.object=stringKey(f.file,"OBJECT"); image.date=stringKey(f.file,"DATE-OBS");
    const auto pixels=std::size_t(image.width)*image.height;
    std::vector<float> planar(pixels*channels);
    float nullValue=std::numeric_limits<float>::quiet_NaN(); int anyNull=0;
    fits_read_img(f.file,TFLOAT,1,LONGLONG(planar.size()),&nullValue,planar.data(),&anyNull,&status); fail(status);
    double scale=inputScale;
    if(!(scale>0)) {
        scale=numberKey(f.file,"AC_SCALE",0);
        if(!(scale>0)) {
            // FITS floats may contain normalized samples or CCD ADUs. Do not
            // normalize each frame by its maximum: that changes photometry.
            const auto maximum=*std::max_element(planar.begin(),planar.end(),[](float a,float b){return (!std::isfinite(a)?-INFINITY:a)<(!std::isfinite(b)?-INFINITY:b);});
            scale=bitpix==8?255:bitpix==16?65535:maximum>2?65535:1;
        }
    }
    image.scale=scale; image.samples.resize(planar.size());
    for(std::size_t p=0;p<pixels;++p)
        for(int c=0;c<channels;++c)image.samples[p*channels+c]=float(planar[p+c*pixels]/scale);
    return image;
}
AstroImage readRaw(const std::filesystem::path& path,double inputScale) {
    // LibRaw is larger than a macOS worker stack; keep it on the heap even
    // when optimized builds inline this function into the FITS dispatch.
    auto decoder=std::make_unique<LibRaw>();
    int code=decoder->open_file(path.string().c_str());
    if(!code)code=decoder->unpack();
    if(code)throw std::runtime_error(std::string("RAW: ")+libraw_strerror(code));
    auto& data=decoder->imgdata;
    if(!data.rawdata.raw_image || !data.idata.filters || data.idata.filters==9)
        throw std::runtime_error("This RAW sensor layout is not supported (a Bayer mosaic is required).");
    AstroImage image;
    image.width=data.sizes.width; image.height=data.sizes.height; image.bits=16;
    validate(image.width,image.height,1);
    std::string tile;
    for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
        const int c=decoder->COLOR(y+data.sizes.top_margin,x+data.sizes.left_margin);
        tile+=c==0?'R':c==2?'B':'G';
    }
    image.bayer=pattern(tile);
    if(image.bayer==domain::BayerPattern::None)throw std::runtime_error("Unsupported RAW Bayer pattern.");
    image.exposure=data.other.shutter;
    image.scale=inputScale>0?inputScale:double(data.color.maximum?data.color.maximum:65535);
    image.samples.resize(std::size_t(image.width)*image.height);
    const int stride=data.sizes.raw_pitch/2;
    for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x)
        image.samples[std::size_t(y)*image.width+x]=float(data.rawdata.raw_image[(y+data.sizes.top_margin)*stride+x+data.sizes.left_margin]/image.scale);
    return image;
}
}
AstroImage readAstroImage(const std::filesystem::path& path,double inputScale) {
    auto suffix=path.extension().string();
    std::transform(suffix.begin(),suffix.end(),suffix.begin(),[](unsigned char c){return std::tolower(c);});
    if(suffix==".fits"||suffix==".fit"||suffix==".fts"||suffix==".fz")return readFits(path,inputScale);
    return readRaw(path,inputScale);
}
void writeAstroFits(const std::filesystem::path& path,const AstroImage& image) {
    validate(image.width,image.height,image.channels);
    const auto pixels=std::size_t(image.width)*image.height;
    if(image.samples.size()!=pixels*image.channels)throw std::runtime_error("Invalid FITS sample count.");
    Fits f; int status=0;
    fits_create_diskfile(&f.file,path.string().c_str(),&status); fail(status); // never overwrite an input
    long axes[3]{image.width,image.height,image.channels};
    fits_create_img(f.file,FLOAT_IMG,image.channels==1?2:3,axes,&status); fail(status);
    auto key=[&](const char* name,double value){fits_update_key(f.file,TDOUBLE,name,&value,nullptr,&status);};
    auto text=[&](const char* name,const std::string& value){if(!value.empty())fits_update_key(f.file,TSTRING,name,const_cast<char*>(value.c_str()),nullptr,&status);};
    key("AC_SCALE",1); key("EXPTIME",image.exposure);
    if(image.hasGain)key("GAIN",image.gain);
    if(image.hasTemperature)key("CCD-TEMP",image.temperature);
    text("BAYERPAT",image.bayer==domain::BayerPattern::None?"":std::string(domain::to_string(image.bayer)));
    text("IMAGETYP",image.imageType); text("FILTER",image.filter); text("OBJECT",image.object); text("DATE-OBS",image.date);
    std::vector<float> planar(image.samples.size());
    for(std::size_t p=0;p<pixels;++p)for(int c=0;c<image.channels;++c)planar[p+c*pixels]=image.samples[p*image.channels+c];
    fits_write_img(f.file,TFLOAT,1,LONGLONG(planar.size()),planar.data(),&status);
    fits_write_chksum(f.file,&status); fail(status);
    fits_close_file(f.file,&status); f.file=nullptr; fail(status);
}
AstroImage imageFromFrame(const domain::FrameBuffer& frame) {
    const auto& d=frame.descriptor;
    if(!frame.storage||frame.storage->size()<d.expectedPayloadBytes())throw std::runtime_error("Incomplete camera frame.");
    AstroImage image; image.width=int(d.width); image.height=int(d.height); image.bits=d.validBits;
    image.bayer=d.bayer;
    const bool wide=d.storage==domain::PixelStorage::Mono16||d.storage==domain::PixelStorage::Bayer16;
    const bool rgb=d.storage==domain::PixelStorage::RGB24||d.storage==domain::PixelStorage::BGR24||d.storage==domain::PixelStorage::BGRA32;
    if(!wide&&!rgb&&d.storage!=domain::PixelStorage::Mono8&&d.storage!=domain::PixelStorage::Bayer8)throw std::runtime_error("Unsupported camera pixel format.");
    image.channels=rgb?3:1; validate(image.width,image.height,image.channels);
    image.scale=double((std::uint64_t(1)<<std::clamp(int(d.validBits),1,16))-1);
    if(rgb)image.scale=255;
    image.samples.resize(std::size_t(image.width)*image.height*image.channels);
    const auto stride=d.strideBytes?d.strideBytes:d.width*domain::bytesPerPixel(d.storage);
    const auto* bytes=reinterpret_cast<const unsigned char*>(frame.storage->data());
    for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x) {
        const auto* p=bytes+y*stride+x*domain::bytesPerPixel(d.storage);
        const auto at=(std::size_t(y)*image.width+x)*image.channels;
        if(wide){std::uint16_t value;std::memcpy(&value,p,2);image.samples[at]=float(value/image.scale);}
        else if(!rgb)image.samples[at]=float(*p/image.scale);
        else {const bool reverse=d.storage!=domain::PixelStorage::RGB24;for(int c=0;c<3;++c)image.samples[at+c]=p[reverse?2-c:c]/255.f;}
    }
    return image;
}
}
