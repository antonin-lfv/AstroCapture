#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QImage>
#include <QImageReader>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <random>
#include "MosaicController.hpp"
#include "Check.hpp"
void wait(astro::app::MosaicController& c,int timeout=60000) {
    QElapsedTimer timer;timer.start();
    while(c.busy()&&timer.elapsed()<timeout){QCoreApplication::processEvents();QThread::msleep(5);}
    ASTRO_TEST_CHECK(!c.busy());
}
int main(int argc,char** argv) {
    QGuiApplication application(argc,argv);
    astro::app::MosaicController controller;
    if(argc==3) { // Opt-in local validation; never used by CTest.
        QList<QUrl> urls;
        for(const auto& name:QDir(QString::fromLocal8Bit(argv[1])).entryList({"*.png","*.tif","*.tiff","*.jpg"},QDir::Files,QDir::Name))
            urls.append(QUrl::fromLocalFile(QDir(QString::fromLocal8Bit(argv[1])).filePath(name)));
        int last=-1;
        QObject::connect(&controller,&astro::app::MosaicController::changed,[&]{if(controller.progress()!=last){last=controller.progress();std::cout<<last<<"% "<<controller.status().toStdString()<<std::endl;}});
        controller.addImages(urls);wait(controller);controller.assemble();wait(controller,1200000);
        std::cout<<controller.status().toStdString()<<std::endl;
        if(!controller.hasResult())return 1;
        controller.exportImage(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2])));wait(controller);
        std::cout<<controller.status().toStdString()<<std::endl;
        return QFileInfo::exists(QString::fromLocal8Bit(argv[2]))?0:1;
    }
    QTemporaryDir temp;
    QImage base(640,320,QImage::Format_RGB32);
    std::mt19937 random(91);
    // Distinct textured blocks at multiple scales produce stable feature points.
    for(int y=0;y<320;++y)for(int x=0;x<640;++x){const int v=30+int(random()%190);base.setPixel(x,y,qRgb(v,v,v));}
    base=base.scaled(160,80,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).scaled(640,320,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    const auto a=temp.filePath("A.png"),b=temp.filePath("B.png");
    ASTRO_TEST_CHECK(base.copy(0,0,420,320).save(a));ASTRO_TEST_CHECK(base.copy(220,0,420,320).save(b));
    int fileChanges=0; QObject::connect(&controller,&astro::app::MosaicController::filesChanged,[&]{++fileChanges;});
    controller.addImages({QUrl::fromLocalFile(a),QUrl::fromLocalFile(b),QUrl::fromLocalFile(a)});wait(controller);
    ASTRO_TEST_CHECK(controller.files().size()==2);
    const auto beforeCancelledImport=controller.files();
    controller.addImages({QUrl::fromLocalFile(a)});ASTRO_TEST_CHECK(controller.busy());controller.cancel();wait(controller);
    ASTRO_TEST_CHECK(controller.files()==beforeCancelledImport);
    controller.setImageSelected(0,false);ASTRO_TEST_CHECK(controller.selectedCount()==1 && controller.files().size()==2);
    controller.assemble();ASTRO_TEST_CHECK(!controller.busy() && !controller.hasResult());
    controller.setImageSelected(0,true);fileChanges=1;
    controller.assemble();wait(controller);
    std::cout<<controller.status().toStdString()<<std::endl;
    ASTRO_TEST_CHECK(controller.hasResult() && controller.imageWidth()>=637 && controller.imageWidth()<=645);
    ASTRO_TEST_CHECK(fileChanges==1); // Progress must not recreate the thumbnail model.
    const auto plainDocument=controller.processingDocument().value<astro::app::ImageDocument>();
    ASTRO_TEST_CHECK(plainDocument.pixels && plainDocument.preview==controller.image());
    const auto extra=temp.filePath("cancelled-input.png");ASTRO_TEST_CHECK(base.copy(0,0,420,320).save(extra));
    controller.addImages({QUrl::fromLocalFile(extra)});controller.cancel();wait(controller);
    ASTRO_TEST_CHECK(controller.files().size()==2&&controller.processingDocument().value<astro::app::ImageDocument>().pixels==plainDocument.pixels);
    ASTRO_TEST_CHECK(plainDocument.mosaicImageCount==2);
    ASTRO_TEST_CHECK(!controller.captionTechnicalText().isEmpty() && !controller.exportAnnotation().contains("technicalText"));
    controller.setExportAnnotation({{"enabled",true},{"technicalText","Mosaïque · filtre solaire Hα"}});
    ASTRO_TEST_CHECK(controller.captionTechnicalText()=="Mosaïque · filtre solaire Hα");
    ASTRO_TEST_CHECK(controller.processingDocument().value<astro::app::ImageDocument>().annotation.value("technicalText")=="Mosaïque · filtre solaire Hα");
    controller.setExportAnnotation({{"enabled",true},{"technicalText",""}});
    ASTRO_TEST_CHECK(controller.captionTechnicalText().isEmpty() && controller.annotationImage().isNull());
    controller.setExportAnnotation({{"enabled",true},{"technical",false},{"title","Lune · été"}});
    const auto titleOnly=controller.annotationImage();
    ASTRO_TEST_CHECK(!titleOnly.isNull());
    int titleBottom=0;
    for(int y=0;y<titleOnly.height();++y)for(int x=0;x<titleOnly.width();++x)if(qAlpha(titleOnly.pixel(x,y)))titleBottom=y;
    controller.setExportAnnotation({{"enabled",true},{"technical",false},{"title","Lune · été"},{"top","30 mai 2023"}});
    const auto titled=controller.annotationImage();int bodyBottom=0;
    for(int y=0;y<titled.height();++y)for(int x=0;x<titled.width();++x) {
        if(qAlpha(titled.pixel(x,y))) bodyBottom=y;
        if(y<=titleBottom) ASTRO_TEST_CHECK(titled.pixel(x,y)==titleOnly.pixel(x,y));
    }
    ASTRO_TEST_CHECK(bodyBottom>titleBottom);
    const auto captionDocument=controller.processingDocument().value<astro::app::ImageDocument>();
    ASTRO_TEST_CHECK(captionDocument.pixels==plainDocument.pixels && captionDocument.preview==plainDocument.preview);
    ASTRO_TEST_CHECK(captionDocument.annotation.value("title")=="Lune · été" && captionDocument.protectedPaths.contains(QFileInfo(a).canonicalFilePath()));
    int exports=0;QObject::connect(&controller,&astro::app::MosaicController::imageExported,[&]{++exports;});
    const auto out=temp.filePath("mosaic.tif"); controller.exportImage(QUrl::fromLocalFile(out));ASTRO_TEST_CHECK(controller.exporting());wait(controller);ASTRO_TEST_CHECK(!controller.exporting());
    ASTRO_TEST_CHECK(exports==1 && QFileInfo(out).size()>1000000);
    if(QImageReader::supportedImageFormats().contains("tiff")){QImageReader reader(out);ASTRO_TEST_CHECK(reader.read().size()==QSize(controller.imageWidth(),controller.imageHeight()));}
    controller.exportImage(QUrl::fromLocalFile(temp.filePath("missing/mosaic.tif")));wait(controller);ASTRO_TEST_CHECK(exports==1&&!controller.exporting());
    const auto old=controller.image();controller.assemble();controller.cancel();wait(controller);ASTRO_TEST_CHECK(controller.image()==old);
    controller.removeImage(0);ASTRO_TEST_CHECK(!controller.hasResult()&&controller.files().size()==1&&QFileInfo::exists(a));
    controller.clear();ASTRO_TEST_CHECK(controller.files().isEmpty());
    const auto blankPath=temp.filePath("blank.png");QImage blank(420,320,QImage::Format_RGB32);blank.fill(Qt::gray);ASTRO_TEST_CHECK(blank.save(blankPath));
    controller.addImages({QUrl::fromLocalFile(blankPath),QUrl::fromLocalFile(a),QUrl::fromLocalFile(b)});wait(controller);
    controller.assemble();wait(controller);
    ASTRO_TEST_CHECK(controller.hasResult() && controller.files().size()==3 && controller.selectedCount()==2 && controller.excludedFiles()==QStringList{"blank.png"});
    const auto cleaned=controller.processingDocument().value<astro::app::ImageDocument>();
    ASTRO_TEST_CHECK(cleaned.mosaicImageCount==2); // Count assembled images, excluding the rejected tile.
    ASTRO_TEST_CHECK(cleaned.edgeMask && cleaned.edgeMask->size()==std::size_t(controller.imageWidth())*controller.imageHeight());
    ASTRO_TEST_CHECK(cleaned.protectedPaths.contains(QFileInfo(blankPath).canonicalFilePath()));
    ASTRO_TEST_CHECK(QFileInfo::exists(blankPath));
    controller.restoreExcluded();ASTRO_TEST_CHECK(controller.files().size()==3 && controller.selectedCount()==3 && controller.excludedFiles().isEmpty() && !controller.hasResult());
    controller.clear();
    const auto blank2=temp.filePath("blank2.png");ASTRO_TEST_CHECK(blank.save(blank2));
    controller.addImages({QUrl::fromLocalFile(blankPath),QUrl::fromLocalFile(blank2)});wait(controller);controller.assemble();wait(controller);
    ASTRO_TEST_CHECK(!controller.hasResult() && controller.files().size()==2 && controller.status().contains("No pair"));
}
