/* -----------------------------------------------------------------------------
Software Copyright License for the Fraunhofer Software Library VVenc

(c) Copyright (2019-2020) Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. 

1.    INTRODUCTION

The Fraunhofer Software Library VVenc (“Fraunhofer Versatile Video Encoding Library”) is software that implements (parts of) the Versatile Video Coding Standard - ITU-T H.266 | MPEG-I - Part 3 (ISO/IEC 23090-3) and related technology. 
The standard contains Fraunhofer patents as well as third-party patents. Patent licenses from third party standard patent right holders may be required for using the Fraunhofer Versatile Video Encoding Library. It is in your responsibility to obtain those if necessary. 

The Fraunhofer Versatile Video Encoding Library which mean any source code provided by Fraunhofer are made available under this software copyright license. 
It is based on the official ITU/ISO/IEC VVC Test Model (VTM) reference software whose copyright holders are indicated in the copyright notices of its source files. The VVC Test Model (VTM) reference software is licensed under the 3-Clause BSD License and therefore not subject of this software copyright license.

2.    COPYRIGHT LICENSE

Internal use of the Fraunhofer Versatile Video Encoding Library, in source and binary forms, with or without modification, is permitted without payment of copyright license fees for non-commercial purposes of evaluation, testing and academic research. 

No right or license, express or implied, is granted to any part of the Fraunhofer Versatile Video Encoding Library except and solely to the extent as expressly set forth herein. Any commercial use or exploitation of the Fraunhofer Versatile Video Encoding Library and/or any modifications thereto under this license are prohibited.

For any other use of the Fraunhofer Versatile Video Encoding Library than permitted by this software copyright license You need another license from Fraunhofer. In such case please contact Fraunhofer under the CONTACT INFORMATION below.

3.    LIMITED PATENT LICENSE

As mentioned under 1. Fraunhofer patents are implemented by the Fraunhofer Versatile Video Encoding Library. If You use the Fraunhofer Versatile Video Encoding Library in Germany, the use of those Fraunhofer patents for purposes of testing, evaluating and research and development is permitted within the statutory limitations of German patent law. However, if You use the Fraunhofer Versatile Video Encoding Library in a country where the use for research and development purposes is not permitted without a license, you must obtain an appropriate license from Fraunhofer. It is Your responsibility to check the legal requirements for any use of applicable patents.    

Fraunhofer provides no warranty of patent non-infringement with respect to the Fraunhofer Versatile Video Encoding Library.


4.    DISCLAIMER

The Fraunhofer Versatile Video Encoding Library is provided by Fraunhofer "AS IS" and WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, including but not limited to the implied warranties fitness for a particular purpose. IN NO EVENT SHALL FRAUNHOFER BE LIABLE for any direct, indirect, incidental, special, exemplary, or consequential damages, including but not limited to procurement of substitute goods or services; loss of use, data, or profits, or business interruption, however caused and on any theory of liability, whether in contract, strict liability, or tort (including negligence), arising in any way out of the use of the Fraunhofer Versatile Video Encoding Library, even if advised of the possibility of such damage.

5.    CONTACT INFORMATION

Fraunhofer Heinrich Hertz Institute
Attention: Video Coding & Analytics Department
Einsteinufer 37
10587 Berlin, Germany
www.hhi.fraunhofer.de/vvc
vvc@hhi.fraunhofer.de
----------------------------------------------------------------------------- */


/** \file     Prediction.cpp
    \brief    prediction class
*/

#include "IntraPrediction.h"
#include "Unit.h"
#include "UnitTools.h"
#include "Rom.h"
#include "InterpolationFilter.h"
#include "dtrace_next.h"

#include <memory.h>

//! \ingroup CommonLib
//! \{

namespace vvenc {

// ====================================================================================================================
// Tables
// ====================================================================================================================

const uint8_t IntraPrediction::m_aucIntraFilter[MAX_INTRA_FILTER_DEPTHS] =
{
  24, //   1xn
  24, //   2xn
  24, //   4xn
  14, //   8xn
  2,  //  16xn
  0,  //  32xn
  0,  //  64xn
  0   // 128xn
};

//NOTE: Bit-Limit - 24-bit source
void xPredIntraPlanar_Core( PelBuf& pDst, const CPelBuf& pSrc )
{
  const uint32_t width  = pDst.width;
  const uint32_t height = pDst.height;
  const uint32_t log2W  = Log2(width);
  const uint32_t log2H  = Log2(height);

  int leftColumn[MAX_TB_SIZEY + 1], topRow[MAX_TB_SIZEY + 1], bottomRow[MAX_TB_SIZEY], rightColumn[MAX_TB_SIZEY];
  const uint32_t offset = 1 << (log2W + log2H);

  // Get left and above reference column and row
  for( int k = 0; k < width + 1; k++ )
  {
    topRow[k] = pSrc.at( k + 1, 0 );
  }

  for( int k = 0; k < height + 1; k++ )
  {
    leftColumn[k] = pSrc.at( k + 1, 1 );
  }

  // Prepare intermediate variables used in interpolation
  int bottomLeft = leftColumn[height];
  int topRight = topRow[width];

  for( int k = 0; k < width; k++ )
  {
    bottomRow[k] = bottomLeft - topRow[k];
    topRow[k]    = topRow[k] << log2H;
  }

  // with some optimizations gcc-8 gives spurious "-Wmaybe-uninitialized" warnings here (says leftColumn would be uninitialized here)
  GCC_WARNING_DISABLE_maybe_uninitialized
  for( int k = 0; k < height; k++ )
  {
    rightColumn[k] = topRight - leftColumn[k];
    leftColumn[k]  = leftColumn[k] << log2W;
  }
  GCC_WARNING_RESET

  const uint32_t finalShift = 1 + log2W + log2H;
  const uint32_t stride     = pDst.stride;
  Pel*       pred       = pDst.buf;
  for( int y = 0; y < height; y++, pred += stride )
  {
    int horPred = leftColumn[y];

    for( int x = 0; x < width; x++ )
    {
      horPred += rightColumn[y];
      topRow[x] += bottomRow[x];

      int vertPred = topRow[x];
      pred[x]      = ( ( horPred << log2H ) + ( vertPred << log2W ) + offset ) >> finalShift;
    }
  }
}

void  IntraPredSampleFilter_Core(PelBuf& dstBuf, const CPelBuf& pSrc)
{
  const int iWidth  = dstBuf.width;
  const int iHeight = dstBuf.height;

  const int scale = ((Log2(iWidth*iHeight) - 2) >> 2);
  CHECK(scale < 0 || scale > 31, "PDPC: scale < 0 || scale > 31");

  for (int y = 0; y < iHeight; y++)
  {
    const int wT   = 32 >> std::min(31, ((y << 1) >> scale));
    const Pel left = pSrc.at(y + 1, 1);
    for (int x = 0; x < iWidth; x++)
    {
      const int wL    = 32 >> std::min(31, ((x << 1) >> scale));
      const Pel top   = pSrc.at(x + 1, 0);
      const Pel val   = dstBuf.at(x, y);
      dstBuf.at(x, y) = val + ((wL * (left - val) + wT * (top - val) + 32) >> 6);
    }
  }
}

void IntraHorVerPDPC_Core(Pel* pDsty,const int dstStride,Pel* refSide,const int width,const int height,int scale,const Pel* refMain, const ClpRng& clpRng)
{
  const Pel topLeft = refMain[0];

  for( int y = 0; y < height; y++ )
  {
    memcpy(pDsty,&refMain[1],width*sizeof(Pel));
    const Pel left    = refSide[1 + y];
    for (int x = 0; x < std::min(3 << scale, width); x++)
    {
      const int wL  = 32 >> (2 * x >> scale);
      const Pel val = pDsty[x];
      pDsty[x]      = ClipPel(val + ((wL * (left - topLeft) + 32) >> 6), clpRng);
    }
    pDsty += dstStride;
  }
}
void IntraAnglePDPC_Core(Pel* pDsty,const int dstStride,Pel* refSide,const int width,const int height,int scale,int invAngle)
{
  for (int y = 0; y<height; y++, pDsty += dstStride)
  {
    int       invAngleSum = 256;
    for (int x = 0; x < std::min(3 << scale, width); x++)
    {
      invAngleSum += invAngle;
      int wL   = 32 >> (2 * x >> scale);
      Pel left = refSide[y + (invAngleSum >> 9) + 1];
      pDsty[x] = pDsty[x] + ((wL * (left - pDsty[x]) + 32) >> 6);
    }
  }
}

void IntraPredAngleLuma_Core(Pel* pDstBuf,const ptrdiff_t dstStride,Pel* refMain,int width,int height,int deltaPos,int intraPredAngle,const TFilterCoeff *ff_unused,const bool useCubicFilter,const ClpRng& clpRng)
{
  for (int y = 0; y<height; y++ )
  {
    const int deltaInt   = deltaPos >> 5;
    const int deltaFract = deltaPos & ( 32 - 1 );

    const TFilterCoeff      intraSmoothingFilter[4] = {TFilterCoeff(16 - (deltaFract >> 1)), TFilterCoeff(32 - (deltaFract >> 1)), TFilterCoeff(16 + (deltaFract >> 1)), TFilterCoeff(deltaFract >> 1)};
    const TFilterCoeff *f = useCubicFilter ? InterpolationFilter::getChromaFilterTable(deltaFract) : intraSmoothingFilter;

    Pel p[4];

    int refMainIndex = deltaInt + 1;

 //   const TFilterCoeff *f = &ff[deltaFract << 2];

    for( int x = 0; x < width; x++, refMainIndex++ )
    {
      p[0] = refMain[refMainIndex - 1];
      p[1] = refMain[refMainIndex    ];
      p[2] = refMain[refMainIndex + 1];
      p[3] = refMain[refMainIndex + 2];

      pDstBuf[y*dstStride + x] = static_cast<Pel>((static_cast<int>(f[0] * p[0]) + static_cast<int>(f[1] * p[1]) + static_cast<int>(f[2] * p[2]) + static_cast<int>(f[3] * p[3]) + 32) >> 6);

      if( useCubicFilter ) // only cubic filter has negative coefficients and requires clipping
      {
        pDstBuf[y*dstStride + x] = ClipPel( pDstBuf[y*dstStride + x], clpRng );
      }
    }
    deltaPos += intraPredAngle;
  }
}

void IntraPredAngleChroma_Core(Pel* pDstBuf,const ptrdiff_t dstStride,int16_t* pBorder,int width,int height,int deltaPos,int intraPredAngle)
{
  for (int y = 0; y<height; y++)
  {
    const int deltaInt   = deltaPos >> 5;
    const int deltaFract = deltaPos & (32 - 1);

    // Do linear filtering
    const Pel* pRM = pBorder + deltaInt + 1;
    int lastRefMainPel = *pRM++;

    for( int x = 0; x < width; pRM++, x++ )
    {
      int thisRefMainPel = *pRM;
      pDstBuf[x + 0] = ( Pel ) ( ( ( 32 - deltaFract )*lastRefMainPel + deltaFract*thisRefMainPel + 16 ) >> 5 );
      lastRefMainPel = thisRefMainPel;
    }
    deltaPos += intraPredAngle;
    pDstBuf += dstStride;
  }
}

// ====================================================================================================================
// Constructor / destructor / initialize
// ====================================================================================================================

IntraPrediction::IntraPrediction()
:  m_pMdlmTemp( nullptr )
,  m_currChromaFormat( NUM_CHROMA_FORMAT )
{
  IntraPredAngleLuma    = IntraPredAngleLuma_Core;
  IntraPredAngleChroma  = IntraPredAngleChroma_Core;
  IntraAnglePDPC        = IntraAnglePDPC_Core;
  IntraHorVerPDPC       = IntraHorVerPDPC_Core;
  IntraPredSampleFilter = IntraPredSampleFilter_Core;
  xPredIntraPlanar      = xPredIntraPlanar_Core;
}

IntraPrediction::~IntraPrediction()
{
  destroy();
}

void IntraPrediction::destroy()
{
  delete[] m_pMdlmTemp;
  m_pMdlmTemp = nullptr;
}

void IntraPrediction::init(ChromaFormat chromaFormatIDC, const unsigned bitDepthY)
{
  m_currChromaFormat = chromaFormatIDC;

  if (m_pMdlmTemp == nullptr)
  {
    m_pMdlmTemp = new Pel[(2 * MAX_TB_SIZEY + 1)*(2 * MAX_TB_SIZEY + 1)];//MDLM will use top-above and left-below samples.
  }
#if   ENABLE_SIMD_OPT_INTRAPRED
  initIntraPredictionX86();
#endif

}

// ====================================================================================================================
// Public member functions
// ====================================================================================================================

// Function for calculating DC value of the reference samples used in Intra prediction
//NOTE: Bit-Limit - 25-bit source
Pel IntraPrediction::xGetPredValDc( const CPelBuf& pSrc, const Size& dstSize )
{
  CHECK( dstSize.width == 0 || dstSize.height == 0, "Empty area provided" );

  int idx, sum = 0;
  Pel dcVal;
  const int width  = dstSize.width;
  const int height = dstSize.height;
  const auto denom     = (width == height) ? (width << 1) : std::max(width,height);
  const auto divShift  = Log2(denom);
  const auto divOffset = (denom >> 1);
  const int off = m_ipaParam.multiRefIndex + 1;


  if ( width >= height )
  {
    for( idx = 0; idx < width; idx++ )
    {
      sum += pSrc.at( off + idx, 0);
    }
  }
  if ( width <= height )
  {
    for( idx = 0; idx < height; idx++ )
    {
      sum += pSrc.at( off + idx, 1);
    }
  }

  dcVal = (sum + divOffset) >> divShift;
  return dcVal;
}

int IntraPrediction::getWideAngle( int width, int height, int predMode )
{
  if ( predMode > DC_IDX && predMode <= VDIA_IDX )
  {
    int modeShift[] = { 0, 6, 10, 12, 14, 15 };
    int deltaSize = abs(Log2(width) - Log2(height));
    if (width > height && predMode < 2 + modeShift[deltaSize])
    {
      predMode += (VDIA_IDX - 1);
    }
    else if (height > width && predMode > VDIA_IDX - modeShift[deltaSize])
    {
      predMode -= (VDIA_IDX - 1);
    }
  }
  return predMode;
}

void IntraPrediction::predIntraAng( const ComponentID compId, PelBuf& piPred, const PredictionUnit &pu)
{
  const ComponentID    compID       = compId;
  const ChannelType    channelType  = toChannelType( compID );
  const uint32_t       uiDirMode    = /*isLuma( compId ) && pu.cu->bdpcmMode ? BDPCM_IDX : */PU::getFinalIntraMode( pu, channelType );

  CHECK( Log2(piPred.width) < 2 && pu.cs->pcv->noChroma2x2, "Size not allowed" );
  CHECK( Log2(piPred.width) > 7, "Size not allowed" );

//  const int multiRefIdx = m_ipaParam.multiRefIndex;
  const int srcStride  = m_refBufferStride[compID];
  const int srcHStride = 2;

  const CPelBuf& srcBuf = CPelBuf(getPredictorPtr(compID), srcStride, srcHStride);
  const ClpRng& clpRng(pu.cu->cs->slice->clpRngs[compID]);

  switch (uiDirMode)
  {
    case(PLANAR_IDX): xPredIntraPlanar( piPred, srcBuf); break;
    case(DC_IDX):     xPredIntraDc    ( piPred, srcBuf ); break;
//    case(BDPCM_IDX):  xPredIntraBDPCM ( piPred, srcBuf, pu.cu->bdpcmMode, clpRng); break;
    default:          xPredIntraAng   ( piPred, srcBuf, channelType, clpRng); break;
  }

  if (m_ipaParam.applyPDPC)
  {
    if (uiDirMode == PLANAR_IDX || uiDirMode == DC_IDX)
    {
      IntraPredSampleFilter(piPred, srcBuf);
    }
  }
}

void IntraPrediction::predIntraChromaLM(const ComponentID compID, PelBuf& piPred, const PredictionUnit &pu, const CompArea& chromaArea, int intraDir)
{
  CHECK( piPred.width > MAX_TB_SIZEY || piPred.height > MAX_TB_SIZEY, "not enough memory");
  const int iLumaStride = 2 * MAX_TB_SIZEY + 1;
  PelBuf Temp = PelBuf(m_pMdlmTemp + iLumaStride + 1, iLumaStride, Size(chromaArea));

  int a, b, iShift;
  xGetLMParameters(pu, compID, chromaArea, a, b, iShift); // th shift result is unsigned

  ////// final prediction
  piPred.copyFrom(Temp);
  piPred.linearTransform(a, iShift, b, true, pu.cs->slice->clpRngs[compID]);
}

/** Function for deriving planar intra prediction. This function derives the prediction samples for planar mode (intra coding).
 */

void IntraPrediction::xPredIntraDc( PelBuf& pDst, const CPelBuf& pSrc )
{
  const Pel dcval = xGetPredValDc( pSrc, pDst );
  pDst.fill( dcval );
}

// Function for initialization of intra prediction parameters
void IntraPrediction::initPredIntraParams(const PredictionUnit & pu, const CompArea area, const SPS& sps)
{
  const ComponentID compId = area.compID;
  const ChannelType chType = toChannelType(compId);

  const bool        useISP = NOT_INTRA_SUBPARTITIONS != pu.cu->ispMode && isLuma( chType );

  const Size   cuSize    = Size( pu.cu->blocks[compId].width, pu.cu->blocks[compId].height );
  const Size   puSize    = Size( area.width, area.height );
  const Size&  blockSize = useISP ? cuSize : puSize;
  const int      dirMode = PU::getFinalIntraMode(pu, chType);
  const int     predMode = getWideAngle( blockSize.width, blockSize.height, dirMode );

  m_ipaParam.isModeVer            = predMode >= DIA_IDX;
  m_ipaParam.multiRefIndex        = isLuma (chType) ? pu.multiRefIdx : 0 ;
  m_ipaParam.refFilterFlag        = false;
  m_ipaParam.interpolationFlag    = false;
  m_ipaParam.applyPDPC            = (puSize.width >= MIN_TB_SIZEY && puSize.height >= MIN_TB_SIZEY) && m_ipaParam.multiRefIndex == 0;

  const int    intraPredAngleMode = (m_ipaParam.isModeVer) ? predMode - VER_IDX : -(predMode - HOR_IDX);


  int absAng = 0;
  if (dirMode > DC_IDX && dirMode < NUM_LUMA_MODE) // intraPredAngle for directional modes
  {
    static const int angTable[32]    = { 0,    1,    2,    3,    4,    6,     8,   10,   12,   14,   16,   18,   20,   23,   26,   29,   32,   35,   39,  45,  51,  57,  64,  73,  86, 102, 128, 171, 256, 341, 512, 1024 };
    static const int invAngTable[32] = {
      0,   16384, 8192, 5461, 4096, 2731, 2048, 1638, 1365, 1170, 1024, 910, 819, 712, 630, 565,
      512, 468,   420,  364,  321,  287,  256,  224,  191,  161,  128,  96,  64,  48,  32,  16
    };   // (512 * 32) / Angle

    const int     absAngMode         = abs(intraPredAngleMode);
    const int     signAng            = intraPredAngleMode < 0 ? -1 : 1;
                  absAng             = angTable  [absAngMode];

    m_ipaParam.absInvAngle           = invAngTable[absAngMode];
    m_ipaParam.intraPredAngle        = signAng * absAng;
    if (intraPredAngleMode < 0)
    {
      m_ipaParam.applyPDPC = false;
    }
    else if (intraPredAngleMode > 0)
    {
      const int sideSize = m_ipaParam.isModeVer ? puSize.height : puSize.width;
      const int maxScale = 2;

      m_ipaParam.angularScale = std::min(maxScale, floorLog2(sideSize) - (floorLog2(3 * m_ipaParam.absInvAngle - 2) - 8));
      m_ipaParam.applyPDPC &= m_ipaParam.angularScale >= 0;
    }
  }

  // high level conditions and DC intra prediction
  if(   sps.spsRExt.intraSmoothingDisabled
    || !isLuma( chType )
    || useISP
    || PU::isMIP( pu, chType ) //th remove this
    || m_ipaParam.multiRefIndex
    || DC_IDX == dirMode
    )
  {
  }
  else if (isLuma( chType ) && pu.cu->bdpcmMode) // BDPCM
  {
    m_ipaParam.refFilterFlag = false;
  }
  else if (dirMode == PLANAR_IDX) // Planar intra prediction
  {
    m_ipaParam.refFilterFlag = puSize.width * puSize.height > 32 ? true : false;
  }
  else if (!useISP)// HOR, VER and angular modes (MDIS)
  {
    bool filterFlag = false;
    {
      const int diff = std::min<int>( abs( predMode - HOR_IDX ), abs( predMode - VER_IDX ) );
      const int log2Size = (Log2(puSize.width * puSize.height) >> 1);
      CHECK( log2Size >= MAX_INTRA_FILTER_DEPTHS, "Size not supported" );
      filterFlag = (diff > m_aucIntraFilter[log2Size]);
    }

    // Selelection of either ([1 2 1] / 4 ) refrence filter OR Gaussian 4-tap interpolation filter
    if (filterFlag)
    {
      const bool isRefFilter       =  isIntegerSlope(absAng);
      CHECK( puSize.width * puSize.height <= 32, "DCT-IF interpolation filter is always used for 4x4, 4x8, and 8x4 luma CB" );
      m_ipaParam.refFilterFlag     =  isRefFilter;
      m_ipaParam.interpolationFlag = !isRefFilter;
    }
  }
}

#include "CommonDefX86.h"


/** Function for deriving the simplified angular intra predictions.
*
* This function derives the prediction samples for the angular mode based on the prediction direction indicated by
* the prediction mode index. The prediction direction is given by the displacement of the bottom row of the block and
* the reference row above the block in the case of vertical prediction or displacement of the rightmost column
* of the block and reference column left from the block in the case of the horizontal prediction. The displacement
* is signalled at 1/32 pixel accuracy. When projection of the predicted pixel falls inbetween reference samples,
* the predicted value for the pixel is linearly interpolated from the reference samples. All reference samples are taken
* from the extended main reference.
*/
//NOTE: Bit-Limit - 25-bit source

void IntraPrediction::xPredIntraAng( PelBuf& pDst, const CPelBuf& pSrc, const ChannelType channelType, const ClpRng& clpRng)
{
  int width =int(pDst.width);
  int height=int(pDst.height);

  const bool bIsModeVer     = m_ipaParam.isModeVer;
  const int  multiRefIdx    = m_ipaParam.multiRefIndex;
  const int  intraPredAngle = m_ipaParam.intraPredAngle;
  const int  absInvAngle    = m_ipaParam.absInvAngle;

  Pel* refMain;
  Pel* refSide;

  Pel  refAbove[2 * MAX_CU_SIZE + 3 + 33 * MAX_REF_LINE_IDX];
  Pel  refLeft [2 * MAX_CU_SIZE + 3 + 33 * MAX_REF_LINE_IDX];

  // Initialize the Main and Left reference array.
  if (intraPredAngle < 0)
  {
    memcpy(&refAbove[height],pSrc.buf,(width + 2 + multiRefIdx)*sizeof(Pel));
    for (int y = 0; y <= height + 1 + multiRefIdx; y++)
    {
      refLeft[y + width] = pSrc.at(y, 1);
    }
    refMain = bIsModeVer ? refAbove + height : refLeft + width;
    refSide = bIsModeVer ? refLeft + width : refAbove + height;

    // Extend the Main reference to the left.
    int sizeSide = bIsModeVer ? height : width;
    for (int k = -sizeSide; k <= -1; k++)
    {
      refMain[k] = refSide[std::min((-k * absInvAngle + 256) >> 9, sizeSide)];
    }
  }
  else
  {
    memcpy(&refAbove[0],pSrc.buf,((width<<1) + multiRefIdx+1)*sizeof(Pel));
    for (int y = 0; y <= 2 * height + multiRefIdx; y++)
    {
      refLeft[y] = pSrc.at(y,1);
    }

    refMain = bIsModeVer ? refAbove : refLeft;
    refSide = bIsModeVer ? refLeft : refAbove;

    // Extend main reference to right using replication
    const int log2Ratio = Log2(width) - Log2(height);
    const int s         = std::max<int>(0, bIsModeVer ? log2Ratio : -log2Ratio);
    const int maxIndex  = (multiRefIdx << s) + 2;
    const int refLength = 2* (bIsModeVer ? width : height);
    const Pel val       = refMain[refLength + multiRefIdx];
    for (int z = 1; z <= maxIndex; z++)
    {
      refMain[refLength + multiRefIdx + z] = val;
    }
  }

  // swap width/height if we are doing a horizontal mode:
  if (!bIsModeVer)
  {
    std::swap(width, height);
  }
  Pel tempArray[MAX_CU_SIZE*MAX_CU_SIZE];
  const int dstStride = bIsModeVer ? pDst.stride : MAX_CU_SIZE;
  Pel* pDstBuf = bIsModeVer ? pDst.buf : tempArray;

  // compensate for line offset in reference line buffers
  refMain += multiRefIdx;
  refSide += multiRefIdx;

  Pel* pDsty = pDstBuf;

  if( intraPredAngle == 0 )  // pure vertical or pure horizontal
  {
    if (m_ipaParam.applyPDPC)
    {
      const int scale   = (Log2(width * height) - 2) >> 2;
      IntraHorVerPDPC(pDsty,dstStride,refSide,width,height,scale,refMain,clpRng);
    }
    else
    {
      for( int y = 0; y < height; y++ )
      {
        memcpy(pDsty,&refMain[1],width*sizeof(Pel));
        pDsty += dstStride;
      }
    }
  }
  else
  {
    if ( !isIntegerSlope( abs(intraPredAngle) ) )
    {
      int deltaPos = intraPredAngle * (1 + multiRefIdx);
      if( isLuma(channelType) )
      {
        IntraPredAngleLuma( pDstBuf,dstStride,refMain,width,height,deltaPos,intraPredAngle,nullptr,!m_ipaParam.interpolationFlag,clpRng );
      }
      else
      {
        IntraPredAngleChroma(pDstBuf,dstStride,refMain,width,height,deltaPos,intraPredAngle);
      }
    }
    else
    {
      for (int y = 0, deltaPos = intraPredAngle * (1 + multiRefIdx); y<height; y++, deltaPos += intraPredAngle, pDsty += dstStride)
      {
        const int deltaInt   = deltaPos >> 5;
        // Just copy the integer samples
        memcpy(pDsty,refMain  + deltaInt + 1,width*sizeof(Pel));
      }
    }

    if (m_ipaParam.applyPDPC)
    {
      pDsty = pDstBuf;
      IntraAnglePDPC(pDsty,dstStride,refSide,width,height,m_ipaParam.angularScale,absInvAngle);
    }
  } // else

  // Flip the block if this is the horizontal mode
  if( !bIsModeVer )
  {
    pDst.transposedFrom( CPelBuf( pDstBuf, dstStride, width, height) );
  }
}


inline bool isAboveLeftAvailable  ( const CodingUnit &cu, const ChannelType& chType, const Position& posLT );
inline int  isAboveAvailable      ( const CodingUnit &cu, const ChannelType& chType, const Position& posLT, const uint32_t uiNumUnitsInPU, const uint32_t unitWidth, bool *validFlags );
inline int  isLeftAvailable       ( const CodingUnit &cu, const ChannelType& chType, const Position& posLT, const uint32_t uiNumUnitsInPU, const uint32_t unitWidth, bool *validFlags );
inline int  isAboveRightAvailable ( const CodingUnit &cu, const ChannelType& chType, const Position& posRT, const uint32_t uiNumUnitsInPU, const uint32_t unitHeight, bool *validFlags );
inline int  isBelowLeftAvailable  ( const CodingUnit &cu, const ChannelType& chType, const Position& posLB, const uint32_t uiNumUnitsInPU, const uint32_t unitHeight, bool *validFlags );

void IntraPrediction::initIntraPatternChType(const CodingUnit &cu, const CompArea& area, const bool forceRefFilterFlag)
{
  const CodingStructure& cs   = *cu.cs;

  if (!forceRefFilterFlag)
  {
    initPredIntraParams(*cu.pu, area, *cs.sps);
  }

  Pel *refBufUnfiltered = m_refBuffer[area.compID][PRED_BUF_UNFILTERED];
  Pel *refBufFiltered   = m_refBuffer[area.compID][PRED_BUF_FILTERED];

  // ----- Step 1: unfiltered reference samples -----
  xFillReferenceSamples( cs.picture->getRecoBuf( area ), refBufUnfiltered, area, cu );
  // ----- Step 2: filtered reference samples -----
  if( m_ipaParam.refFilterFlag || forceRefFilterFlag )
  {
    xFilterReferenceSamples( refBufUnfiltered, refBufFiltered, area, *cs.sps, cu.pu->multiRefIdx );
  }
}

void IntraPrediction::reset()
{
  m_lastCh = MAX_NUM_CH;
  m_lastArea = Area(0,0,0,0);
}

void IntraPrediction::xFillReferenceSamples( const CPelBuf& recoBuf, Pel* refBufUnfiltered, const CompArea& area, const CodingUnit &cu )
{
  const ChannelType      chType = toChannelType( area.compID );
  const CodingStructure &cs     = *cu.cs;
  const SPS             &sps    = *cs.sps;
  const PreCalcValues   &pcv    = *cs.pcv;

  const int multiRefIdx         = (area.compID == COMP_Y) ? cu.pu->multiRefIdx : 0;

  const int  tuWidth            = area.width;
  const int  tuHeight           = area.height;
  const int  predSize           = 2*area.width;
  const int  predHSize          = 2*area.height;
  const int predStride = predSize + 1 + multiRefIdx;
  m_refBufferStride[area.compID] = predStride;

  const bool noShift            = pcv.noChroma2x2 && area.width == 4; // don't shift on the lowest level (chroma not-split)
  const int  unitWidth          = tuWidth  <= 2 && cu.ispMode && isLuma(area.compID) ? tuWidth  : pcv.minCUSize >> (noShift ? 0 : getComponentScaleX(area.compID, sps.chromaFormatIdc));
  const int  unitHeight         = tuHeight <= 2 && cu.ispMode && isLuma(area.compID) ? tuHeight : pcv.minCUSize >> (noShift ? 0 : getComponentScaleY(area.compID, sps.chromaFormatIdc));

  const int  totalAboveUnits    = (predSize + (unitWidth - 1)) / unitWidth;
  const int  totalLeftUnits     = (predHSize + (unitHeight - 1)) / unitHeight;
  const int  totalUnits         = totalAboveUnits + totalLeftUnits + 1; //+1 for top-left

  if( m_lastArea != area || m_lastCh != chType )
  {
    m_lastCh = chType;
    m_lastArea = area;
    const int  numAboveUnits      = std::max<int>( tuWidth / unitWidth, 1 );
    const int  numLeftUnits       = std::max<int>( tuHeight / unitHeight, 1 );
    const int  numAboveRightUnits = totalAboveUnits - numAboveUnits;
    const int  numLeftBelowUnits  = totalLeftUnits - numLeftUnits;

    CHECK( numAboveUnits <= 0 || numLeftUnits <= 0 || numAboveRightUnits <= 0 || numLeftBelowUnits <= 0, "Size not supported" );

    // ----- Step 1: analyze neighborhood -----
    const Position posLT          = area;
    const Position posRT          = area.topRight();
    const Position posLB          = area.bottomLeft();

    m_numIntraNeighbor = 0;

    memset( m_neighborFlags, 0, totalUnits );

    m_neighborFlags[totalLeftUnits] = isAboveLeftAvailable( cu, chType, posLT );
    m_numIntraNeighbor += m_neighborFlags[totalLeftUnits] ? 1 : 0;
    m_numIntraNeighbor += isAboveAvailable     ( cu, chType, posLT, numAboveUnits,      unitWidth,  (m_neighborFlags + totalLeftUnits + 1) );
    m_numIntraNeighbor += isAboveRightAvailable( cu, chType, posRT, numAboveRightUnits, unitWidth,  (m_neighborFlags + totalLeftUnits + 1 + numAboveUnits) );
    m_numIntraNeighbor += isLeftAvailable      ( cu, chType, posLT, numLeftUnits,       unitHeight, (m_neighborFlags + totalLeftUnits - 1) );
    m_numIntraNeighbor += isBelowLeftAvailable ( cu, chType, posLB, numLeftBelowUnits,  unitHeight, (m_neighborFlags + totalLeftUnits - 1 - numLeftUnits) );
  }
  // ----- Step 2: fill reference samples (depending on neighborhood) -----

  const Pel*  srcBuf    = recoBuf.buf;
  const int   srcStride = recoBuf.stride;
        Pel*  ptrDst    = refBufUnfiltered;
  const Pel*  ptrSrc;
  const Pel   valueDC   = 1 << (sps.bitDepths[ chType ] - 1);


  if( m_numIntraNeighbor == 0 )
  {
    // Fill border with DC value
    for (int j = 0; j <= predSize + multiRefIdx; j++) { ptrDst[j] = valueDC; }
    for (int i = 0; i <= predHSize + multiRefIdx; i++) { ptrDst[i+predStride] = valueDC; }
  }
  else if( m_numIntraNeighbor == totalUnits )
  {
    // Fill top-left border and top and top right with rec. samples
    ptrSrc = srcBuf - (1 + multiRefIdx) * srcStride - (1 + multiRefIdx);
    for (int j = 0; j <= predSize + multiRefIdx; j++) { ptrDst[j] = ptrSrc[j]; }
    for (int i = 0; i <= predHSize + multiRefIdx; i++)
    {
      ptrDst[i + predStride] = ptrSrc[i * srcStride];
    }
  }
  else // reference samples are partially available
  {
    // Fill top-left sample(s) if available
    ptrSrc = srcBuf - (1 + multiRefIdx) * srcStride - (1 + multiRefIdx);
    ptrDst = refBufUnfiltered;
    if (m_neighborFlags[totalLeftUnits])
    {
      ptrDst[0] = ptrSrc[0];
      ptrDst[predStride] = ptrSrc[0];
      for (int i = 1; i <= multiRefIdx; i++)
      {
        ptrDst[i] = ptrSrc[i];
        ptrDst[i + predStride] = ptrSrc[i * srcStride];
      }
    }

    // Fill left & below-left samples if available (downwards)
    ptrSrc += (1 + multiRefIdx) * srcStride;
    ptrDst += (1 + multiRefIdx) + predStride;
    for (int unitIdx = totalLeftUnits - 1; unitIdx > 0; unitIdx--)
    {
      if (m_neighborFlags[unitIdx])
      {
        for (int i = 0; i < unitHeight; i++)
        {
          ptrDst[i] = ptrSrc[i*srcStride];
        }
      }
      ptrSrc += unitHeight * srcStride;
      ptrDst += unitHeight;
    }
    // Fill last below-left sample(s)
    if (m_neighborFlags[0])
    {
      int lastSample = (predHSize % unitHeight == 0) ? unitHeight : predHSize % unitHeight;
      for (int i = 0; i < lastSample; i++)
      {
        ptrDst[i] = ptrSrc[i*srcStride];
      }
    }

    // Fill above & above-right samples if available (left-to-right)
    ptrSrc = srcBuf - srcStride * (1 + multiRefIdx);
    ptrDst = refBufUnfiltered + 1 + multiRefIdx;
    for (int unitIdx = totalLeftUnits + 1; unitIdx < totalUnits - 1; unitIdx++)
    {
      if (m_neighborFlags[unitIdx])
      {
        memcpy(ptrDst,ptrSrc,unitWidth*sizeof(Pel));
      }
      ptrSrc += unitWidth;
      ptrDst += unitWidth;
    }
    // Fill last above-right sample(s)
    if (m_neighborFlags[totalUnits - 1])
    {
      int lastSample = (predSize % unitWidth == 0) ? unitWidth : predSize % unitWidth;
      memcpy(ptrDst,ptrSrc,lastSample*sizeof(Pel));
    }

    // pad from first available down to the last below-left
    ptrDst = refBufUnfiltered;
    int lastAvailUnit = 0;
    if (!m_neighborFlags[0])
    {
      int firstAvailUnit = 1;
      while (firstAvailUnit < totalUnits && !m_neighborFlags[firstAvailUnit])
      {
        firstAvailUnit++;
      }

      // first available sample
      int firstAvailRow = -1;
      int firstAvailCol = 0;
      if (firstAvailUnit < totalLeftUnits)
      {
        firstAvailRow = (totalLeftUnits - firstAvailUnit) * unitHeight + multiRefIdx;
      }
      else if (firstAvailUnit == totalLeftUnits)
      {
        firstAvailRow = multiRefIdx;
      }
      else
      {
        firstAvailCol = (firstAvailUnit - totalLeftUnits - 1) * unitWidth + 1 + multiRefIdx;
      }
      const Pel firstAvailSample = ptrDst[firstAvailRow < 0 ? firstAvailCol : firstAvailRow + predStride];

      // last sample below-left (n.a.)
      int lastRow = predHSize + multiRefIdx;

      // fill left column
      for (int i = lastRow; i > firstAvailRow; i--)
      {
        ptrDst[i + predStride] = firstAvailSample;
      }
      // fill top row
      if (firstAvailCol > 0)
      {
        for (int j = 0; j < firstAvailCol; j++)
        {
          ptrDst[j] = firstAvailSample;
        }
      }
      lastAvailUnit = firstAvailUnit;
    }

    // pad all other reference samples.
    int currUnit = lastAvailUnit + 1;
    while (currUnit < totalUnits)
    {
      if (!m_neighborFlags[currUnit]) // samples not available
      {
        // last available sample
        int lastAvailRow = -1;
        int lastAvailCol = 0;
        if (lastAvailUnit < totalLeftUnits)
        {
          lastAvailRow = (totalLeftUnits - lastAvailUnit - 1) * unitHeight + multiRefIdx + 1;
        }
        else if (lastAvailUnit == totalLeftUnits)
        {
          lastAvailCol = multiRefIdx;
        }
        else
        {
          lastAvailCol = (lastAvailUnit - totalLeftUnits) * unitWidth + multiRefIdx;
        }
        const Pel lastAvailSample = ptrDst[lastAvailRow < 0 ? lastAvailCol : lastAvailRow + predStride];

        // fill current unit with last available sample
        if (currUnit < totalLeftUnits)
        {
          for (int i = lastAvailRow - 1; i >= lastAvailRow - unitHeight; i--)
          {
            ptrDst[i + predStride] = lastAvailSample;
          }
        }
        else if (currUnit == totalLeftUnits)
        {
          for (int i = 0; i < multiRefIdx + 1; i++)
          {
            ptrDst[i + predStride] = lastAvailSample;
          }
          for (int j = 0; j < multiRefIdx + 1; j++)
          {
            ptrDst[j] = lastAvailSample;
          }
        }
        else
        {
          int numSamplesInUnit = (currUnit == totalUnits - 1) ? ((predSize % unitWidth == 0) ? unitWidth : predSize % unitWidth) : unitWidth;
          for (int j = lastAvailCol + 1; j <= lastAvailCol + numSamplesInUnit; j++)
          {
            ptrDst[j] = lastAvailSample;
          }
        }
      }
      lastAvailUnit = currUnit;
      currUnit++;
    }
  }
}

void IntraPrediction::xFilterReferenceSamples( const Pel* refBufUnfiltered, Pel* refBufFiltered, const CompArea& area, const SPS &sps
  , int multiRefIdx
  , int stride
)
{
  if (area.compID != COMP_Y)
  {
    multiRefIdx = 0;
  }
  const int predSize = 2*area.width + multiRefIdx;
  const int predHSize = 2*area.height + multiRefIdx;
  const int predStride = stride == 0 ? predSize + 1 : stride;


  const Pel topLeft =
    (refBufUnfiltered[0] + refBufUnfiltered[1] + refBufUnfiltered[predStride] + refBufUnfiltered[predStride + 1] + 2)
    >> 2;

  refBufFiltered[0] = topLeft;

  for (int i = 1; i < predSize; i++)
  {
    refBufFiltered[i] = (refBufUnfiltered[i - 1] + 2 * refBufUnfiltered[i] + refBufUnfiltered[i + 1] + 2) >> 2;
  }
  refBufFiltered[predSize] = refBufUnfiltered[predSize];

  refBufFiltered += predStride;
  refBufUnfiltered += predStride;

  refBufFiltered[0] = topLeft;

  for (int i = 1; i < predHSize; i++)
  {
    refBufFiltered[i] = (refBufUnfiltered[i - 1] + 2 * refBufUnfiltered[i] + refBufUnfiltered[i + 1] + 2) >> 2;
  }
  refBufFiltered[predHSize] = refBufUnfiltered[predHSize];
}

bool isAboveLeftAvailable(const CodingUnit &cu, const ChannelType& chType, const Position& posLT)
{
  const CodingStructure& cs = *cu.cs;
  const Position refPos = posLT.offset(-1, -1);

  if (!cs.isDecomp(refPos, chType))
  {
    return false;
  }

  return (cs.getCURestricted(refPos, cu, chType) != NULL);
}

int isAboveAvailable(const CodingUnit &cu, const ChannelType& chType, const Position& posLT, const uint32_t uiNumUnitsInPU, const uint32_t unitWidth, bool *bValidFlags)
{
  const CodingStructure& cs = *cu.cs;

  bool *    validFlags  = bValidFlags;
  int       numIntra    = 0;
  const int maxDx       = uiNumUnitsInPU * unitWidth;
  unsigned  checkPosX   = 0;
  bool      valid       = false;

  for (int dx = 0; dx < maxDx; dx += unitWidth)
  {
    if( dx >= checkPosX )
    {
      const Position refPos = posLT.offset(dx, -1);

      if (!cs.isDecomp(refPos, chType))
      {
        break;
      }

      const CodingUnit* cuN = cs.getCURestricted(refPos, cu, chType);
      valid = (cuN != NULL);
      if( cuN ) checkPosX = chType == CH_C ? (cuN->Cb().x + cuN->Cb().width - posLT.x) : (cuN->Y().x + cuN->Y().width - posLT.x);
    }

    numIntra += valid ? 1 : 0;
    *validFlags = valid;

    validFlags++;
  }

  return numIntra;
}

int isLeftAvailable(const CodingUnit &cu, const ChannelType& chType, const Position& posLT, const uint32_t uiNumUnitsInPU, const uint32_t unitHeight, bool *bValidFlags)
{
  const CodingStructure& cs = *cu.cs;

  bool *    validFlags = bValidFlags;
  int       numIntra   = 0;
  const int maxDy      = uiNumUnitsInPU * unitHeight;
  unsigned checkPosY   = 0;
  bool     valid       = false;

  for (int dy = 0; dy < maxDy; dy += unitHeight)
  {
    if( dy >= checkPosY )
    {
      const Position refPos = posLT.offset(-1, dy);

      if (!cs.isDecomp(refPos, chType))
      {
        break;
      }

      const CodingUnit* cuN = cs.getCURestricted(refPos, cu, chType);
      valid = (cuN != NULL);
      if( cuN ) checkPosY = chType == CH_C ? (cuN->Cb().y + cuN->Cb().height - posLT.y) : (cuN->Y().y + cuN->Y().height - posLT.y);
    }
    numIntra += valid ? 1 : 0;
    *validFlags = valid;

    validFlags--;
  }

  return numIntra;
}

int isAboveRightAvailable(const CodingUnit &cu, const ChannelType& chType, const Position& posRT, const uint32_t uiNumUnitsInPU, const uint32_t unitWidth, bool *bValidFlags )
{
  const CodingStructure& cs = *cu.cs;

  bool *    validFlags = bValidFlags;
  int       numIntra   = 0;
  const int maxDx      = uiNumUnitsInPU * unitWidth;
  unsigned  checkPosX   = 0;
  bool      valid       = false;

  for (int dx = 0; dx < maxDx; dx += unitWidth)
  {
    if( dx >= checkPosX )
    {
      const Position refPos = posRT.offset(unitWidth + dx, -1);

      if (!cs.isDecomp(refPos, chType))
      {
        break;
      }

      const CodingUnit* cuN = cs.getCURestricted(refPos, cu, chType);
      valid = (cuN != NULL);
      if(cuN) checkPosX = chType == CH_C ? (cuN->Cb().x + cuN->Cb().width - (posRT.x + unitWidth)) : (cuN->Y().x + cuN->Y().width - (posRT.x + unitWidth));
    }
    numIntra += valid ? 1 : 0;
    *validFlags = valid;

    validFlags++;
  }

  return numIntra;
}

int isBelowLeftAvailable(const CodingUnit &cu, const ChannelType& chType, const Position& posLB, const uint32_t uiNumUnitsInPU, const uint32_t unitHeight, bool *bValidFlags )
{
  const CodingStructure& cs = *cu.cs;

  bool *    validFlags = bValidFlags;
  int       numIntra   = 0;
  const int maxDy      = uiNumUnitsInPU * unitHeight;
  unsigned  checkPosY   = 0;
  bool      valid       = false;

  for (int dy = 0; dy < maxDy; dy += unitHeight)
  {
    if( dy >= checkPosY )
    {
      const Position refPos = posLB.offset(-1, unitHeight + dy);

      if (!cs.isDecomp(refPos, chType))
      {
        break;
      }

      const CodingUnit* cuN = cs.getCURestricted(refPos, cu, chType);
      valid = (cuN != NULL);
      if( cuN ) checkPosY = chType == CH_C ? (cuN->Cb().y + cuN->Cb().height - (posLB.y + unitHeight)) : (cuN->Y().y + cuN->Y().height - (posLB.y + unitHeight));
    }
    numIntra += valid ? 1 : 0;
    *validFlags = valid;

    validFlags--;
  }

  return numIntra;
}

// LumaRecPixels
void IntraPrediction::loadLMLumaRecPels(const PredictionUnit &pu, const CompArea& chromaArea )
{
  int iDstStride = 2 * MAX_TB_SIZEY + 1;
  Pel* pDst0 = m_pMdlmTemp + iDstStride + 1;
  //assert 420 chroma subsampling
  CompArea lumaArea = CompArea( COMP_Y, pu.chromaFormat, chromaArea.lumaPos(), recalcSize( pu.chromaFormat, CH_C, CH_L, chromaArea.size() ) );//needed for correct pos/size (4x4 Tus)

  CHECK(lumaArea.width == chromaArea.width && CHROMA_444 != pu.chromaFormat, "");
  CHECK(lumaArea.height == chromaArea.height && CHROMA_444 != pu.chromaFormat && CHROMA_422 != pu.chromaFormat, "");

  const SizeType uiCWidth = chromaArea.width;
  const SizeType uiCHeight = chromaArea.height;

  const CPelBuf Src = pu.cs->picture->getRecoBuf( lumaArea );
  Pel const* pRecSrc0   = Src.bufAt( 0, 0 );
  int iRecStride        = Src.stride;
  int logSubWidthC  = getChannelTypeScaleX(CH_C, pu.chromaFormat);
  int logSubHeightC = getChannelTypeScaleY(CH_C, pu.chromaFormat);

  int iRecStride2       = iRecStride << logSubHeightC;

  const CodingUnit& lumaCU = isChroma( pu.chType ) ? *pu.cs->picture->cs->getCU( lumaArea.pos(), CH_L, TREE_D ) : *pu.cu;
  const CodingUnit&     cu = *pu.cu;

  const CompArea& area = isChroma( pu.chType ) ? chromaArea : lumaArea;

  const uint32_t uiTuWidth  = area.width;
  const uint32_t uiTuHeight = area.height;

  const int  unitWidthLog2  = MIN_CU_LOG2 - getComponentScaleX( area.compID, area.chromaFormat );
  const int  unitHeightLog2 = MIN_CU_LOG2 - getComponentScaleY( area.compID, area.chromaFormat );
  const int  unitWidth  = 1<<unitWidthLog2;
  const int  unitHeight = 1<<unitHeightLog2;

  const int  iTUWidthInUnits  = uiTuWidth >> unitWidthLog2;
  const int  iTUHeightInUnits = uiTuHeight >> unitHeightLog2;
  const int  iAboveUnits      = iTUWidthInUnits;
  const int  iLeftUnits       = iTUHeightInUnits;

  const int  chromaUnitWidthLog2  = MIN_CU_LOG2 - logSubWidthC;
  const int  chromaUnitHeightLog2 = MIN_CU_LOG2 - logSubHeightC;
  const int  chromaUnitWidth = 1<<chromaUnitWidthLog2;
  const int  chromaUnitHeight = 1<<chromaUnitHeightLog2;
  const int  topTemplateSampNum = 2 * uiCWidth; // for MDLM, the number of template samples is 2W or 2H.
  const int  leftTemplateSampNum = 2 * uiCHeight;
  const int  totalAboveUnits = (topTemplateSampNum + (chromaUnitWidth - 1)) >> chromaUnitWidthLog2;
  const int  totalLeftUnits = (leftTemplateSampNum + (chromaUnitHeight - 1)) >> chromaUnitHeightLog2;
  const int  totalUnits = totalLeftUnits + totalAboveUnits + 1;
  const int  aboveRightUnits = totalAboveUnits - iAboveUnits;
  const int  leftBelowUnits = totalLeftUnits - iLeftUnits;

  int avaiAboveRightUnits = 0;
  int avaiLeftBelowUnits = 0;
  bool  bNeighborFlags[4 * MAX_NUM_PART_IDXS_IN_CTU_WIDTH + 1];
  memset(bNeighborFlags, 0, totalUnits);
  bool aboveIsAvailable, leftIsAvailable;
  const CodingUnit& chromaCU = isChroma( pu.chType ) ? cu : lumaCU;
  const ChannelType areaCh = toChannelType( area.compID );

  int availlableUnit = isLeftAvailable(chromaCU, areaCh, area.pos(), iLeftUnits, unitHeight, (bNeighborFlags + iLeftUnits + leftBelowUnits - 1));

  leftIsAvailable = availlableUnit == iTUHeightInUnits;

  availlableUnit = isAboveAvailable(chromaCU, areaCh, area.pos(), iAboveUnits, unitWidth, (bNeighborFlags + iLeftUnits + leftBelowUnits + 1));

  aboveIsAvailable = availlableUnit == iTUWidthInUnits;

  if (leftIsAvailable)   // if left is not available, then the below left is not available
  {
    avaiLeftBelowUnits = isBelowLeftAvailable(chromaCU, areaCh, area.bottomLeftComp(area.compID), leftBelowUnits, unitHeight, (bNeighborFlags + leftBelowUnits - 1));
  }

  if (aboveIsAvailable)   // if above is not available, then  the above right is not available.
  {
    avaiAboveRightUnits = isAboveRightAvailable(chromaCU, areaCh, area.topRightComp(area.compID), aboveRightUnits, unitWidth, (bNeighborFlags + iLeftUnits + leftBelowUnits + iAboveUnits + 1));
  }

  Pel*       pDst  = nullptr;
  Pel const* piSrc = nullptr;

  bool isFirstRowOfCtu = (lumaArea.y & ((pu.cs->sps)->CTUSize - 1)) == 0;

  if (aboveIsAvailable)
  {
    pDst  = pDst0    - iDstStride;
    int addedAboveRight = 0;
    if ((pu.intraDir[1] == MDLM_L_IDX) || (pu.intraDir[1] == MDLM_T_IDX))
    {
      addedAboveRight = avaiAboveRightUnits*chromaUnitWidth;
    }
    for (int i = 0; i < uiCWidth + addedAboveRight; i++)
    {
      const bool leftPadding = i == 0 && !leftIsAvailable;
      if (pu.chromaFormat == CHROMA_444)
      {
        piSrc = pRecSrc0 - iRecStride;
        pDst[i] = piSrc[i];
      }
      else if (isFirstRowOfCtu)
      {
        piSrc   = pRecSrc0 - iRecStride;
        pDst[i] = (piSrc[2 * i] * 2 + piSrc[2 * i - (leftPadding ? 0 : 1)] + piSrc[2 * i + 1] + 2) >> 2;
      }
      else if (pu.chromaFormat == CHROMA_422)
      {
        piSrc = pRecSrc0 - iRecStride2;

        int s = 2;
        s += piSrc[2 * i] * 2;
        s += piSrc[2 * i - (leftPadding ? 0 : 1)];
        s += piSrc[2 * i + 1];
        pDst[i] = s >> 2;
      }
      else if (pu.cs->sps->verCollocatedChroma )
      {
        piSrc = pRecSrc0 - iRecStride2;

        int s = 4;
        s += piSrc[2 * i - iRecStride];
        s += piSrc[2 * i] * 4;
        s += piSrc[2 * i - (leftPadding ? 0 : 1)];
        s += piSrc[2 * i + 1];
        s += piSrc[2 * i + iRecStride];
        pDst[i] = s >> 3;
      }
      else
      {
        piSrc = pRecSrc0 - iRecStride2;
        int s = 4;
        s += piSrc[2 * i] * 2;
        s += piSrc[2 * i + 1];
        s += piSrc[2 * i - (leftPadding ? 0 : 1)];
        s += piSrc[2 * i + iRecStride] * 2;
        s += piSrc[2 * i + 1 + iRecStride];
        s += piSrc[2 * i + iRecStride - (leftPadding ? 0 : 1)];
        pDst[i] = s >> 3;
      }
    }
  }

  if (leftIsAvailable)
  {
    pDst  = pDst0    - 1;
    piSrc = pRecSrc0 - 1 - logSubWidthC;

    int addedLeftBelow = 0;
    if ((pu.intraDir[1] == MDLM_L_IDX) || (pu.intraDir[1] == MDLM_T_IDX))
    {
      addedLeftBelow = avaiLeftBelowUnits*chromaUnitHeight;
    }

    for (int j = 0; j < uiCHeight + addedLeftBelow; j++)
    {
      if (pu.chromaFormat == CHROMA_444)
      {
        pDst[0] = piSrc[0];
      }
      else if (pu.chromaFormat == CHROMA_422)
      {
        int s = 2;
        s += piSrc[0] * 2;
        s += piSrc[-1];
        s += piSrc[1];
        pDst[0] = s >> 2;
      }
      else if (pu.cs->sps->verCollocatedChroma)
      {
        const bool abovePadding = j == 0 && !aboveIsAvailable;

        int s = 4;
        s += piSrc[-(abovePadding ? 0 : iRecStride)];
        s += piSrc[0] * 4;
        s += piSrc[-1];
        s += piSrc[1];
        s += piSrc[iRecStride];
        pDst[0] = s >> 3;
      }
      else
      {
        int s = 4;
        s += piSrc[0] * 2;
        s += piSrc[1];
        s += piSrc[-1];
        s += piSrc[iRecStride] * 2;
        s += piSrc[iRecStride + 1];
        s += piSrc[iRecStride - 1];
        pDst[0] = s >> 3;
      }

      piSrc += iRecStride2;
      pDst  += iDstStride;
    }
  }

  // inner part from reconstructed picture buffer
  for( int j = 0; j < uiCHeight; j++ )
  {
    for( int i = 0; i < uiCWidth; i++ )
    {
      if (pu.chromaFormat == CHROMA_444)
      {
        pDst0[i] = pRecSrc0[i];
      }
      else if (pu.chromaFormat == CHROMA_422)
      {
        const bool leftPadding  = i == 0 && !leftIsAvailable;

        int s = 2;
        s += pRecSrc0[2 * i] * 2;
        s += pRecSrc0[2 * i - (leftPadding ? 0 : 1)];
        s += pRecSrc0[2 * i + 1];
        pDst0[i] = s >> 2;
      }
      else if (pu.cs->sps->verCollocatedChroma)
      {
        const bool leftPadding  = i == 0 && !leftIsAvailable;
        const bool abovePadding = j == 0 && !aboveIsAvailable;

        int s = 4;
        s += pRecSrc0[2 * i - (abovePadding ? 0 : iRecStride)];
        s += pRecSrc0[2 * i] * 4;
        s += pRecSrc0[2 * i - (leftPadding ? 0 : 1)];
        s += pRecSrc0[2 * i + 1];
        s += pRecSrc0[2 * i + iRecStride];
        pDst0[i] = s >> 3;
      }
      else
      {
        CHECK(pu.chromaFormat != CHROMA_420, "Chroma format must be 4:2:0 for vertical filtering");
        const bool leftPadding = i == 0 && !leftIsAvailable;

        int s = 4;
        s += pRecSrc0[2 * i] * 2;
        s += pRecSrc0[2 * i + 1];
        s += pRecSrc0[2 * i - (leftPadding ? 0 : 1)];
        s += pRecSrc0[2 * i + iRecStride] * 2;
        s += pRecSrc0[2 * i + 1 + iRecStride];
        s += pRecSrc0[2 * i + iRecStride - (leftPadding ? 0 : 1)];
        pDst0[i] = s >> 3;
      }
    }

    pDst0    += iDstStride;
    pRecSrc0 += iRecStride2;
  }
}

void IntraPrediction::xGetLMParameters(const PredictionUnit &pu, const ComponentID compID,
                                              const CompArea& chromaArea,
                                              int& a, int& b, int& iShift)
{
  CHECK(compID == COMP_Y, "");

  const SizeType cWidth  = chromaArea.width;
  const SizeType cHeight = chromaArea.height;

  const Position posLT = chromaArea;

  CodingStructure & cs = *(pu.cs);
  const CodingUnit &cu = *(pu.cu);

  const SPS &        sps           = *cs.sps;
  const uint32_t     tuWidth     = chromaArea.width;
  const uint32_t     tuHeight    = chromaArea.height;
  const ChromaFormat nChromaFormat = sps.chromaFormatIdc;

  const int unitWidthLog2    = MIN_CU_LOG2 - getComponentScaleX(chromaArea.compID, nChromaFormat);
  const int unitHeightLog2   = MIN_CU_LOG2 - getComponentScaleY(chromaArea.compID, nChromaFormat);
  const int unitWidth    = 1<<unitWidthLog2;
  const int unitHeight   = 1<<unitHeightLog2;

  const int tuWidthInUnits  = tuWidth >> unitWidthLog2;
  const int tuHeightInUnits = tuHeight >> unitHeightLog2;
  const int aboveUnits      = tuWidthInUnits;
  const int leftUnits       = tuHeightInUnits;
  int topTemplateSampNum = 2 * cWidth; // for MDLM, the template sample number is 2W or 2H;
  int leftTemplateSampNum = 2 * cHeight;
  int totalAboveUnits = (topTemplateSampNum + (unitWidth - 1)) >> unitWidthLog2;
  int totalLeftUnits = (leftTemplateSampNum + (unitHeight - 1)) >> unitHeightLog2;
  int totalUnits = totalLeftUnits + totalAboveUnits + 1;
  int aboveRightUnits = totalAboveUnits - aboveUnits;
  int leftBelowUnits = totalLeftUnits - leftUnits;
  int avaiAboveRightUnits = 0;
  int avaiLeftBelowUnits = 0;
  int avaiAboveUnits = 0;
  int avaiLeftUnits = 0;

  const int curChromaMode = pu.intraDir[1];
  bool neighborFlags[4 * MAX_NUM_PART_IDXS_IN_CTU_WIDTH + 1];
  memset(neighborFlags, 0, totalUnits);

  bool aboveAvailable, leftAvailable;

  int availableUnit = isAboveAvailable(cu, CH_C, posLT, aboveUnits, unitWidth,
    (neighborFlags + leftUnits + leftBelowUnits + 1));
  aboveAvailable = availableUnit == tuWidthInUnits;

  availableUnit = isLeftAvailable(cu, CH_C, posLT, leftUnits, unitHeight,
    (neighborFlags + leftUnits + leftBelowUnits - 1));
  leftAvailable = availableUnit == tuHeightInUnits;
  if (leftAvailable) // if left is not available, then the below left is not available
  {
    avaiLeftUnits = tuHeightInUnits;
    avaiLeftBelowUnits = isBelowLeftAvailable(cu, CH_C, chromaArea.bottomLeftComp(chromaArea.compID), leftBelowUnits, unitHeight, (neighborFlags + leftBelowUnits - 1));
  }
  if (aboveAvailable) // if above is not available, then  the above right is not available.
  {
    avaiAboveUnits = tuWidthInUnits;
    avaiAboveRightUnits = isAboveRightAvailable(cu, CH_C, chromaArea.topRightComp(chromaArea.compID), aboveRightUnits, unitWidth, (neighborFlags + leftUnits + leftBelowUnits + aboveUnits + 1));
  }

  const int srcStride = 2 * MAX_TB_SIZEY + 1;
  Pel* srcColor0 = m_pMdlmTemp + srcStride + 1;

  Pel* curChroma0 = getPredictorPtr(compID);

  unsigned internalBitDepth = sps.bitDepths[CH_C];

  int minLuma[2] = {  MAX_INT, 0 };
  int maxLuma[2] = { -MAX_INT, 0 };

  Pel* src = srcColor0 - srcStride;
  int actualTopTemplateSampNum = 0;
  int actualLeftTemplateSampNum = 0;
  if (curChromaMode == MDLM_T_IDX)
  {
    leftAvailable = 0;
    avaiAboveRightUnits = avaiAboveRightUnits > (cHeight>>unitWidthLog2) ?  cHeight>>unitWidthLog2 : avaiAboveRightUnits;
    actualTopTemplateSampNum = unitWidth*(avaiAboveUnits + avaiAboveRightUnits);
  }
  else if (curChromaMode == MDLM_L_IDX)
  {
    aboveAvailable = 0;
    avaiLeftBelowUnits = avaiLeftBelowUnits > (cWidth>>unitHeightLog2) ? cWidth>>unitHeightLog2 : avaiLeftBelowUnits;
    actualLeftTemplateSampNum = unitHeight*(avaiLeftUnits + avaiLeftBelowUnits);
  }
  else if (curChromaMode == LM_CHROMA_IDX)
  {
    actualTopTemplateSampNum = cWidth;
    actualLeftTemplateSampNum = cHeight;
  }
  int startPos[2]; //0:Above, 1: Left
  int pickStep[2];

  int aboveIs4 = leftAvailable  ? 0 : 1;
  int leftIs4 =  aboveAvailable ? 0 : 1;

  startPos[0] = actualTopTemplateSampNum >> (2 + aboveIs4);
  pickStep[0] = std::max(1, actualTopTemplateSampNum >> (1 + aboveIs4));

  startPos[1] = actualLeftTemplateSampNum >> (2 + leftIs4);
  pickStep[1] = std::max(1, actualLeftTemplateSampNum >> (1 + leftIs4));

  Pel selectLumaPix[4] = { 0, 0, 0, 0 };
  Pel selectChromaPix[4] = { 0, 0, 0, 0 };

  int cntT, cntL;
  cntT = cntL = 0;
  int cnt = 0;
  if (aboveAvailable)
  {
    cntT = std::min(actualTopTemplateSampNum, (1 + aboveIs4) << 1);
    src = srcColor0 - srcStride;
    const Pel *cur = curChroma0 + 1;
    for (int pos = startPos[0]; cnt < cntT; pos += pickStep[0], cnt++)
    {
      selectLumaPix[cnt] = src[pos];
      selectChromaPix[cnt] = cur[pos];
    }
  }

  if (leftAvailable)
  {
    cntL = std::min(actualLeftTemplateSampNum, ( 1 + leftIs4 ) << 1 );
    src = srcColor0 - 1;
    const Pel *cur = curChroma0 + m_refBufferStride[compID] + 1;
    for (int pos = startPos[1], cnt = 0; cnt < cntL; pos += pickStep[1], cnt++)
    {
      selectLumaPix[cnt + cntT] = src[pos * srcStride];
      selectChromaPix[cnt + cntT] = cur[pos];
    }
  }
  cnt = cntL + cntT;

  if (cnt == 2)
  {
    selectLumaPix[3] = selectLumaPix[0]; selectChromaPix[3] = selectChromaPix[0];
    selectLumaPix[2] = selectLumaPix[1]; selectChromaPix[2] = selectChromaPix[1];
    selectLumaPix[0] = selectLumaPix[1]; selectChromaPix[0] = selectChromaPix[1];
    selectLumaPix[1] = selectLumaPix[3]; selectChromaPix[1] = selectChromaPix[3];
  }

  int minGrpIdx[2] = { 0, 2 };
  int maxGrpIdx[2] = { 1, 3 };
  int *tmpMinGrp = minGrpIdx;
  int *tmpMaxGrp = maxGrpIdx;
  if (selectLumaPix[tmpMinGrp[0]] > selectLumaPix[tmpMinGrp[1]]) std::swap(tmpMinGrp[0], tmpMinGrp[1]);
  if (selectLumaPix[tmpMaxGrp[0]] > selectLumaPix[tmpMaxGrp[1]]) std::swap(tmpMaxGrp[0], tmpMaxGrp[1]);
  if (selectLumaPix[tmpMinGrp[0]] > selectLumaPix[tmpMaxGrp[1]]) std::swap(tmpMinGrp, tmpMaxGrp);
  if (selectLumaPix[tmpMinGrp[1]] > selectLumaPix[tmpMaxGrp[0]]) std::swap(tmpMinGrp[1], tmpMaxGrp[0]);

  minLuma[0] = (selectLumaPix[tmpMinGrp[0]] + selectLumaPix[tmpMinGrp[1]] + 1 )>>1;
  minLuma[1] = (selectChromaPix[tmpMinGrp[0]] + selectChromaPix[tmpMinGrp[1]] + 1) >> 1;
  maxLuma[0] = (selectLumaPix[tmpMaxGrp[0]] + selectLumaPix[tmpMaxGrp[1]] + 1 )>>1;
  maxLuma[1] = (selectChromaPix[tmpMaxGrp[0]] + selectChromaPix[tmpMaxGrp[1]] + 1) >> 1;

  if (leftAvailable || aboveAvailable)
  {
    int diff = maxLuma[0] - minLuma[0];
    if (diff > 0)
    {
      int diffC = maxLuma[1] - minLuma[1];
      int x = floorLog2( diff );
      static const uint8_t DivSigTable[1 << 4] = {
        // 4bit significands - 8 ( MSB is omitted )
        0,  7,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  1,  1,  0
      };
      int normDiff = (diff << 4 >> x) & 15;
      int v = DivSigTable[normDiff] | 8;
      x += normDiff != 0;

      int y = floorLog2( abs( diffC ) ) + 1;
      int add = 1 << y >> 1;
      a = (diffC * v + add) >> y;
      iShift = 3 + x - y;
      if ( iShift < 1 )
      {
        iShift = 1;
        a = ( (a == 0)? 0: (a < 0)? -15 : 15 );   // a=Sign(a)*15
      }
      b = minLuma[1] - ((a * minLuma[0]) >> iShift);
    }
    else
    {
      a = 0;
      b = minLuma[1];
      iShift = 0;
    }
  }
  else
  {
    a = 0;
    b = 1 << (internalBitDepth - 1);
    iShift = 0;
  }
}

void IntraPrediction::initIntraMip( const PredictionUnit &pu )
{
  CHECK( pu.lwidth() > pu.cs->sps->getMaxTbSize() || pu.lheight() > pu.cs->sps->getMaxTbSize(), "Error: block size not supported for MIP" );

  // prepare input (boundary) data for prediction
  CHECK(m_ipaParam.refFilterFlag, "ERROR: unfiltered refs expected for MIP");
  Pel *ptrSrc = getPredictorPtr(COMP_Y);
  const int srcStride  = m_refBufferStride[COMP_Y];
  const int srcHStride = 2;

  m_matrixIntraPred.prepareInputForPred(CPelBuf(ptrSrc, srcStride, srcHStride), pu.Y(), pu.cu->slice->sps->bitDepths[CH_L]);
}

void IntraPrediction::predIntraMip( PelBuf &piPred, const PredictionUnit &pu )
{
  CHECK( pu.lwidth() > pu.cs->sps->getMaxTbSize() || pu.lheight() > pu.cs->sps->getMaxTbSize(), "Error: block size not supported for MIP" );
  CHECK( pu.lwidth() != (1 << floorLog2(pu.lwidth())) || pu.lheight() != (1 << floorLog2(pu.lheight())), "Error: expecting blocks of size 2^M x 2^N" );

  // generate mode-specific prediction
  const int bitDepth = pu.cu->slice->sps->bitDepths[CH_L];

  CHECK( pu.lwidth() != piPred.stride, " no support yet" );
 
  m_matrixIntraPred.predBlock(piPred.buf, pu.intraDir[CH_L], pu.mipTransposedFlag, bitDepth);
}

} // namespace vvenc

//! \}

