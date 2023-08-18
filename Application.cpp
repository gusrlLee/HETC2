#include <future>
#include <stdio.h>
#include <limits>
#include <math.h>
#include <memory>
#include <string.h>
#include <iostream>
#include <io.h> // file handle
#include <direct.h> // for make directory
#include <list>
#include <sys/stat.h>
#include <queue>

#include "ErrorBlockData.h" // for pipeline

#ifdef _MSC_VER
#  include "getopt/getopt.h"
#else
#  include <unistd.h>
#  include <getopt.h>
#endif

#include "Bitmap.hpp"
#include "BlockData.hpp"
#include "DataProvider.hpp"
#include "Debug.hpp"
#include "Error.hpp"
#include "System.hpp"
#include "TaskDispatch.hpp"
#include "Timing.hpp"

#include "BlockDataGPU.hpp" // for GPU encoding

template <typename T>
void saveToOffData(T& encoder, const char* file_path)
{
    betsy::CpuImage downloadImg;
    encoder.startDownload();
    encoder.downloadTo(downloadImg);
    betsy::FormatKTX::save(file_path, downloadImg);
    downloadImg.data = 0;
}


struct DebugCallback_t : public DebugLog::Callback
{
    void OnDebugMessage( const char* msg ) override
    {
        fprintf( stderr, "%s\n", msg );
    }
} DebugCallback;

void Usage()
{
    fprintf( stderr, "Usage: etcpak [options] input.png {output.pvr}\n" );
    fprintf( stderr, "  Options:\n" );
    fprintf( stderr, "  -v                     view mode (loads pvr/ktx file, decodes it and saves to png)\n" );
    fprintf( stderr, "  -s                     display image quality measurements\n" );
    fprintf( stderr, "  -b                     benchmark mode\n" );
    fprintf( stderr, "  -M                     switch benchmark to multi-threaded mode\n" );
    fprintf( stderr, "  -m                     generate mipmaps\n" );
    fprintf( stderr, "  -d                     enable dithering\n" );
    fprintf( stderr, "  -a alpha.pvr           save alpha channel in a separate file\n" );
    fprintf( stderr, "  --etc1                 use ETC1 mode (ETC2 is used by default)\n" );
    fprintf( stderr, "  --rgba                 enable RGBA in ETC2 mode (RGB is used by default)\n" );
    fprintf( stderr, "  --disable-heuristics   disable heuristic selector of compression mode\n" );
    fprintf( stderr, "  --dxtc                 use DXT1/DXT5 compression\n" );
    fprintf( stderr, "  --linear               input data is in linear space (disable sRGB conversion for mips)\n" );
    fprintf( stderr, "  --betsy-mode           execute BetsyGPU Encoding (with Multi-threading Option)\n");
    fprintf( stderr, "  --fast                 Quality Option Fast mode   (with betsy-mode Option)\n");
    fprintf( stderr, "  --normal               Qualtiy Option Normal mode (default) (with betsy-mode Option)\n");
    fprintf( stderr, "  --best                 Qualtiy Option Best mode   (with betsy-mode Option)\n\n");
    fprintf( stderr, "Output file name may be unneeded for some modes.\n" );
}

int isFileOrDir(_finddata_t fd)
{

    if (fd.attrib & _A_SUBDIR)
        return 0;
    else
        return 1;

}

int main( int argc, char** argv )
{
    DebugLog::AddCallback( &DebugCallback );

    bool viewMode = false;
    bool stats = false;
    bool benchmark = false;
    bool benchMt = false;
    bool mipmap = false;
    bool dither = false;
    bool etc2 = true;
    bool rgba = false;
    bool dxtc = false;
    bool linearize = true;
    bool useHeuristics = true;
    bool useBetsy = false; // for betsy mode (hyeon add)
    bool isTargetDir = false;
    const char* alpha = nullptr;
    unsigned int cpus = System::CPUCores();

    bool modeBest = false;
    bool modeNormal = true; // (default Normal mode)
    bool modeFast = false;
    float qualityRatio = 0.4; // 70 % 

    if( argc < 3 )
    {
        Usage();
        return 1;
    }

    enum Options
    {
        OptEtc1,
        OptRgba,
        OptDxtc,
        OptLinear,
        OptNoHeuristics,
        OptBetsyMode,
        OptQualityFast,
        OptQualityNormal,
        OptQualityBest
    };

    struct option longopts[] = {
        { "etc1", no_argument, nullptr, OptEtc1 },
        { "rgba", no_argument, nullptr, OptRgba },
        { "dxtc", no_argument, nullptr, OptDxtc },
        { "linear", no_argument, nullptr, OptLinear },
        { "disable-heuristics", no_argument, nullptr, OptNoHeuristics },
        { "betsy-mode", no_argument, nullptr, OptBetsyMode },
        { "fast", no_argument, nullptr, OptQualityFast },
        { "normal", no_argument, nullptr, OptQualityNormal },
        { "best", no_argument, nullptr, OptQualityBest },
        {}
    };

    int c;
    while( ( c = getopt_long( argc, argv, "vo:a:sbMmd", longopts, nullptr ) ) != -1 )
    {
        switch( c )
        {
        case '?':
            Usage();
            return 1;
        case 'v':
            viewMode = true;
            break;
        case 'a':
            alpha = optarg;
            break;
        case 's':
            stats = true;
            break;
        case 'b':
            benchmark = true;
            break;
        case 'M':
            benchMt = true;
            break;
        case 'm':
            mipmap = true;
            break;
        case 'd':
            dither = true;
            break;
        case OptEtc1:
            etc2 = false;
            break;
        case OptRgba:
            rgba = true;
            etc2 = true;
            break;
        case OptDxtc:
            etc2 = false;
            dxtc = true;
            break;
        case OptLinear:
            linearize = false;
            break;
        case OptNoHeuristics:
            useHeuristics = false;
        case OptBetsyMode:
            useBetsy = true;
            break;
        case OptQualityFast:
            modeFast = true;
            modeNormal = false;
            break;
        case OptQualityNormal:
            modeNormal = true;
            break;
        case OptQualityBest:
            modeBest = true;
            modeNormal = false;
            break;
        default:
            break;
        }
    }

    if( etc2 && dither )
    {
        printf( "Dithering is disabled in ETC2 mode, as it degrades image quality.\n" );
        dither = false;
    }

    const char* input = nullptr;
    const char* output = nullptr;
    if( benchmark )
    {
        if( argc - optind < 1 )
        {
            Usage();
            return 1;
        }

        input = argv[optind];
    }
    else
    {
        if( argc - optind < 2 )
        {
            Usage();
            return 1;
        }

        input = argv[optind];
        output = argv[optind+1];
    }

    // hyeon add for target directory
    struct _finddata_t fd;
    int checkFileOrDir = 0;
    intptr_t handle;
    std::vector<std::string> imagePathList;

    if ((handle = _findfirst(input, &fd)) == -1L)
    {
        std::cout << "No file in directory!" << std::endl;
        return 0;
    }

    checkFileOrDir = isFileOrDir(fd);
    if (checkFileOrDir == 0 && fd.name[0] != '.') {
        isTargetDir = true;
        int result = 1;
        std::string input_dir = input;
        handle = _findfirst((input_dir + "/*.*").c_str(), &fd);

        while (result != -1 ) // find texture file 
        {
            if (fd.name[0] != '.') { // avoid hide file 
                imagePathList.push_back(fd.name);
            }
            result = _findnext(handle, &fd);
        }
    }
    _findclose(handle);
    std::string mode;
    if (modeFast) // Fast Mode 
    {
        mode = "Fast Mode";
        qualityRatio = 0.1;

    }
    else if (modeNormal)
    {
        mode = "Normal Mode";
        qualityRatio = 0.4;
    }
    else 
    {
        mode = "Best Mode";
        qualityRatio = 1.0;
    }

    if( benchmark )
    {
        if( viewMode )
        {
            auto bd = std::make_shared<BlockData>( input );

            constexpr int NumTasks = 9;
            uint64_t timeData[NumTasks];
            for( int i=0; i<NumTasks; i++ )
            {
                const auto start = GetTime();
                auto res = bd->Decode();
                const auto end = GetTime();
                timeData[i] = end - start;
            }
            std::sort( timeData, timeData+NumTasks );
            const auto median = timeData[NumTasks/2] / 1000.f;
            printf( "Median decode time for %i runs: %0.3f ms (%0.3f Mpx/s)\n", NumTasks, median, bd->Size().x * bd->Size().y / ( median * 1000 ) );
        }
        else
        {
            auto start = GetTime();
            auto bmp = std::make_shared<Bitmap>( input, std::numeric_limits<unsigned int>::max(), !dxtc );
            auto data = bmp->Data();
            auto end = GetTime();
            printf( "Image load time: %0.3f ms\n", ( end - start ) / 1000.f );

            constexpr int NumTasks = 9;
            uint64_t timeData[NumTasks];
            if( benchMt )
            {
                TaskDispatch taskDispatch( cpus );
                const unsigned int parts = ( ( bmp->Size().y / 4 ) + 32 - 1 ) / 32;

                for( int i=0; i<NumTasks; i++ )
                {
                    BlockData::Type type;
                    Channels channel;
                    
                    if( alpha ) channel = Channels::Alpha;
                    else channel = Channels::RGB;
                    if( rgba ) type = BlockData::Etc2_RGBA;
                    else if( etc2 ) type = BlockData::Etc2_RGB;
                    else if( dxtc ) type = bmp->Alpha() ? BlockData::Dxt5 : BlockData::Dxt1;
                    else type = BlockData::Etc1;

                    auto bd = std::make_shared<BlockData>( bmp->Size(), true, type );
                    auto ptr = bmp->Data();
                    const auto width = bmp->Size().x;
                    const auto localStart = GetTime();
                    auto start = GetTime();
                    auto linesLeft = bmp->Size().y / 4;
                    size_t offset = 0;
                    if( rgba || type == BlockData::Dxt5 )
                    {
                        for( int j=0; j<parts; j++ )
                        {
                            const auto lines = std::min( 32, linesLeft );
                            taskDispatch.Queue( [bd, ptr, width, lines, offset, useHeuristics] {
                                bd->ProcessRGBA( ptr, width * lines / 4, offset, width, useHeuristics );
                            } );
                            linesLeft -= lines;
                            ptr += width * lines;
                            offset += width * lines / 4;
                        }
                    }
                    else
                    {
                        for( int j=0; j<parts; j++ )
                        {
                            const auto lines = std::min( 32, linesLeft );
                            taskDispatch.Queue( [bd, ptr, width, lines, offset, channel, dither, useHeuristics] {
                                bd->Process( ptr, width * lines / 4, offset, width, channel, dither, useHeuristics );
                            } );
                            linesLeft -= lines;
                            ptr += width * lines;
                            offset += width * lines / 4;
                        }
                    }
                    taskDispatch.Sync();
                    auto end = GetTime();
                    const auto localEnd = GetTime();
                    timeData[i] = localEnd - localStart;
                    printf("etcpak encoding time: %0.3f ms\n", (end - start) / 1000.f);
                }
            }
            else
            {
                for( int i=0; i<NumTasks; i++ )
                {
                    BlockData::Type type;
                    Channels channel;
                    if( alpha ) channel = Channels::Alpha;
                    else channel = Channels::RGB;
                    if( rgba ) type = BlockData::Etc2_RGBA;
                    else if( etc2 ) type = BlockData::Etc2_RGB;
                    else if( dxtc ) type = bmp->Alpha() ? BlockData::Dxt5 : BlockData::Dxt1;
                    else type = BlockData::Etc1;
                    auto bd = std::make_shared<BlockData>( bmp->Size(), false, type );
                    const auto localStart = GetTime();
                    if( rgba || type == BlockData::Dxt5 )
                    {
                        bd->ProcessRGBA( bmp->Data(), bmp->Size().x * bmp->Size().y / 16, 0, bmp->Size().x, useHeuristics );
                    }
                    else
                    {
                        bd->Process( bmp->Data(), bmp->Size().x * bmp->Size().y / 16, 0, bmp->Size().x, channel, dither, useHeuristics );
                    }
                    const auto localEnd = GetTime();
                    timeData[i] = localEnd - localStart;
                }
            }
            std::sort( timeData, timeData+NumTasks );
            const auto median = timeData[NumTasks/2] / 1000.f;
            printf( "Median compression time for %i runs: %0.3f ms (%0.3f Mpx/s)", NumTasks, median, bmp->Size().x * bmp->Size().y / ( median * 1000 ) );
            if( benchMt )
            {
                printf( " multi threaded (%i cores)\n", cpus );
            }
            else
            {
                printf( " single threaded\n" );
            }
        }
    }
    else if( viewMode )
    {
        if (isTargetDir)
        {
            std::cout << "Check Result directory" << std::endl;
        }
        else
        {
            auto bd = std::make_shared<BlockData>(input);
            auto out = bd->Decode();
            out->Write(output);
        }
    }
    else if (useBetsy)
    {
        printf("Use Betsy Mode..\n");
        std::cout << "Your Selected Mode : " << mode << std::endl << "Qulity Ratio : " << qualityRatio * 100 << "%" << std::endl;
        
        std::vector<float> timeStamp;

        betsy::initBetsyPlatform();
        auto bdg = std::make_shared<BlockDataGPU>();
        auto start = GetTime();
        bdg->setEncodingEnv( false );
        auto    end = GetTime();

        int max_compute_work_group_count[3];
        int max_compute_work_group_size[3];
        int max_compute_work_group_invocations;

        for (int idx = 0; idx < 3; idx++) {
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_count[idx]);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_size[idx]);
        }
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);

        printf("betsy Init time: %0.3f ms\n", (end - start) / 1000.f);

        TaskDispatch taskDispatch(cpus);
        auto errorBlockDataPipeline = std::make_shared<ErrorBlockData>();
        BlockDataPtr priorBd;
        // CPU encoding with Multi processing 
        //-------------------------------------------------------------------------

        // Target directroy
        if (isTargetDir)
        {
            bool once = false;
            if ((handle = _findfirst(output, &fd)) == -1L)
            {
                std::cout << "Make output directory : " << output << std::endl;
                mkdir(output);
            }

            for (int t = 0; t < imagePathList.size(); t++)
            {
                std::string inputDir = input;
                std::string outputDir = output;
                std::string outputElement;
                if (outputDir.at(outputDir.length() - 1) == '/')
                {
                    outputElement = outputDir + "compressed_" + imagePathList[t];
                }
                else
                {
                    outputElement = outputDir + "/compressed_" + imagePathList[t];
                }

                outputElement.replace(outputElement.end() - 3, outputElement.end(), "ktx");
                // for debug
                // std::cout << "Create! : " << outputElement << std::endl;

                DataProvider dp((inputDir + "/" + imagePathList[t]).c_str(), mipmap, !dxtc, linearize);
                auto num = dp.NumberOfParts();
                errorBlockDataPipeline->setNumTasks(num);
 

                BlockData::Type type;
                if (etc2)
                {
                    if (rgba && dp.Alpha())
                    {
                        type = BlockData::Etc2_RGBA;
                    }
                    else
                    {
                        type = BlockData::Etc2_RGB;
                    }
                }
                else if (dxtc)
                {
                    if (dp.Alpha())
                    {
                        type = BlockData::Dxt5;
                    }
                    else
                    {
                        type = BlockData::Dxt1;
                    }
                }
                else
                {
                    type = BlockData::Etc1;
                }

                auto bd = std::make_shared<BlockData>(outputElement.c_str(), dp.Size(), mipmap, type);
                BlockDataPtr bda;
                if (alpha && dp.Alpha() && !rgba)
                {
                    bda = std::make_shared<BlockData>(alpha, dp.Size(), mipmap, type);
                }

                auto start = GetTime();
                for (int i = 0; i < num; i++)
                {
                    auto part = dp.NextPart();

                    if (type == BlockData::Etc2_RGBA || type == BlockData::Dxt5)
                    {
                        TaskDispatch::Queue([part, i, &bd, &dither, useHeuristics]()
                            {
                                bd->ProcessRGBA(part.src, part.width / 4 * part.lines, part.offset, part.width, useHeuristics);
                            });
                    }
                    else
                    {
                        //TaskDispatch::Queue([part, i, &bd, &dither, &errorBlockDataPipeline, useHeuristics]()
                        //    {
                        //        bd->Process(part.src, part.width / 4 * part.lines, errorBlockDataPipeline, part.offset, part.width, Channels::RGB, dither, useHeuristics);
                        //    });
                        if (bda)
                        {
                            TaskDispatch::Queue([part, i, &bda, useHeuristics]()
                                {
                                    bda->Process(part.src, part.width / 4 * part.lines, part.offset, part.width, Channels::Alpha, false, useHeuristics);
                                });
                        }
                    }
                }

                if (once)
                {
                    if (!errorBlockDataPipeline->isEmpty())
                    {
                        bdg->ProcessWithGPU(errorBlockDataPipeline, max_compute_work_group_size[1]);
                    }
                }

                TaskDispatch::Sync(); // wait CPU
                errorBlockDataPipeline->pushHighErrorBlocks();
                priorBd = bd; // for maintaining data address

                if (t == imagePathList.size() - 1) // for calculate residual block
                {
                    if (!errorBlockDataPipeline->isEmpty())
                    {
                        bdg->ProcessWithGPU(errorBlockDataPipeline, max_compute_work_group_size[1]);
                    }
                    priorBd = nullptr;
                }

                once = true;
                auto end = GetTime();
                float time = (end - start) / 1000.f;
                std::cout << t << " : output : " << outputElement.c_str() << " compression time = " << time << "ms" << std::endl;
                timeStamp.push_back(time);
            }

            // print result time 
            float sum = 0;
            for (int i = 0; i < timeStamp.size(); i++)
            {
                sum += timeStamp[i];
            }
            std::cout << "1 image compression average time = " << sum / timeStamp.size() << "ms" << std::endl;
            std::cout << "total image = " << timeStamp.size() << " total time = " << sum << "ms" << std::endl;
        } // target file 
        // target file 
        else
        {
            auto start = GetTime();
            auto bmp = std::make_shared<Bitmap>(input, std::numeric_limits<unsigned int>::max(), !dxtc);
            auto data = bmp->Data();
            auto end = GetTime();
            printf("Image load time: %0.3f ms\n", (end - start) / 1000.f);

            const unsigned int parts = ((bmp->Size().y / 4) + 32 - 1) / 32; // parts = 4

            BlockData::Type type;
            Channels channel;

            // define format 
            if (alpha) channel = Channels::Alpha;
            else channel = Channels::RGB;

            if (rgba) type = BlockData::Etc2_RGBA;
            else if (etc2) type = BlockData::Etc2_RGB;
            else if (dxtc) type = bmp->Alpha() ? BlockData::Dxt5 : BlockData::Dxt1;
            else type = BlockData::Etc1;

            auto bd = std::make_shared<BlockData>(output, bmp->Size(), false, type);
            auto ptr = bmp->Data();
            const auto width = bmp->Size().x;
            auto linesLeft = bmp->Size().y / 4;
            uint64_t BUFFER_MAX_SIZE = width * linesLeft;
            size_t offset = 0;

            std::vector<ErrorBlockDataPtr> errPipes;
            PixBlock* finalPipe = new PixBlock[width * bmp->Size().y];
            PixBlock** pixelMat = new PixBlock * [parts];
            int* sizeArr = new int[parts];

            for (int i = 0; i < parts; i++)
            {
                pixelMat[i] = new PixBlock[BUFFER_MAX_SIZE];
                sizeArr[i] = 0;
            }

            //for (int i = 0; i < parts; i++)
            //{
            //    ErrorBlockDataPtr pipe = std::make_shared<ErrorBlockData>();
            //    errPipes.push_back(pipe);
            //}

            const auto localStart = GetTime();
            if (rgba || type == BlockData::Dxt5)
            {
                for (int j = 0; j < parts; j++)
                {
                    const auto lines = std::min(32, linesLeft);
                    auto pPixelMat = pixelMat[j];
                    auto pSizeArr = &sizeArr[j];

                    taskDispatch.Queue([bd, ptr, width, lines, offset, useHeuristics, pPixelMat, pSizeArr] {
                        bd->ProcessRGBA(ptr, width * lines / 4, pPixelMat, pSizeArr, offset, width, useHeuristics);
                        });
                    linesLeft -= lines;
                    ptr += width * lines * 4;
                    offset += width * lines / 4;
                }
            }
            else
            {
                for (int j = 0; j < parts; j++)
                {
                    const auto lines = std::min(32, linesLeft);
                    //ErrorBlockDataPtr pipe = errPipes[j];
                    auto pPixelMat = pixelMat[j];
                    auto pSizeArr = &sizeArr[j];

                    taskDispatch.Queue([bd, ptr, width, lines, offset, channel, dither, useHeuristics, pPixelMat, pSizeArr] {
                        bd->Process(ptr, width * lines / 4, pPixelMat, pSizeArr, offset, width, channel, dither, useHeuristics);
                        });
                    linesLeft -= lines;
                    ptr += width * lines * 4;
                    offset += width * lines / 4;
                }
            }
            taskDispatch.Sync();

            ErrorBlockDataPtr pFpipe = std::make_shared<ErrorBlockData>();
            //for (int i = 1; i < parts; i++)
            //{
            //    errPipes[0]->merge(errPipes[i]->m_pipe);
            //}

            int finalPipeSize = 0;
            for (int i = 0; i < parts; i++)
            {
                std::memcpy(finalPipe + finalPipeSize, pixelMat[i], sizeArr[i] * sizeof(PixBlock));
                finalPipeSize += sizeArr[i];
            }

            //for (int i = 0; i < finalPipeSize; i++)
            //{
            //    printf("%d : %d\n",i, finalPipe[i].error);
            //}

            // betsy GPU encoding. // 3ms =========
            //bdg->ProcessWithGPU(errPipes[0], max_compute_work_group_size[1]); 
            // GPU image max size 1024 x 1024 x 64 in opengl Compute shader work group size 
            std::cout << "finalPipeSize = " << finalPipeSize << std::endl;
            bdg->ProcessWithGPU(finalPipe, finalPipeSize, max_compute_work_group_size[0] * max_compute_work_group_size[1], qualityRatio);
            const auto localEnd = GetTime();
            printf("total encoding time: %0.3f \n", (localEnd - localStart) / 1000.f);


            // delete memory 
            for (int i = 0; i < parts; i++)
            {
                delete[] pixelMat[i];
            }

            delete[] pixelMat;
            delete[] sizeArr;
        }
    }
    
    else
    {
        DataProvider dp( input, mipmap, !dxtc, linearize );
        auto num = dp.NumberOfParts();

        BlockData::Type type;
        if( etc2 )
        {
            if( rgba && dp.Alpha() )
            {
                type = BlockData::Etc2_RGBA;
            }
            else
            {
                type = BlockData::Etc2_RGB;
            }
        }
        else if( dxtc )
        {
            if( dp.Alpha() )
            {
                type = BlockData::Dxt5;
            }
            else
            {
                type = BlockData::Dxt1;
            }
        }
        else
        {
            type = BlockData::Etc1;
        }

        TaskDispatch taskDispatch( cpus );

        auto bd = std::make_shared<BlockData>( output, dp.Size(), mipmap, type );
        BlockDataPtr bda;
        if( alpha && dp.Alpha() && !rgba )
        {
            bda = std::make_shared<BlockData>( alpha, dp.Size(), mipmap, type );
        }

        const auto etc_start = GetTime();
        for( int i=0; i<num; i++ )
        {
            auto part = dp.NextPart();

            if( type == BlockData::Etc2_RGBA || type == BlockData::Dxt5 )
            {
                TaskDispatch::Queue( [part, i, &bd, &dither, useHeuristics]()
                {
                    bd->ProcessRGBA( part.src, part.width / 4 * part.lines, part.offset, part.width, useHeuristics );
                } );
            }
            else
            {
                TaskDispatch::Queue( [part, i, &bd, &dither, useHeuristics]()
                {
                    bd->Process( part.src, part.width / 4 * part.lines, part.offset, part.width, Channels::RGB, dither, useHeuristics );
                } );
                if( bda )
                {
                    TaskDispatch::Queue( [part, i, &bda, useHeuristics]()
                    {
                        bda->Process( part.src, part.width / 4 * part.lines, part.offset, part.width, Channels::Alpha, false, useHeuristics );
                    } );
                }
            }
        }
        TaskDispatch::Sync();
        const auto etc_end = GetTime();
        printf("etcpak encoding time: %0.3f ms\n", (etc_end - etc_start) / 1000.f);

        if( stats )
        {
            auto out = bd->Decode();
            float mse = CalcMSE3( dp.ImageData(), *out );
            printf( "RGB data\n" );
            printf( "  RMSE: %f\n", sqrt( mse ) );
            printf( "  PSNR: %f\n", 20 * log10( 255 ) - 10 * log10( mse ) );
        }
    }

    return 0;
}
