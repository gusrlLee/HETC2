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
    fprintf( stderr, "  --betsy-mode           execute BetsyGPU Encoding (with Multi-threading Option)\n\n");
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
        OptBetsyMode
    };

    struct option longopts[] = {
        { "etc1", no_argument, nullptr, OptEtc1 },
        { "rgba", no_argument, nullptr, OptRgba },
        { "dxtc", no_argument, nullptr, OptDxtc },
        { "linear", no_argument, nullptr, OptLinear },
        { "disable-heuristics", no_argument, nullptr, OptNoHeuristics },
        { "betsy-mode", no_argument, nullptr, OptBetsyMode },
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
    // for check file name 
    //std::cout << "texture count : " << imagePathList.size() << std::endl;
    //std::cout << "example data : " << imagePathList[2] << std::endl;

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
                    auto bd = std::make_shared<BlockData>( bmp->Size(), false, type );
                    auto ptr = bmp->Data();
                    const auto width = bmp->Size().x;
                    const auto localStart = GetTime();
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
                    const auto localEnd = GetTime();
                    timeData[i] = localEnd - localStart;
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
    else if ( useBetsy )
    {
        std::vector<float> timeStamp;

        betsy::initBetsyPlatform();
        auto bdg = std::make_shared<BlockDataGPU>();
        auto start = GetTime();
        bdg->setEncodingEnv();
        // bdg->initGPU(input);
        // glFinish();
        auto    end = GetTime();

        printf("betsy Init time: %0.3f ms\n", (end - start) / 1000.f);
        TaskDispatch taskDispatch(cpus);
        auto errorBlockDataPipeline = std::make_shared<ErrorBlockData>();
        
        // CPU encoding with Multi processing 
        //-------------------------------------------------------------------------

        // Target directroy
        if ( isTargetDir ) 
        {
            if ((handle = _findfirst(output, &fd)) == -1L)
            {
                std::cout << "Make output directory : " << output << std::endl;
                mkdir(output);
            }

            for (int i = 0; i < imagePathList.size(); i++)
            {   
                std::string inputDir = input;
                std::string outputDir = output;
                std::string outputElement;
                if (outputDir.at(outputDir.length()-1) == '/')
                {
                    outputElement = outputDir + "compressed_" + imagePathList[i];
                }
                else
                {
                    outputElement = outputDir + "/compressed_" + imagePathList[i];
                }

                outputElement.replace(outputElement.end() - 3, outputElement.end(), "ktx");
                // for debug
                // std::cout << "Create! : " << outputElement << std::endl;

                DataProvider dp((inputDir + "/" + imagePathList[i]).c_str(), mipmap, !dxtc, linearize);
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
                        TaskDispatch::Queue([part, i, &bd, &dither, &errorBlockDataPipeline, useHeuristics]()
                            {
                                bd->Process(part.src, part.width / 4 * part.lines, errorBlockDataPipeline, part.offset, part.width, Channels::RGB, dither, useHeuristics);
                            });
                        if (bda)
                        {
                            TaskDispatch::Queue([part, i, &bda, useHeuristics]()
                                {
                                    bda->Process(part.src, part.width / 4 * part.lines, part.offset, part.width, Channels::Alpha, false, useHeuristics);
                                });
                        }
                    }
                }
                TaskDispatch::Sync();
                // push block vector 
                errorBlockDataPipeline->pushHighErrorBlocks();
                TaskDispatch::Queue([&bdg, &errorBlockDataPipeline]() // start GPU encoding 
                    {
                        bdg->ProcessWithGPU(errorBlockDataPipeline);
                    });
                TaskDispatch::Sync();
                auto end = GetTime();

                float time = (end - start) / 1000.f;
                std::cout << "output : " << outputElement.c_str() << " compression time = " << time << "ms" << std::endl;
                timeStamp.push_back(time);
            }

            float sum = 0;
            for (int i = 0; i < timeStamp.size(); i++)
            {
                sum += timeStamp[i];
            }
            std::cout << "1 image compression average time = " << sum / timeStamp.size() << "ms" << std::endl;
            std::cout << "total image = " << timeStamp.size() << " total time = " << sum << "ms" << std::endl;
        } // target file 
        else
        {
            DataProvider dp(input, mipmap, !dxtc, linearize);
            auto num = dp.NumberOfParts();
            errorBlockDataPipeline->setNumTasks( num );

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
            auto bd = std::make_shared<BlockData>(output, dp.Size(), mipmap, type);
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
                    TaskDispatch::Queue([part, i, &bd, &dither, &errorBlockDataPipeline, useHeuristics]()
                        {
                            // our path
                            bd->Process(part.src, part.width / 4 * part.lines, errorBlockDataPipeline, part.offset, part.width, Channels::RGB, dither, useHeuristics);
                        });
                    if (bda)
                    {
                        TaskDispatch::Queue([part, i, &bda, useHeuristics]()
                            {
                                bda->Process(part.src, part.width / 4 * part.lines, part.offset, part.width, Channels::Alpha, false, useHeuristics);
                            });
                    }
                }
            }

            TaskDispatch::Sync(); // wait end CPU encoding.
            auto end = GetTime();
            printf("etcpak encoding time: %0.3f ms\n", (end - start) / 1000.f);

            errorBlockDataPipeline->pushHighErrorBlocks();

            //-------------------------------------------------------------------------
            start = GetTime();
            TaskDispatch::Queue([&bdg, &errorBlockDataPipeline]() // start GPU encoding 
                {
                    bdg->ProcessWithGPU(errorBlockDataPipeline);
                });
            TaskDispatch::Sync();
            end = GetTime();
            printf("betsy encoding time: %0.3f ms\n", (end - start) / 1000.f);
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

        auto start = GetTime();
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
        auto end = GetTime();
        printf("etcpak encoding time: %0.3f ms\n", (end - start) / 1000.f);

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
