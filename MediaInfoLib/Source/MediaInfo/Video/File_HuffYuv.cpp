/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// http://multimedia.cx/huffyuv.txt
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
// Pre-compilation
#include "MediaInfo/PreComp.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Setup.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#if defined(MEDIAINFO_HUFFYUV_YES)
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Video/File_HuffYuv.h"
#include "ZenLib/BitStream.h"
//---------------------------------------------------------------------------

#include <algorithm>
using namespace std;

//---------------------------------------------------------------------------
namespace MediaInfoLib
{

//***************************************************************************
// Info
//***************************************************************************

static const char* HuffYUV_ColorSpace (int16u BitCount)
{
    switch (BitCount&0xFFF8)
    {
        case  8 :
        case 16 : return "YUV";
        case 24 : return "RGB";
        case 32 : return "RGBA";
        default : return "";
    }
}

static const char* HuffYUV_ChromaSubsampling (int16u BitCount)
{
    switch (BitCount&0xFFF8)
    {
        case  8 : return "4:2:0";
        case 16 : return "4:2:2";
        default : return "";
    }
}

static const string HuffYUV_ColorSpace(bool rgb, bool chroma, bool alpha)
{
    string ToReturn;

    if (rgb)
    {
        ToReturn="RGB";
    }
    else
    {
        ToReturn=chroma?"YUV":"Y";
    }

    if (alpha)
        ToReturn+='A';

    return ToReturn;
}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
File_HuffYuv::File_HuffYuv()
:File__Analyze()
{
    //Configuration
    ParserName="HuffYUV";
    #if MEDIAINFO_TRACE
        Trace_Layers_Update(8); //Stream
    #endif //MEDIAINFO_TRACE
    StreamSource=IsStream;

    //In
    BitCount=0;
    Height=0;
}

//***************************************************************************
// Streams management
//***************************************************************************

//---------------------------------------------------------------------------
void File_HuffYuv::Streams_Accept()
{
    Stream_Prepare(Stream_Video);
    Fill(Stream_Video, 0, Video_Format, "HuffYUV");
}

//***************************************************************************
// Buffer - Global
//***************************************************************************

//---------------------------------------------------------------------------
void File_HuffYuv::Read_Buffer_OutOfBand()
{
    FrameHeader();
    if (Element_Offset<Element_Size)
        Skip_XX(Element_Size-Element_Offset,                    "Unknown");

    FILLING_BEGIN()
        Accept();
    FILLING_END();
}

//---------------------------------------------------------------------------
void File_HuffYuv::Read_Buffer_Continue()
{
    Skip_XX(Element_Size,                                       "Data");

    if (!Status[IsAccepted]) //No test except that the stream has no out of band data
    {
        Accept();
        Fill(Stream_Video, 0, Video_Format_Version, "Version 1");
    }

    Frame_Count++;
    Finish();
}

//***************************************************************************
// Elements
//***************************************************************************

//---------------------------------------------------------------------------
void File_HuffYuv::FrameHeader()
{
    //Version
    if (Element_Size<4)
    {
        Reject();
        return;
    }
    int8u Version;
    if (Buffer[3]==0)
    {
        Version=Element_Size>4?2:1;
    }
    else
        Version=3;

    //Parsing
    int8u   bpp_override=0, chroma_v_shift=0, chroma_h_shift=0, interlace;
    bool    alpha=false, chroma=false, rgb=false;
    Element_Begin1("method");
    BS_Begin();
        Skip_SB(                                                "unknown");
        Skip_SB(                                                "decorrelate");
        Skip_S1(6,                                              "predictor");
    Element_End0();
    if (Version<=2)
    {
        Get_S1 (8, bpp_override,                                "bpp_override");
    }
    else
    {
        Get_S1 (4, bpp_override,                                "bit_depth"); Param_Info2(bpp_override+1, "bits");
        Get_S1 (2, chroma_v_shift,                              "chroma_v_shift");
        Get_S1 (2, chroma_h_shift,                              "chroma_h_shift");
    }
    Skip_SB(                                                    "unknown");
    Skip_SB(                                                    "context");
    Get_S1 (2, interlace,                                       "interlace");
    if (Version<=2)
    {
        Skip_S1(4,                                              "unknown");
        Skip_S1(8,                                              "zero");
    }
    else
    {
        Skip_SB(                                                "unknown");
        Get_SB (   alpha,                                       "alpha");
        Get_SB (   rgb,                                         "rgb");
        if (rgb)
            Skip_SB(                                            "unused");
        else
            Get_SB (   chroma,                                  "chroma");
        Skip_S1(7,                                              "unused");
        Skip_SB(                                                "version 3+ indicator");
    }
    BS_End();

    if (Frame_Count==0)
    {
        if (Version==2)
        {
            //BiCount;
            if (bpp_override)
                BitCount=bpp_override;
            Fill(Stream_Video, 0, Video_BitDepth, 8);
        }
        else
        {
            Fill(Stream_Video, 0, Video_BitDepth, bpp_override+1);
        }

        Fill(Stream_Video, 0, Video_Format_Version, __T("Version ")+Ztring::ToZtring(Version));

        if (Version==2)
        {
            Fill(Stream_Video, 0, Video_ColorSpace, HuffYUV_ColorSpace(BitCount));
            Fill(Stream_Video, 0, Video_ChromaSubsampling, HuffYUV_ChromaSubsampling(BitCount));
        }
        else
        {
            Fill(Stream_Video, 0, Video_ColorSpace, HuffYUV_ColorSpace(rgb, chroma, alpha));
            string ChromaSubsampling;
            if (chroma)
            {
                switch (chroma_h_shift)
                {
                    case 0 :
                            switch (chroma_v_shift)
                            {
                                case 0 : ChromaSubsampling="4:4:4"; break;
                                default: ;
                            }
                            break;
                    case 1 :
                            switch (chroma_v_shift)
                            {
                                case 0 : ChromaSubsampling="4:2:2"; break;
                                case 1 : ChromaSubsampling="4:2:0"; break;
                                default: ;
                            }
                            break;
                    case 2 :
                            switch (chroma_v_shift)
                            {
                                case 0 : ChromaSubsampling="4:1:1"; break;
                                case 1 : ChromaSubsampling="4:1:0"; break;
                                case 2 : ChromaSubsampling="4:1:0 (4x4)"; break;
                                default: ;
                            }
                            break;
                    default: ;
                }
            }
            if (!ChromaSubsampling.empty() && alpha)
                ChromaSubsampling+=":4";
            Fill(Stream_Video, 0, Video_ChromaSubsampling, ChromaSubsampling);
        }

        switch (interlace)
        {
            case 0 :
                    if (Version<=2 && Height)
                        Fill(Stream_Video, 0, Video_ScanType, Height>288?"Interlaced":"Progressive");
                    break;
            case 1 :
                    Fill(Stream_Video, 0, Video_ScanType, "Interlaced");
                    break;
            case 2 :
                    Fill(Stream_Video, 0, Video_ScanType, "Progressive");
                    break;
            default:;
        }
    }
}

} //NameSpace

#endif //MEDIAINFO_HUFFYUV_YES
