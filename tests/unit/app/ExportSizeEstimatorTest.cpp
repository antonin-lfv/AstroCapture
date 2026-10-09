#include "ExportSizeEstimator.hpp"
#include "ExportAnnotation.hpp"
#include "Check.hpp"
#include <QGuiApplication>
#include <QBuffer>
#include <QColorSpace>
#include <QElapsedTimer>
#include <QImageWriter>
#include <QThread>
#include <cmath>

using namespace astro;
static void until(const std::function<bool()>& ready) {
    QElapsedTimer timer;timer.start();
    while(!ready() && timer.elapsed()<10000){QCoreApplication::processEvents();QThread::msleep(1);}
    ASTRO_TEST_CHECK(ready());
}
static qint64 encodedSize(const processing::FloatImage& pixels,const QString& format) {
    QImage image(pixels.width,pixels.height,QImage::Format_RGBA64);image.setColorSpace(QColorSpace::SRgb);
    for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x) {
        const auto at=(std::size_t(y)*pixels.width+x)*3;
        reinterpret_cast<QRgba64*>(image.scanLine(y))[x]=QRgba64::fromRgba64(app::annotatedSample(pixels.rgb[at],0,0),app::annotatedSample(pixels.rgb[at+1],0,1),app::annotatedSample(pixels.rgb[at+2],0,2),65535);
    }
    if(format=="jpg")image=image.convertToFormat(QImage::Format_RGB888);
    QBuffer buffer;ASTRO_TEST_CHECK(buffer.open(QIODevice::WriteOnly));QImageWriter writer(&buffer,format=="png"?"PNG":"JPEG");
    if(format=="jpg")writer.setQuality(95);
    ASTRO_TEST_CHECK(writer.write(image));return buffer.size();
}
int main(int argc,char** argv) {
    QGuiApplication application(argc,argv);
    app::ExportSizeEstimator estimate;
    estimate.setActive(true);ASTRO_TEST_CHECK(estimate.bytes()<0 && !estimate.calculating());
    for(int kind=0;kind<3;++kind) {
        auto pixels=std::make_shared<processing::FloatImage>();pixels->width=2048;pixels->height=1536;pixels->rgb.resize(std::size_t(pixels->width)*pixels->height*3);
        std::uint32_t random=0x12345678;
        for(int y=0;y<pixels->height;++y)for(int x=0;x<pixels->width;++x)for(int c=0;c<3;++c) {
            random^=random<<13;random^=random>>17;random^=random<<5;
            pixels->rgb[(std::size_t(y)*pixels->width+x)*3+c]=kind==0?.25f:kind==1?float((x+y+c*100)%4096)/4095:float(random&65535)/65535;
        }
        estimate.setInput(QVariant::fromValue(app::ExportSizeInput{pixels,{}}));
        for(const QString format:{QString("png"),QString("jpg")}) {
            estimate.setFormat(format);until([&]{return estimate.bytes()>=0 && !estimate.calculating();});
            ASTRO_TEST_CHECK(!estimate.exact());
            const double actual=encodedSize(*pixels,format);
            ASTRO_TEST_CHECK(std::abs(estimate.bytes()-actual)<=std::max(1000000.,actual*.25));
        }
        // Changing the format while an estimate is pending must retain the new result.
        estimate.setFormat("png");estimate.setFormat("tiff");
        until([&]{return estimate.bytes()>=0 && !estimate.calculating();});
        ASTRO_TEST_CHECK(estimate.exact() && estimate.bytes()==double(std::uint64_t(pixels->width)*pixels->height*6+192));
    }
    estimate.setInput({});ASTRO_TEST_CHECK(estimate.bytes()<0 && !estimate.calculating());
}
