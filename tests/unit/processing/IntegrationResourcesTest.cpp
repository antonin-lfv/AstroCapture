#include <astro/processing/IntegrationResources.hpp>
#include "../../../core/processing/src/WorkingFiles.hpp"
#include "Check.hpp"
#include <limits>
#include <cmath>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif
using namespace astro::processing;
template<class F> void fails(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}ASTRO_TEST_CHECK(failed);}
int main() {
    IntegrationResourceRequest request{6244,4170,3,12};
    const auto normal=estimateIntegrationResources(request);
    ASTRO_TEST_CHECK(normal.temporaryBytes==6244ULL*4170*12*12);
    ASTRO_TEST_CHECK(normal.resultBytes==6244ULL*4170*12);
    ASTRO_TEST_CHECK(normal.stripBytes<=64ULL*1024*1024 && normal.width==6244 && normal.height==4170);
    request.drizzle=true;request.scale=2;
    fails([&]{estimateIntegrationResources(request);}); // No silent lift of downstream 32 MP limits.
    request.scale=1;const auto rgb=estimateIntegrationResources(request);
    ASTRO_TEST_CHECK(rgb.temporaryBytes==6244ULL*4170*12*16);
    request.channels=1;request.nativeBayer=true;request.count=500;
    const auto bayer=estimateIntegrationResources(request);
    ASTRO_TEST_CHECK(bayer.temporaryBytes==6244ULL*4170*500*24);
    ASTRO_TEST_CHECK(bayer.stripColumns<6244 && bayer.stripRows==1 && bayer.stripBytes<=64ULL*1024*1024);
    request.diagnosticMaps=true;const auto maps=estimateIntegrationResources(request);
    ASTRO_TEST_CHECK(maps.resultBytes==6244ULL*4170*48 && maps.workingBytes>=bayer.workingBytes);
    for(int width:{1,64,6000,100000})for(int count:{1,12,500})for(int weights:{0,1,3}) {
        const auto columns=integrationStripColumns(width,count,weights,65536);
        const auto rows=integrationStripRows(columns,count,weights,65536);
        ASTRO_TEST_CHECK(columns>=1 && columns<=width && rows>=1);
        ASTRO_TEST_CHECK(std::uint64_t(columns)*rows*count*(3+weights)*4<=65536);
    }
    fails([&]{integrationStripColumns(6000,500,3,100);});
    fails([&]{integrationStripColumns(-1,500,3,100);});
    fails([&]{integrationStripColumns(1,501,3,100);});
    request.scale=std::numeric_limits<double>::quiet_NaN();fails([&]{estimateIntegrationResources(request);});
    request.scale=1;request.calibrationBytes=std::numeric_limits<std::uint64_t>::max();fails([&]{estimateIntegrationResources(request);});
    const auto directory=std::filesystem::temp_directory_path()/("astrocapture-resources-test-"+std::to_string(std::random_device{}()));
    ASTRO_TEST_CHECK(std::filesystem::create_directory(directory));
    const std::vector<float> samples{-2,0,.25f,10,100};
    {
        WorkingFiles archive(directory,128,0);
        const auto first=archive.write(samples,nullptr,{}),second=archive.write(samples,&samples,{});
        std::vector<float> read(5);archive.read(read.data(),read.size(),first);ASTRO_TEST_CHECK(read==samples);
        archive.read(read.data(),read.size(),second+samples.size()*4);ASTRO_TEST_CHECK(read==samples);
        fails([&]{archive.read(read.data(),read.size(),second+samples.size()*8);});
    }
    ASTRO_TEST_CHECK(std::filesystem::is_empty(directory));
    fails([&]{WorkingFiles archive(directory,128,0);archive.write(samples,nullptr,[]{throw std::runtime_error("cancel");});});
    ASTRO_TEST_CHECK(std::filesystem::is_empty(directory));
#ifndef _WIN32
    const auto child=fork();ASTRO_TEST_CHECK(child>=0);
    if(child==0){WorkingFiles archive(directory,1024*1024,0);std::vector<float> data(262144,1);archive.write(data,nullptr,{});_exit(0);}
    int status=0;ASTRO_TEST_CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    ASTRO_TEST_CHECK(std::filesystem::is_empty(directory)); // No C++ destructor ran in the child.
#endif
    ASTRO_TEST_CHECK(std::filesystem::remove(directory));
}
