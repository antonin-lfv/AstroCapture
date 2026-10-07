#include <astro/processing/Mosaic.hpp>
#include <astro/processing/Surface.hpp>
#include <random>
#include <cmath>
#include <iostream>
#include "Check.hpp"
using namespace astro::processing;
FloatImage texture(int w,int h,int seed=42) {
    std::mt19937 random(seed); std::uniform_real_distribution<float> noise(.05,.95);
    std::vector<float> gray(w*h); for(auto& v:gray)v=noise(random);
    for(int pass=0;pass<4;++pass) {
        auto old=gray;
        for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x) gray[y*w+x]=(old[y*w+x]*4+old[y*w+x-1]+old[y*w+x+1]+old[(y-1)*w+x]+old[(y+1)*w+x])/8;
    }
    FloatImage out{w,h,std::vector<float>(w*h*3)};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c)
        out.rgb[(y*w+x)*3+c]=std::clamp((gray[y*w+x]-.5f)*3+.45f+float(.12*std::sin(x*.04)*std::cos(y*.05)),.01f,.95f);
    return out;
}
float sample(const FloatImage& image,double x,double y,int c) {
    x=std::clamp(x,0.,double(image.width-1));y=std::clamp(y,0.,double(image.height-1));
    const int x0=int(x),y0=int(y),x1=std::min(x0+1,image.width-1),y1=std::min(y0+1,image.height-1);
    const double fx=x-x0,fy=y-y0;
    auto at=[&](int xx,int yy){return image.rgb[(yy*image.width+xx)*3+c];};
    return float((at(x0,y0)*(1-fx)+at(x1,y0)*fx)*(1-fy)+(at(x0,y1)*(1-fx)+at(x1,y1)*fx)*fy);
}
int main() {
    const auto base=texture(700,550);
    {
        auto warped=base;
        for(int y=0;y<base.height;++y)for(int x=0;x<base.width;++x)for(int c=0;c<3;++c)
            warped.rgb[(y*base.width+x)*3+c]=sample(base,x-3*std::sin(y*.015),y,c);
        const auto aligned=SurfaceReference(base,64).align(warped,{0,0,1});
        ASTRO_TEST_CHECK(aligned.tracked>20);
        double before=0,after=0;
        for(int y=40;y<base.height-40;++y)for(int x=40;x<base.width-40;++x) {
            const auto p=(y*base.width+x)*3;
            before+=std::pow(warped.rgb[p]-base.rgb[p],2); after+=std::pow(aligned.image.rgb[p]-base.rgb[p],2);
        }
        std::cout<<"Surface squared error ratio: "<<after/before<<"\n";
        ASTRO_TEST_CHECK(after<before*.65);
        auto flat=base; std::fill(flat.rgb.begin(),flat.rgb.end(),.3f);
        const auto fallback=SurfaceReference(flat).align(flat,{0,0,1});
        ASTRO_TEST_CHECK(fallback.tracked==0 && fallback.image.rgb==flat.rgb);
    }
    {
        const auto truth=texture(320,256,73);
        auto blurred=truth;
        for(int y=0;y<truth.height;++y)for(int x=0;x<truth.width;++x)for(int c=0;c<3;++c) {
            float v=0;for(int yy=-3;yy<=3;++yy)for(int xx=-3;xx<=3;++xx)v+=sample(truth,x+xx,y+yy,c)/49.f;
            blurred.rgb[(y*truth.width+x)*3+c]=v;
        }
        auto left=truth,right=truth;
        for(int y=0;y<truth.height;++y)for(int x=0;x<truth.width;++x)for(int c=0;c<3;++c) {
            const auto at=(y*truth.width+x)*3+c;
            if(x>=160)left.rgb[at]=blurred.rgb[at]; else right.rgb[at]=blurred.rgb[at];
        }
        std::vector<FloatImage> views{left,right,blurred};
        const auto selected=stackLocalSelection({0,1,2},1,[&](int i){return views[i];},64);
        std::cout<<"Local counts: "<<selected.usedFrames<<" used, "<<selected.rejectedFrames<<" rejected\n";
        ASTRO_TEST_CHECK(selected.usedFrames>=2 && selected.framesPerArea==1);
        ASTRO_TEST_CHECK(selected.image.width>=truth.width-8 && selected.image.height>=truth.height-8);
        double globalError=0,localError=0;
        for(int y=50;y<206;++y)for(int x=50;x<270;++x)if(std::abs(x-160)>48) {
            const auto at=(y*truth.width+x)*3;
            globalError+=std::pow(left.rgb[at]-truth.rgb[at],2);
            localError+=std::pow(selected.image.rgb[((y-selected.top)*selected.image.width+x-selected.left)*3]-truth.rgb[at],2);
        }
        std::cout<<"Local selection error ratio: "<<localError/globalError<<"\n";
        ASTRO_TEST_CHECK(localError<globalError*.3);
        bool stopped=false;try {stackLocalSelection({0,1},1,[&](int i){return views[i];},64,[]{throw std::runtime_error("cancel");});}catch(...){stopped=true;}
        ASTRO_TEST_CHECK(stopped);
    }
    std::vector<MosaicTile> tiles;
    for(int n=0;n<3;++n) {
        const double angle=n==2?.035:0,dx=n==0?0:n==1?180:90,dy=n==2?160:0,gain=n==1?.8:n==2?1.15:1;
        MosaicTile tile;tile.image={400,300,std::vector<float>(400*300*3)};
        for(int y=0;y<300;++y)for(int x=0;x<400;++x)for(int c=0;c<3;++c)
            tile.image.rgb[(y*400+x)*3+c]=float(gain)*sample(base,dx+x*std::cos(angle)-y*std::sin(angle),dy+x*std::sin(angle)+y*std::cos(angle),c);
        tiles.push_back(std::move(tile));
    }
    const std::vector<MosaicSource> sources{{"left",400,300},{"right",400,300},{"bottom",400,300}};
    const auto mosaic=buildMosaic(sources,[&](int i){return tiles[i];});
    std::cout<<"Mosaic: "<<mosaic.image.width<<" x "<<mosaic.image.height<<", residual "<<mosaic.residual<<"\n";
    ASTRO_TEST_CHECK(mosaic.pairs>=2 && mosaic.residual<1 && mosaic.image.width>=575 && mosaic.image.width<=585 && mosaic.image.height>=460 && mosaic.image.height<=480);
    // Registration and brightness compensation are checked against the original
    // field, not against a second run of the same algorithm.
    double error=0;int count=0;
    // The canvas may add a one-pixel border due to subpixel transforms.
    double bestError=1e9;
    for(int ox=0;ox<=2;++ox)for(int oy=0;oy<=2;++oy) {
        error=0;count=0;
        for(int y=30;y<430;++y)for(int x=120;x<440;++x) {
            error+=std::pow(mosaic.image.rgb[((y+oy)*mosaic.image.width+x+ox)*3]-base.rgb[(y*base.width+x)*3],2); ++count;
        }
        bestError=std::min(bestError,error/count);
    }
    std::cout<<"Mosaic RMS: "<<std::sqrt(bestError)<<"\n";
    ASTRO_TEST_CHECK(std::sqrt(bestError)<.05);
    bool cancelled=false;try { buildMosaic(sources,[&](int i){return tiles[i];},true,[]{throw std::runtime_error("cancel");}); }catch(...){cancelled=true;}
    ASTRO_TEST_CHECK(cancelled);
    const auto clean=buildMosaic(sources,[&](int i){return tiles[i];},true,{}, {},true,true);
    ASTRO_TEST_CHECK(clean.edgeMask.size()==std::size_t(clean.image.width)*clean.image.height);
    ASTRO_TEST_CHECK(clean.usedIndices==std::vector<int>({0,1,2}) && clean.excludedIndices.empty());
    int protectedPixels=0, interiorPixels=0;
    for(std::size_t p=0;p<clean.edgeMask.size();++p) {
        const float w=clean.edgeMask[p]; ASTRO_TEST_CHECK(w>=0 && w<=1);
        if(w==0) {++protectedPixels;ASTRO_TEST_CHECK(clean.image.rgb[p*3]==0);}
        if(w==1)++interiorPixels;
    }
    ASTRO_TEST_CHECK(protectedPixels>0 && interiorPixels>clean.edgeMask.size()/2);
    tiles[1].image=texture(400,300,999);
    bool rejected=false;try {buildMosaic(sources,[&](int i){return tiles[i];});}catch(...){rejected=true;}
    ASTRO_TEST_CHECK(rejected);
    const auto partial=buildMosaic(sources,[&](int i){return tiles[i];},true,{}, {},true);
    ASTRO_TEST_CHECK(partial.usedIndices==std::vector<int>({0,2}) && partial.excludedIndices==std::vector<int>({1}));
    auto reorderedSources=sources;std::swap(reorderedSources[0],reorderedSources[1]);
    const auto reordered=buildMosaic(reorderedSources,[&](int i){return tiles[i==0?1:i==1?0:2];},true,{}, {},true);
    ASTRO_TEST_CHECK(reordered.usedIndices==std::vector<int>({1,2}) && reordered.excludedIndices==std::vector<int>({0}));
    std::vector<MosaicSource> blankSources(26,{"blank",64,64});
    rejected=false;try {buildMosaic(blankSources,[](int){return MosaicTile{{64,64,std::vector<float>(64*64*3,.3f)}, {}};},true,{}, {},true);}catch(const std::exception& e){rejected=std::string(e.what()).find("No pair")!=std::string::npos;}
    ASTRO_TEST_CHECK(rejected);
    rejected=false;try {wavelets(base,{50.01f,0,0,0,0,0},0,{1,1,1},1);}catch(...){rejected=true;}
    ASTRO_TEST_CHECK(rejected);
    const auto full=wavelets(base,{50,20,20,10,10,10},0,{1,1,1},1);
    ASTRO_TEST_CHECK(full.rgb.size()==base.rgb.size());
    rejected=false;try {wavelets(base,{0,0,0,10.01f,0,0},0,{1,1,1},1);}catch(...){rejected=true;}
    ASTRO_TEST_CHECK(rejected);
}
