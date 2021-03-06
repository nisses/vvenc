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
/** \file     YuvX86.cpp
    \brief    SIMD averaging.
*/

#pragma once

#define DONT_UNDEF_SIZE_AWARE_PER_EL_OP 1

#include "CommonDefX86.h"
#include "Unit.h"
#include "InterpolationFilter.h"

#ifdef TARGET_SIMD_X86
#if ENABLE_SIMD_OPT_BUFFER

//! \ingroup CommonLib
//! \{

namespace vvenc {

#if USE_AVX2
template<bool isAligned> static inline __m256i load_aligned_avx2       ( const void* addr );
template<>                      inline __m256i load_aligned_avx2<true> ( const void* addr ) { return _mm256_load_si256 ( (const __m256i *) addr );}
template<>                      inline __m256i load_aligned_avx2<false>( const void* addr ) { return _mm256_loadu_si256( (const __m256i *) addr );}
#endif

template<bool isAligned> static inline __m128i load_aligned       ( const void* addr );
template<>                      inline __m128i load_aligned<true> ( const void* addr ) { return _mm_load_si128 ( (const __m128i *) addr );}
template<>                      inline __m128i load_aligned<false>( const void* addr ) { return _mm_loadu_si128( (const __m128i *) addr );}

template< X86_VEXT vext >
void weightCiip_SSE( Pel* res, const Pel* src, const int numSamples, int numIntra )
{
#if USE_AVX2
  int n = 16;
  if( numIntra == 1 )
  {
    __m256i vres;
    __m256i vpred = _mm256_load_si256((const __m256i*)&res[0]);
    __m256i vsrc  = _mm256_load_si256((const __m256i*)&src[0]);
    for( ; n < numSamples; n+=16)
    {
      vres = _mm256_avg_epu16( vpred, vsrc );
      vpred = _mm256_load_si256((const __m256i*)&res[n]);
      vsrc  = _mm256_load_si256((const __m256i*)&src[n]);
      _mm256_storeu_si256( ( __m256i * )&res[n-16], vres );
    }
    vres = _mm256_avg_epu16( vpred, vsrc );
    _mm256_storeu_si256( ( __m256i * )&res[n-16], vres );
  }
  else
  {
    const Pel* scale   = ( numIntra == 0 ) ? res : src;
    const Pel* unscale = ( numIntra == 0 ) ? src : res;

    __m256i vres;
    __m256i voffset = _mm256_set1_epi16(2);
    __m256i vscl = _mm256_load_si256((const __m256i*)&scale[0]);
    __m256i vuns = _mm256_load_si256((const __m256i*)&unscale[0]);
    for( ; n < numSamples; n+=16)
    {
      vres = _mm256_srai_epi16( _mm256_adds_epi16( _mm256_adds_epi16(_mm256_adds_epi16( vscl, vscl),_mm256_adds_epi16( vscl, vuns)), voffset), 2 );
      vscl = _mm256_load_si256((const __m256i*)&scale[n]);
      vuns = _mm256_load_si256((const __m256i*)&unscale[n]);
      _mm256_storeu_si256( ( __m256i * )&res[n-16], vres );
    }
    vres = _mm256_srai_epi16( _mm256_adds_epi16( _mm256_adds_epi16(_mm256_adds_epi16( vscl, vscl),_mm256_adds_epi16( vscl, vuns)), voffset), 2 );
    _mm256_storeu_si256( ( __m256i * )&res[n-16], vres );
  }
#else
  int n = 8;
  if( numIntra == 1 )
  {
    __m128i vres;
    __m128i vpred = _mm_load_si128((const __m128i*)&res[0]);
    __m128i vsrc  = _mm_load_si128((const __m128i*)&src[0]);
    for( ; n < numSamples; n+=8)
    {
      vres = _mm_avg_epu16( vpred, vsrc );
      vpred = _mm_load_si128((const __m128i*)&res[n]);
      vsrc  = _mm_load_si128((const __m128i*)&src[n]);
      _mm_storeu_si128( ( __m128i * )&res[n-8], vres );
    }
    vres = _mm_avg_epu16( vpred, vsrc );
    _mm_storeu_si128( ( __m128i * )&res[n-8], vres );
  }
  else
  {
    const Pel* scale   = ( numIntra == 0 ) ? res : src;
    const Pel* unscale = ( numIntra == 0 ) ? src : res;

    __m128i vres;
    __m128i voffset = _mm_set1_epi16(2);
    __m128i vscl = _mm_load_si128((const __m128i*)&scale[0]);
    __m128i vuns = _mm_load_si128((const __m128i*)&unscale[0]);
    for( ; n < numSamples; n+=8)
    {
      vres = _mm_srai_epi16( _mm_adds_epi16( _mm_adds_epi16(_mm_adds_epi16( vscl, vscl),_mm_adds_epi16( vscl, vuns)), voffset), 2 );
      vscl = _mm_load_si128((const __m128i*)&scale[n]);
      vuns = _mm_load_si128((const __m128i*)&unscale[n]);
      _mm_storeu_si128( ( __m128i * )&res[n-8], vres );
    }
    vres = _mm_srai_epi16( _mm_adds_epi16( _mm_adds_epi16(_mm_adds_epi16( vscl, vscl),_mm_adds_epi16( vscl, vuns)), voffset), 2 );
    _mm_storeu_si128( ( __m128i * )&res[n-8], vres );
  }
#endif
}

template< X86_VEXT vext, unsigned inputSize, unsigned outputSize >
void mipMatrixMul_SSE( Pel* res, const Pel* input, const uint8_t* weight, const int maxVal, const int inputOffset, bool transpose )
{
  int sum = 0;
  for( int i = 0; i < inputSize; i++ ) { sum += input[i]; }
  const int offset = (1 << (MIP_SHIFT_MATRIX - 1)) - MIP_OFFSET_MATRIX * sum + (inputOffset << MIP_SHIFT_MATRIX);
  CHECK( inputSize != 4 * (inputSize >> 2), "Error, input size not divisible by four" );

#if USE_AVX2
  static const __m256i perm = _mm256_setr_epi32(0,4,1,5,2,6,3,7);
  __m256i vibdimin  = _mm256_set1_epi16( 0 );
  __m256i vibdimax  = _mm256_set1_epi16( maxVal );
  if( inputSize == 4 && outputSize == 4)
  {
    __m256i voffset   = _mm256_set1_epi32( offset );
    __m256i vin = _mm256_set1_epi64x( *((const int64_t*)input) );
    __m256i vw = _mm256_load_si256((const __m256i*)(weight));

    __m256i w0 = _mm256_cvtepi8_epi16( _mm256_castsi256_si128( vw ) );   //w0 - w16
    __m256i w1 = _mm256_cvtepi8_epi16( _mm256_extracti128_si256( vw, 1 ) ); //w16 -w 32
     
    __m256i r0 = _mm256_madd_epi16( vin, w0 );
    __m256i r1 = _mm256_madd_epi16( vin, w1 );
    __m256i r2 = _mm256_hadd_epi32( r0 , r1);
           
            r2 = _mm256_add_epi32( r2, voffset );
    __m256i r3 = _mm256_srai_epi32( r2, MIP_SHIFT_MATRIX );

            vw = _mm256_load_si256((const __m256i*)(weight+32));
            w0 = _mm256_cvtepi8_epi16( _mm256_castsi256_si128( vw ) );   //w0 - w16
            w1 = _mm256_cvtepi8_epi16( _mm256_extracti128_si256( vw, 1 ) ); //w16 -w 32
     
            r0 = _mm256_madd_epi16( vin, w0 );
            r1 = _mm256_madd_epi16( vin, w1 );
            r2 = _mm256_hadd_epi32( r0 , r1);

            r2 = _mm256_add_epi32( r2, voffset );
            r2 = _mm256_srai_epi32( r2, MIP_SHIFT_MATRIX );
            r2 = _mm256_packs_epi32( r3, r2 );
            r2 = _mm256_permutevar8x32_epi32 ( r2, perm );

            r2 = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, r2 ) );

      if( transpose )
      {
        __m256i vshuf0 = _mm256_set_epi8( 0xf, 0xe, 0xb, 0xa, 0x7, 0x6, 0x3, 0x2, 0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0,
                                      0xf, 0xe, 0xb, 0xa, 0x7, 0x6, 0x3, 0x2, 0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0);
         r2 = _mm256_permutevar8x32_epi32( r2, _mm256_set_epi32(7,5,3,1,6,4,2,0) );
         r2 = _mm256_shuffle_epi8 ( r2, vshuf0);
      }

      _mm256_store_si256( ( __m256i * )&res[0], r2 );
  }
  else if( inputSize == 8 )
  {
    __m256i voffset   = _mm256_set1_epi32( offset );
    __m128i inv =_mm_load_si128( ( __m128i* )input );
    __m256i vin = _mm256_permute2f128_si256(_mm256_castsi128_si256(inv), _mm256_castsi128_si256(inv), 2); 
    __m256i r2;
    for( int i = 0; i < outputSize*outputSize; i+=16)
    {
      __m256i vw = _mm256_load_si256((const __m256i*)(weight));

      __m256i w0 = _mm256_cvtepi8_epi16( _mm256_castsi256_si128( vw ) );   //w0 - w16
      __m256i w1 = _mm256_cvtepi8_epi16( _mm256_extracti128_si256( vw, 1 ) ); //w16 -w 32
     
      __m256i r0 = _mm256_madd_epi16( vin, w0 );
      __m256i r1 = _mm256_madd_epi16( vin, w1 );
              r2 = _mm256_hadd_epi32( r0 , r1);
           
              vw = _mm256_load_si256((const __m256i*)(weight+32));
              w0 = _mm256_cvtepi8_epi16( _mm256_castsi256_si128( vw ) );   //w0 - w16
              w1 = _mm256_cvtepi8_epi16( _mm256_extracti128_si256( vw, 1 ) ); //w16 -w 32
     
              r0 = _mm256_madd_epi16( vin, w0 );
              r1 = _mm256_madd_epi16( vin, w1 );
      __m256i r4 = _mm256_hadd_epi32( r0 , r1);

              r2 = _mm256_hadd_epi32( r2 , r4);

              r2 = _mm256_add_epi32( r2, voffset );
      __m256i r3 = _mm256_srai_epi32( r2, MIP_SHIFT_MATRIX );

              vw = _mm256_load_si256((const __m256i*)(weight+64));

              w0 = _mm256_cvtepi8_epi16( _mm256_castsi256_si128( vw ) );   //w0 - w16
              w1 = _mm256_cvtepi8_epi16( _mm256_extracti128_si256( vw, 1 ) ); //w16 -w 32
     
              r0 = _mm256_madd_epi16( vin, w0 );
              r1 = _mm256_madd_epi16( vin, w1 );
              r2 = _mm256_hadd_epi32( r0 , r1);
           
              vw = _mm256_load_si256((const __m256i*)(weight+96));
              w0 = _mm256_cvtepi8_epi16( _mm256_castsi256_si128( vw ) );   //w0 - w16
              w1 = _mm256_cvtepi8_epi16( _mm256_extracti128_si256( vw, 1 ) ); //w16 -w 32
     
              r0 = _mm256_madd_epi16( vin, w0 );
              r1 = _mm256_madd_epi16( vin, w1 );

              r2 = _mm256_hadd_epi32( r2 , _mm256_hadd_epi32( r0 , r1));

              r2 = _mm256_add_epi32( r2, voffset );
              r2 = _mm256_srai_epi32( r2, MIP_SHIFT_MATRIX );

              r2 = _mm256_permutevar8x32_epi32 ( r2, perm );
              r3 = _mm256_permutevar8x32_epi32 ( r3, perm );

              r3 = _mm256_packs_epi32( r3, r2 );
              r2 = _mm256_permute4x64_epi64( r3, 0xd8 );

              r2 = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, r2 ) );

        _mm256_store_si256( ( __m256i * )&res[0], r2 );
        res+=16;
        weight+=128;
    }

    if( transpose )
    {
      if( outputSize == 4 )
      {
        __m256i vshuf0 = _mm256_set_epi8( 0xf, 0xe, 0xb, 0xa, 0x7, 0x6, 0x3, 0x2, 0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0,
                                      0xf, 0xe, 0xb, 0xa, 0x7, 0x6, 0x3, 0x2, 0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0);
         r2 = _mm256_permutevar8x32_epi32( r2, _mm256_set_epi32(7,5,3,1,6,4,2,0) );
         r2 = _mm256_shuffle_epi8 ( r2, vshuf0);
        _mm256_store_si256( ( __m256i * )(res-16), r2 );
      }
      else
      {
        res -= 64;

        __m256i va, vb, vc, vd, wa, wb, wc, wd;

        va = _mm256_load_si256( ( const __m256i* ) res ); 
        vb = _mm256_load_si256( ( const __m256i* ) (res+16) ); 
        vc = _mm256_load_si256( ( const __m256i* ) (res+32) ); 


        va =_mm256_permute4x64_epi64(va, 0xd8); 
        vb =_mm256_permute4x64_epi64(vb, 0xd8);
        vc =_mm256_permute4x64_epi64(vc, 0xd8);
        vd =_mm256_permute4x64_epi64(r2, 0xd8);

        wa = _mm256_unpacklo_epi16( va, vb );
        wb = _mm256_unpackhi_epi16( va, vb );
        wc = _mm256_unpacklo_epi16( vc, vd );
        wd = _mm256_unpackhi_epi16( vc, vd );

        va = _mm256_unpacklo_epi16( wa, wb );
        vb = _mm256_unpackhi_epi16( wa, wb );
        vc = _mm256_unpacklo_epi16( wc, wd );
        vd = _mm256_unpackhi_epi16( wc, wd );

        va =_mm256_permute4x64_epi64(va, 0xd8); 
        vb =_mm256_permute4x64_epi64(vb, 0xd8);
        vc =_mm256_permute4x64_epi64(vc, 0xd8);
        vd =_mm256_permute4x64_epi64(vd, 0xd8);

        wa = _mm256_unpacklo_epi64( va, vc );
        wb = _mm256_unpacklo_epi64( vb, vd );
        wc = _mm256_unpackhi_epi64( va, vc );
        wd = _mm256_unpackhi_epi64( vb, vd );

        _mm256_store_si256( ( __m256i* ) res, wa ); 
        _mm256_store_si256( ( __m256i* ) (res+16), wb );
        _mm256_store_si256( ( __m256i* ) (res+32), wc );
        _mm256_store_si256( ( __m256i* ) (res+48), wd );
      }
    }
  }
#else
  __m128i zero  = _mm_set1_epi16( 0 );
  __m128i vibdimax  = _mm_set1_epi16( maxVal );
  if( inputSize == 4 && outputSize == 4)
  {
    __m128i vin = _mm_set1_epi64x( *((const int64_t*)input) );
    __m128i voffset = _mm_set1_epi32( offset );
    __m128i r2 = vin;
    __m128i r;
    for( int i = 0; i < 2; i++)
    {
             r = r2; // save the result from the first interation
    __m128i vw = _mm_load_si128((const __m128i*)weight);

    __m128i w0 = _mm_unpacklo_epi8( vw, zero );
    __m128i w1 = _mm_unpackhi_epi8( vw, zero );
     
    __m128i r0 = _mm_madd_epi16( vin, w0 );
    __m128i r1 = _mm_madd_epi16( vin, w1 );
            r2 = _mm_hadd_epi32( r0 , r1);
           
            r2 = _mm_add_epi32( r2, voffset );
    __m128i r3 = _mm_srai_epi32( r2, MIP_SHIFT_MATRIX );

            vw = _mm_load_si128((const __m128i*)(weight+16));
            w0 = _mm_unpacklo_epi8( vw, zero );
            w1 = _mm_unpackhi_epi8( vw, zero );
     
            r0 = _mm_madd_epi16( vin, w0 );
            r1 = _mm_madd_epi16( vin, w1 );
            r2 = _mm_hadd_epi32( r0 , r1);

            r2 = _mm_add_epi32( r2, voffset );
            r2 = _mm_srai_epi32( r2, MIP_SHIFT_MATRIX );
            r2 = _mm_packs_epi32( r3, r2 );

            r2 = _mm_min_epi16( vibdimax, _mm_max_epi16( zero, r2 ) );

      _mm_store_si128( ( __m128i * )&res[0], r2 );
      res +=8;
      weight += 32;
    }

    if( transpose)
    {
      __m128i vc, vd, va, vb;
      vc = _mm_unpacklo_epi16( r, r2 );
      vd = _mm_unpackhi_epi16( r, r2 );
 
      va = _mm_unpacklo_epi16( vc, vd );
      vb = _mm_unpackhi_epi16( vc, vd );
 
      _mm_store_si128( ( __m128i* ) (res-16), va ); 
      _mm_store_si128( ( __m128i* ) (res-8), vb ); 
    }

  }
  else
  {
    __m128i vin = _mm_load_si128( (const __m128i*)input);
    __m128i voffset = _mm_set1_epi32( offset );

    for( int i = 0; i < outputSize*outputSize; i+=4)
    {
    __m128i vw = _mm_load_si128((const __m128i*)(weight));

    __m128i w0 = _mm_unpacklo_epi8( vw, zero );
    __m128i w1 = _mm_unpackhi_epi8( vw, zero );
     
    __m128i r0 = _mm_madd_epi16( vin, w0 );
    __m128i r1 = _mm_madd_epi16( vin, w1 );
    __m128i r2 = _mm_hadd_epi32( r0 , r1);
           
            vw = _mm_load_si128((const __m128i*)(weight+16));
            w0 = _mm_unpacklo_epi8( vw, zero );
            w1 = _mm_unpackhi_epi8( vw, zero );
     
            r0 = _mm_madd_epi16( vin, w0 );
            r1 = _mm_madd_epi16( vin, w1 );

            r2 = _mm_hadd_epi32( r2 , _mm_hadd_epi32( r0 , r1));

            r2 = _mm_add_epi32( r2, voffset );
            r2 = _mm_srai_epi32( r2, MIP_SHIFT_MATRIX );

            r2 = _mm_packs_epi32( r2, r2 );

            r2 = _mm_min_epi16( vibdimax, _mm_max_epi16( zero, r2 ) );

      _mm_storel_epi64( ( __m128i * )&res[0], r2 );
      res +=4;
      weight += 32;
    }

    if( transpose )
    {
      if( outputSize == 4)
      {
        res -= 16;
        __m128i vc, vd, va, vb;
        va = _mm_load_si128( ( const __m128i* ) (res) );
        vb = _mm_load_si128( ( const __m128i* ) (res+8) );

        vc = _mm_unpacklo_epi16( va, vb );
        vd = _mm_unpackhi_epi16( va, vb );
 
        va = _mm_unpacklo_epi16( vc, vd );
        vb = _mm_unpackhi_epi16( vc, vd );
 
        _mm_store_si128( ( __m128i* ) (res), va ); 
        _mm_store_si128( ( __m128i* ) (res+8), vb ); 
      }
      else
      {
        res -= 64;
        __m128i va, vb, vc, vd, ve, vf, vg, vh;

        va = _mm_load_si128( ( const __m128i* ) (res) );
        vb = _mm_load_si128( ( const __m128i* ) (res+8) );
        vc = _mm_load_si128( ( const __m128i* ) (res+16) );
        vd = _mm_load_si128( ( const __m128i* ) (res+24) );
        ve = _mm_load_si128( ( const __m128i* ) (res+32) );
        vf = _mm_load_si128( ( const __m128i* ) (res+40) );
        vg = _mm_load_si128( ( const __m128i* ) (res+48) );
        vh = _mm_load_si128( ( const __m128i* ) (res+56) );

        __m128i va01b01 = _mm_unpacklo_epi16( va, vb );
        __m128i va23b23 = _mm_unpackhi_epi16( va, vb );
        __m128i vc01d01 = _mm_unpacklo_epi16( vc, vd );
        __m128i vc23d23 = _mm_unpackhi_epi16( vc, vd );
        __m128i ve01f01 = _mm_unpacklo_epi16( ve, vf );
        __m128i ve23f23 = _mm_unpackhi_epi16( ve, vf );
        __m128i vg01h01 = _mm_unpacklo_epi16( vg, vh );
        __m128i vg23h23 = _mm_unpackhi_epi16( vg, vh );

        va = _mm_unpacklo_epi32( va01b01, vc01d01 );
        vb = _mm_unpackhi_epi32( va01b01, vc01d01 );
        vc = _mm_unpacklo_epi32( va23b23, vc23d23 );
        vd = _mm_unpackhi_epi32( va23b23, vc23d23 );
        ve = _mm_unpacklo_epi32( ve01f01, vg01h01 );
        vf = _mm_unpackhi_epi32( ve01f01, vg01h01 );
        vg = _mm_unpacklo_epi32( ve23f23, vg23h23 );
        vh = _mm_unpackhi_epi32( ve23f23, vg23h23 );

        va01b01 = _mm_unpacklo_epi64( va, ve );
        va23b23 = _mm_unpackhi_epi64( va, ve );
        vc01d01 = _mm_unpacklo_epi64( vb, vf );
        vc23d23 = _mm_unpackhi_epi64( vb, vf );
        ve01f01 = _mm_unpacklo_epi64( vc, vg );
        ve23f23 = _mm_unpackhi_epi64( vc, vg );
        vg01h01 = _mm_unpacklo_epi64( vd, vh );
        vg23h23 = _mm_unpackhi_epi64( vd, vh );

        _mm_store_si128( ( __m128i* ) (res),    va01b01 );
        _mm_store_si128( ( __m128i* ) (res+8) , va23b23 );
        _mm_store_si128( ( __m128i* ) (res+16), vc01d01 );
        _mm_store_si128( ( __m128i* ) (res+24), vc23d23 );
        _mm_store_si128( ( __m128i* ) (res+32), ve01f01 );
        _mm_store_si128( ( __m128i* ) (res+40), ve23f23 );
        _mm_store_si128( ( __m128i* ) (res+48), vg01h01 );
        _mm_store_si128( ( __m128i* ) (res+56), vg23h23 );
      }
    }
  }
#endif
}


template< X86_VEXT vext>
void addAvg_SSE( const Pel* src0, const Pel* src1, Pel* dst, int numSamples, unsigned shift, int offset, const ClpRng& clpRng )
{
#if USE_AVX2
  if( numSamples >= 16 )
  {
    __m256i voffset   = _mm256_set1_epi32( offset );
    __m256i vibdimin  = _mm256_set1_epi16( clpRng.min );
    __m256i vibdimax  = _mm256_set1_epi16( clpRng.max );

    for( int col = 0; col < numSamples; col += 16 )
    {
      __m256i vsrc0 = _mm256_load_si256( ( const __m256i* )&src0[col] );
      __m256i vsrc1 = _mm256_load_si256( ( const __m256i* )&src1[col] );

      __m256i vtmp, vsum, vdst;
      vsum = _mm256_cvtepi16_epi32    ( _mm256_castsi256_si128( vsrc0 ) );
      vdst = _mm256_cvtepi16_epi32    ( _mm256_castsi256_si128( vsrc1 ) );
      vsum = _mm256_add_epi32         ( vsum, vdst );
      vsum = _mm256_add_epi32         ( vsum, voffset );
      vtmp = _mm256_srai_epi32        ( vsum, shift );

      vsum = _mm256_cvtepi16_epi32    ( _mm256_extracti128_si256( vsrc0, 1 ) );
      vdst = _mm256_cvtepi16_epi32    ( _mm256_extracti128_si256( vsrc1, 1 ) );
      vsum = _mm256_add_epi32         ( vsum, vdst );
      vsum = _mm256_add_epi32         ( vsum, voffset );
      vsum = _mm256_srai_epi32        ( vsum, shift );
      vtmp = _mm256_packs_epi32       ( vtmp, vsum );
      vsum = _mm256_permute4x64_epi64 ( vtmp, ( 0 << 0 ) + ( 2 << 2 ) + ( 1 << 4 ) + ( 3 << 6 ) );

      vsum = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, vsum ) );
      _mm256_store_si256( ( __m256i * )&dst[col], vsum );
    }
  }
  else if( numSamples >= 8 )
  {
    __m256i voffset  = _mm256_set1_epi32( offset );
    __m128i vibdimin = _mm_set1_epi16   ( clpRng.min );
    __m128i vibdimax = _mm_set1_epi16   ( clpRng.max );

    for( int col = 0; col < numSamples; col += 8 )
    {
      __m256i vsrc0 = _mm256_cvtepi16_epi32( _mm_load_si128 ( (const __m128i *)&src0[col] ) );
      __m256i vsrc1 = _mm256_cvtepi16_epi32( _mm_load_si128 ( (const __m128i *)&src1[col] ) );

      __m256i
      vsum = _mm256_add_epi32        ( vsrc0, vsrc1 );
      vsum = _mm256_add_epi32        ( vsum, voffset );
      vsum = _mm256_srai_epi32       ( vsum, shift );

      vsum = _mm256_packs_epi32      ( vsum, vsum );
      vsum = _mm256_permute4x64_epi64( vsum, 0 + ( 2 << 2 ) + ( 2 << 4 ) + ( 3 << 6 ) );

      __m128i
      xsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, _mm256_castsi256_si128( vsum ) ) );
      _mm_store_si128( ( __m128i * )&dst[col], xsum );
    }
  }
#else
  if( numSamples >= 8 )
  {
    __m128i vzero    = _mm_setzero_si128();
    __m128i voffset  = _mm_set1_epi32( offset );
    __m128i vibdimin = _mm_set1_epi16( clpRng.min );
    __m128i vibdimax = _mm_set1_epi16( clpRng.max );

    for( int col = 0; col < numSamples; col += 8 )
    {
      __m128i vsrc0 = _mm_load_si128 ( (const __m128i *)&src0[col] );
      __m128i vsrc1 = _mm_load_si128 ( (const __m128i *)&src1[col] );

      __m128i vtmp, vsum, vdst;
      vsum = _mm_cvtepi16_epi32   ( vsrc0 );
      vdst = _mm_cvtepi16_epi32   ( vsrc1 );
      vsum = _mm_add_epi32        ( vsum, vdst );
      vsum = _mm_add_epi32        ( vsum, voffset );
      vtmp = _mm_srai_epi32       ( vsum, shift );

      vsrc0 = _mm_unpackhi_epi64  ( vsrc0, vzero );
      vsrc1 = _mm_unpackhi_epi64  ( vsrc1, vzero );
      vsum = _mm_cvtepi16_epi32   ( vsrc0 );
      vdst = _mm_cvtepi16_epi32   ( vsrc1 );
      vsum = _mm_add_epi32        ( vsum, vdst );
      vsum = _mm_add_epi32        ( vsum, voffset );
      vsum = _mm_srai_epi32       ( vsum, shift );
      vsum = _mm_packs_epi32      ( vtmp, vsum );

      vsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsum ) );
      _mm_store_si128( ( __m128i * )&dst[col], vsum );
    }
  }
#endif
  else if( numSamples == 4 )
  {
    __m128i vzero     = _mm_setzero_si128();
    __m128i voffset   = _mm_set1_epi32( offset );
    __m128i vibdimin  = _mm_set1_epi16( clpRng.min );
    __m128i vibdimax  = _mm_set1_epi16( clpRng.max );

    __m128i vsum = _mm_loadl_epi64  ( ( const __m128i * )&src0[0] );
    __m128i vdst = _mm_loadl_epi64  ( ( const __m128i * )&src1[0] );
    vsum = _mm_cvtepi16_epi32       ( vsum );
    vdst = _mm_cvtepi16_epi32       ( vdst );
    vsum = _mm_add_epi32            ( vsum, vdst );
    vsum = _mm_add_epi32            ( vsum, voffset );
    vsum = _mm_srai_epi32           ( vsum, shift );
    vsum = _mm_packs_epi32          ( vsum, vzero );

    vsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsum ) );
    _mm_storel_epi64( ( __m128i * )&dst[0], vsum );
  }
  else
  {
    THROW( "Unsupported size" );
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template< X86_VEXT vext>
void roundGeo_SSE( const Pel* src, Pel* dst, const int numSamples, unsigned shift, int offset, const ClpRng &clpRng)
{
#if USE_AVX2
  if( numSamples >= 16 )
  {
    __m256i voffset   = _mm256_set1_epi16( offset );
    __m256i vibdimin  = _mm256_set1_epi16( clpRng.min );
    __m256i vibdimax  = _mm256_set1_epi16( clpRng.max );

    for( int col = 0; col < numSamples; col += 16 )
    {
      __m256i val = _mm256_load_si256( ( const __m256i* )&src[col] );
      val = _mm256_add_epi16         ( val, voffset );
      val = _mm256_srai_epi16        ( val, shift );
      val = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, val ) );
      _mm256_store_si256( ( __m256i * )&dst[col], val );
    }
  }
  else
#endif
  {
    __m128i voffset   = _mm_set1_epi16( offset );
    __m128i vibdimin  = _mm_set1_epi16( clpRng.min );
    __m128i vibdimax  = _mm_set1_epi16( clpRng.max );

    if( numSamples >= 8 )
    {
      for( int col = 0; col < numSamples; col += 8 )
      {
        __m128i val = _mm_load_si128 ( (const __m128i *)&src[col] );
        val  = _mm_add_epi16        ( val, voffset );
        val  = _mm_srai_epi16       ( val, shift );
        val  = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, val ) );
        _mm_store_si128( ( __m128i * )&dst[col], val );
      }
    }
    else //if( numSamples == 4 )
    {
      __m128i val = _mm_loadl_epi64  ( ( const __m128i * )&src[0] );
      val = _mm_add_epi16            ( val, voffset );
      val = _mm_srai_epi16           ( val, shift );
      val = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, val ) );
      _mm_storel_epi64( ( __m128i * )&dst[0], val );
    }
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template< X86_VEXT vext >
void recoCore_SSE( const Pel* src0, const Pel* src1, Pel* dst, int numSamples, const ClpRng& clpRng )
{
#if USE_AVX2
  if( vext >= AVX2 && numSamples >= 16 )
  {
    __m256i vbdmin = _mm256_set1_epi16( clpRng.min );
    __m256i vbdmax = _mm256_set1_epi16( clpRng.max );

    for( int n = 0; n < numSamples; n += 16 )
    {
      __m256i vdest = _mm256_load_si256 ( ( const __m256i * )&src0[n] );
      __m256i vsrc1 = _mm256_load_si256( ( const __m256i * )&src1[n] );

      vdest = _mm256_add_epi16( vdest, vsrc1 );
      vdest = _mm256_min_epi16( vbdmax, _mm256_max_epi16( vbdmin, vdest ) );

      _mm256_store_si256( ( __m256i * )&dst[n], vdest );
    }
  }
  else
#endif
  if( numSamples >= 8 )
  {
    __m128i vbdmin = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax = _mm_set1_epi16( clpRng.max );

    for( int n = 0; n < numSamples; n += 8 )
    {
      __m128i vdest = _mm_load_si128 ( ( const __m128i * )&src0[n] );
      __m128i vsrc1 = _mm_load_si128( ( const __m128i * )&src1[n] );

      vdest = _mm_add_epi16( vdest, vsrc1 );
      vdest = _mm_min_epi16( vbdmax, _mm_max_epi16( vbdmin, vdest ) );

      _mm_store_si128( ( __m128i * )&dst[n], vdest );
    }
  }
  else
  {
    __m128i vbdmin = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax = _mm_set1_epi16( clpRng.max );

    __m128i vsrc = _mm_loadl_epi64( ( const __m128i * )&src0[0] );
    __m128i vdst = _mm_loadl_epi64( ( const __m128i * )&src1[0] );

    vdst = _mm_add_epi16( vdst, vsrc );
    vdst = _mm_min_epi16( vbdmax, _mm_max_epi16( vbdmin, vdst ) );

    _mm_storel_epi64( ( __m128i * )&dst[0], vdst );
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template<X86_VEXT vext>
void copyClip_SSE( const Pel* src, Pel* dst, int numSamples, const ClpRng& clpRng )
{
  if( vext >= AVX2 && numSamples >= 16 )
  {
#if USE_AVX2
    __m256i vbdmin   = _mm256_set1_epi16( clpRng.min );
    __m256i vbdmax   = _mm256_set1_epi16( clpRng.max );

    for( int col = 0; col < numSamples; col += 16 )
    {
      __m256i val = _mm256_loadu_si256  ( ( const __m256i * ) &src[col] );
      val = _mm256_min_epi16            ( vbdmax, _mm256_max_epi16( vbdmin, val ) );
      _mm256_storeu_si256               ( ( __m256i * )&dst[col], val );
    }
#endif
  }
  else if(numSamples >= 8 )
  {
    __m128i vbdmin = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax = _mm_set1_epi16( clpRng.max );

    for( int col = 0; col < numSamples; col += 8 )
    {
      __m128i val = _mm_loadu_si128 ( ( const __m128i * ) &src[col] );
      val = _mm_min_epi16           ( vbdmax, _mm_max_epi16( vbdmin, val ) );
      _mm_storeu_si128              ( ( __m128i * )&dst[col], val );
    }
  }
  else
  {
    __m128i vbdmin  = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax  = _mm_set1_epi16( clpRng.max );

    __m128i val;
    val = _mm_loadl_epi64   ( ( const __m128i * )&src[0] );
    val = _mm_min_epi16     ( vbdmax, _mm_max_epi16( vbdmin, val ) );
    _mm_storel_epi64        ( ( __m128i * )&dst[0], val );
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}


template< X86_VEXT vext, int W, bool srcAligned >
void addAvg_SSE_algn( const int16_t* src0, int src0Stride, const int16_t* src1, int src1Stride, int16_t *dst, ptrdiff_t dstStride, int width, int height, unsigned shift, int offset, const ClpRng& clpRng )
{
#if USE_AVX2
  if( W == 16 )
  {
    __m256i voffset   = _mm256_set1_epi32( offset );
    __m256i vibdimin  = _mm256_set1_epi16( clpRng.min );
    __m256i vibdimax  = _mm256_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 16 )
      {
        __m256i vsrc0 = load_aligned_avx2<srcAligned>( ( const void* )&src0[col] );
        __m256i vsrc1 = load_aligned_avx2<srcAligned>( ( const void* )&src1[col] );

        __m256i vtmp, vsum, vdst;
        vsum = _mm256_cvtepi16_epi32    ( _mm256_castsi256_si128( vsrc0 ) );
        vdst = _mm256_cvtepi16_epi32    ( _mm256_castsi256_si128( vsrc1 ) );
        vsum = _mm256_add_epi32         ( vsum, vdst );
        vsum = _mm256_add_epi32         ( vsum, voffset );
        vtmp = _mm256_srai_epi32        ( vsum, shift );

        vsum = _mm256_cvtepi16_epi32    ( _mm256_extracti128_si256( vsrc0, 1 ) );
        vdst = _mm256_cvtepi16_epi32    ( _mm256_extracti128_si256( vsrc1, 1 ) );
        vsum = _mm256_add_epi32         ( vsum, vdst );
        vsum = _mm256_add_epi32         ( vsum, voffset );
        vsum = _mm256_srai_epi32        ( vsum, shift );
        vtmp = _mm256_packs_epi32       ( vtmp, vsum );
        vsum = _mm256_permute4x64_epi64 ( vtmp, ( 0 << 0 ) + ( 2 << 2 ) + ( 1 << 4 ) + ( 3 << 6 ) );

        vsum = _mm256_min_epi16( vibdimax, _mm256_max_epi16( vibdimin, vsum ) );
        _mm256_storeu_si256( ( __m256i * )&dst[col], vsum );
      }

      src0 += src0Stride;
      src1 += src1Stride;
      dst  +=  dstStride;
    }
  }
  else
#endif
#if USE_AVX2
  if( W >= 8 )
  {
    __m256i voffset  = _mm256_set1_epi32( offset );
    __m128i vibdimin = _mm_set1_epi16   ( clpRng.min );
    __m128i vibdimax = _mm_set1_epi16   ( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 8 )
      {
        __m256i vsrc0 = _mm256_cvtepi16_epi32( load_aligned<srcAligned>( ( const void* )&src0[col] ) );
        __m256i vsrc1 = _mm256_cvtepi16_epi32( load_aligned<srcAligned>( ( const void* )&src1[col] ) );

        __m256i
        vsum = _mm256_add_epi32        ( vsrc0, vsrc1 );
        vsum = _mm256_add_epi32        ( vsum, voffset );
        vsum = _mm256_srai_epi32       ( vsum, shift );

        vsum = _mm256_packs_epi32      ( vsum, vsum );
        vsum = _mm256_permute4x64_epi64( vsum, 0 + ( 2 << 2 ) + ( 2 << 4 ) + ( 3 << 6 ) );

        __m128i
        xsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, _mm256_castsi256_si128( vsum ) ) );
        _mm_storeu_si128( ( __m128i * )&dst[col], xsum );
      }

      src0 += src0Stride;
      src1 += src1Stride;
      dst  +=  dstStride;
    }
  }
#else
  if( W >= 8 )
  {
    __m128i vzero    = _mm_setzero_si128();
    __m128i voffset  = _mm_set1_epi32( offset );
    __m128i vibdimin = _mm_set1_epi16( clpRng.min );
    __m128i vibdimax = _mm_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 8 )
      {
        __m128i vsrc0 = load_aligned<srcAligned>( ( const void* )&src0[col] );
        __m128i vsrc1 = load_aligned<srcAligned>( ( const void* )&src1[col] );

        __m128i vtmp, vsum, vdst;
        vsum = _mm_cvtepi16_epi32   ( vsrc0 );
        vdst = _mm_cvtepi16_epi32   ( vsrc1 );
        vsum = _mm_add_epi32        ( vsum, vdst );
        vsum = _mm_add_epi32        ( vsum, voffset );
        vtmp = _mm_srai_epi32       ( vsum, shift );

        vsrc0 = _mm_unpackhi_epi64  ( vsrc0, vzero );
        vsrc1 = _mm_unpackhi_epi64  ( vsrc1, vzero );
        vsum = _mm_cvtepi16_epi32   ( vsrc0 );
        vdst = _mm_cvtepi16_epi32   ( vsrc1 );
        vsum = _mm_add_epi32        ( vsum, vdst );
        vsum = _mm_add_epi32        ( vsum, voffset );
        vsum = _mm_srai_epi32       ( vsum, shift );
        vsum = _mm_packs_epi32      ( vtmp, vsum );

        vsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsum ) );
        _mm_storeu_si128( ( __m128i * )&dst[col], vsum );
      }

      src0 += src0Stride;
      src1 += src1Stride;
      dst  +=  dstStride;
    }
  }
#endif
  else if( W == 4 )
  {
    __m128i vzero     = _mm_setzero_si128();
    __m128i voffset   = _mm_set1_epi32( offset );
    __m128i vibdimin  = _mm_set1_epi16( clpRng.min );
    __m128i vibdimax  = _mm_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 4 )
      {
        __m128i vsum = _mm_loadl_epi64  ( ( const __m128i * )&src0[col] );
        __m128i vdst = _mm_loadl_epi64  ( ( const __m128i * )&src1[col] );
        vsum = _mm_cvtepi16_epi32       ( vsum );
        vdst = _mm_cvtepi16_epi32       ( vdst );
        vsum = _mm_add_epi32            ( vsum, vdst );
        vsum = _mm_add_epi32            ( vsum, voffset );
        vsum = _mm_srai_epi32           ( vsum, shift );
        vsum = _mm_packs_epi32          ( vsum, vzero );

        vsum = _mm_min_epi16( vibdimax, _mm_max_epi16( vibdimin, vsum ) );
        _mm_storel_epi64( ( __m128i * )&dst[col], vsum );
      }

      src0 += src0Stride;
      src1 += src1Stride;
      dst  +=  dstStride;
    }
  }
  else
  {
    THROW( "Unsupported size" );
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template< X86_VEXT vext, int W >
void addAvg_SSE( const int16_t* src0, int src0Stride, const int16_t* src1, int src1Stride, int16_t *dst, int dstStride, int width, int height, unsigned shift, int offset, const ClpRng& clpRng/*, bool srcAligned*/ )
{
/*
  if( srcAligned )
  {
    addAvg_SSE_algn<vext, W, true>( src0, src0Stride, src1, src1Stride, dst, dstStride, width, height, shift, offset, clpRng );
  }
  else
*/
  {
    addAvg_SSE_algn<vext, W, false>( src0, src0Stride, src1, src1Stride, dst, dstStride, width, height, shift, offset, clpRng );
  }
}

template< X86_VEXT vext >
void roundIntVector_SIMD(int* v, int size, unsigned int nShift, const int dmvLimit)
{
  CHECKD(size % 16 != 0, "Size must be multiple of 16!");
#ifdef USE_AVX512
  if (vext >= AVX512 && size >= 16)
  {
    __m512i dMvMin = _mm256_set1_epi32(-dmvLimit);
    __m512i dMvMax = _mm256_set1_epi32(dmvLimit);
    __m512i nOffset = _mm512_set1_epi32((1 << (nShift - 1)));
    __m512i vones = _mm512_set1_epi32(1);
    __m512i vzero = _mm512_setzero_si512();
    for (int i = 0; i < size; i += 16, v += 16)
    {
      __m512i src = _mm512_loadu_si512(v);
      __mmask16 mask = _mm512_cmpge_epi32_mask(src, vzero);
      src = __mm512_add_epi32(src, nOffset);
      __mm512i dst = _mm512_srai_epi32(_mm512_mask_sub_epi32(src, mask, src, vones), nShift);
      dst = _mm512_min_epi32(dMvMax, _mm512_max_epi32(dMvMin, dst));
      _mm512_storeu_si512(v, dst);
    }
  }
  else
#endif
#ifdef USE_AVX2
  if (vext >= AVX2 && size >= 8)
  {
    __m256i dMvMin = _mm256_set1_epi32(-dmvLimit);
    __m256i dMvMax = _mm256_set1_epi32(dmvLimit);
    __m256i nOffset = _mm256_set1_epi32(1 << (nShift - 1));
    __m256i vzero = _mm256_setzero_si256();
    for (int i = 0; i < size; i += 8, v += 8)
    {
      __m256i src = _mm256_lddqu_si256((__m256i*)v);
      __m256i of  = _mm256_cmpgt_epi32(src, vzero);
      __m256i dst = _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(src, nOffset), of), nShift);
      dst = _mm256_min_epi32(dMvMax, _mm256_max_epi32(dMvMin, dst));
      _mm256_storeu_si256((__m256i*)v, dst);
    }
  }
  else
#endif
  {
    __m128i dMvMin = _mm_set1_epi32(-dmvLimit);
    __m128i dMvMax = _mm_set1_epi32(dmvLimit);
    __m128i nOffset = _mm_set1_epi32((1 << (nShift - 1)));
    __m128i vzero = _mm_setzero_si128();
    for (int i = 0; i < size; i += 4, v += 4)
    {
      __m128i src = _mm_loadu_si128((__m128i*)v);
      __m128i of  = _mm_cmpgt_epi32(src, vzero);
      __m128i dst = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(src, nOffset), of), nShift);
      dst = _mm_min_epi32(dMvMax, _mm_max_epi32(dMvMin, dst));
      _mm_storeu_si128((__m128i*)v, dst);
    }
  }
}


template< X86_VEXT vext, int W >
void reco_SSE( const int16_t* src0, int src0Stride, const int16_t* src1, int src1Stride, int16_t *dst, int dstStride, int width, int height, const ClpRng& clpRng )
{
  if( W == 8 )
  {
#if USE_AVX2
    if( vext >= AVX2 && ( width & 15 ) == 0 )
    {
      __m256i vbdmin = _mm256_set1_epi16( clpRng.min );
      __m256i vbdmax = _mm256_set1_epi16( clpRng.max );

      for( int row = 0; row < height; row++ )
      {
        for( int col = 0; col < width; col += 16 )
        {
          __m256i vdest = _mm256_loadu_si256 ( ( const __m256i * )&src0[col] );
          __m256i vsrc1 = _mm256_loadu_si256( ( const __m256i * )&src1[col] );

          vdest = _mm256_add_epi16( vdest, vsrc1 );
          vdest = _mm256_min_epi16( vbdmax, _mm256_max_epi16( vbdmin, vdest ) );

          _mm256_storeu_si256( ( __m256i * )&dst[col], vdest );
        }

        src0 += src0Stride;
        src1 += src1Stride;
        dst  += dstStride;
      }
    }
    else
#endif
    {
      __m128i vbdmin = _mm_set1_epi16( clpRng.min );
      __m128i vbdmax = _mm_set1_epi16( clpRng.max );

      for( int row = 0; row < height; row++ )
      {
        for( int col = 0; col < width; col += 8 )
        {
          __m128i vdest = _mm_loadu_si128 ( ( const __m128i * )&src0[col] );
          __m128i vsrc1 = _mm_loadu_si128( ( const __m128i * )&src1[col] );

          vdest = _mm_add_epi16( vdest, vsrc1 );
          vdest = _mm_min_epi16( vbdmax, _mm_max_epi16( vbdmin, vdest ) );

          _mm_storeu_si128( ( __m128i * )&dst[col], vdest );
        }

        src0 += src0Stride;
        src1 += src1Stride;
        dst  += dstStride;
      }
    }
  }
  else if( W == 4 )
  {
    __m128i vbdmin = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax = _mm_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 4 )
      {
        __m128i vsrc = _mm_loadl_epi64( ( const __m128i * )&src0[col] );
        __m128i vdst = _mm_loadl_epi64( ( const __m128i * )&src1[col] );

        vdst = _mm_add_epi16( vdst, vsrc );
        vdst = _mm_min_epi16( vbdmax, _mm_max_epi16( vbdmin, vdst ) );

        _mm_storel_epi64( ( __m128i * )&dst[col], vdst );
      }

      src0 += src0Stride;
      src1 += src1Stride;
      dst  +=  dstStride;
    }
  }
  else
  {
    THROW( "Unsupported size" );
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template<X86_VEXT vext>
void copyBufferSimd( const char* src, int srcStride, char* dst, int dstStride, int width, int height)
{
  _mm_prefetch( src            , _MM_HINT_T0 );
  _mm_prefetch( src + srcStride, _MM_HINT_T0 );

  if( width == srcStride && width == dstStride )
  {
    memcpy( dst, src, width * height );
  }
  else
  {
    while( height-- )
    {
      const char* nextSrcLine = src + srcStride;
            char* nextDstLine = dst + dstStride;

      _mm_prefetch( nextSrcLine, _MM_HINT_T0 );

      memcpy( dst, src, width );

      src = nextSrcLine;
      dst = nextDstLine;
    }
  }
}


template<X86_VEXT vext>
void paddingSimd(Pel* dst, int stride, int width, int height, int padSize)
{
  __m128i x;
#ifdef USE_AVX2
  __m256i x16;
#endif
  int temp, j;
  for (int i = 1; i <= padSize; i++)
  {
    j = 0;
    temp = width;
#ifdef USE_AVX2
    while ((temp >> 4) > 0)
    {

      x16 = _mm256_loadu_si256((const __m256i*)(&(dst[j])));
      _mm256_storeu_si256((__m256i*)(dst + j - i*stride), x16);
      x16 = _mm256_loadu_si256((const __m256i*)(dst + j + (height - 1)*stride));
      _mm256_storeu_si256((__m256i*)(dst + j + (height - 1 + i)*stride), x16);


      j = j + 16;
      temp = temp - 16;
    }
#endif
    while ((temp >> 3) > 0)
    {

      x = _mm_loadu_si128((const __m128i*)(&(dst[j])));
      _mm_storeu_si128((__m128i*)(dst + j - i*stride), x);
      x = _mm_loadu_si128((const __m128i*)(dst + j + (height - 1)*stride));
      _mm_storeu_si128((__m128i*)(dst + j + (height - 1 + i)*stride), x);

      j = j + 8;
      temp = temp - 8;
    }
    while ((temp >> 2) > 0)
    {
      x = _mm_loadl_epi64((const __m128i*)(&dst[j]));
      _mm_storel_epi64((__m128i*)(dst + j - i*stride), x);
      x = _mm_loadl_epi64((const __m128i*)(dst + j + (height - 1)*stride));
      _mm_storel_epi64((__m128i*)(dst + j + (height - 1 + i)*stride), x);

      j = j + 4;
      temp = temp - 4;
    }
    while (temp > 0)
    {
      dst[j - i*stride] = dst[j];
      dst[j + (height - 1 + i)*stride] = dst[j + (height - 1)*stride];
      j++;
      temp--;
    }
  }


  //Left and Right Padding
  Pel* ptr1 = dst - padSize*stride;
  Pel* ptr2 = dst - padSize*stride + width - 1;
  int offset = 0;
  for (int i = 0; i < height + 2 * padSize; i++)
  {
    offset = stride * i;
    for (int j = 1; j <= padSize; j++)
    {
      *(ptr1 - j + offset) = *(ptr1 + offset);
      *(ptr2 + j + offset) = *(ptr2 + offset);
    }

  }
}

#if ENABLE_SIMD_OPT_BCW
template< X86_VEXT vext, int W >
void removeHighFreq_SSE(int16_t* src0, int src0Stride, const int16_t* src1, int src1Stride, int width, int height)
{
 if (W == 8)
 {
   // TODO: AVX2 impl
   {
     for (int row = 0; row < height; row++)
     {
       for (int col = 0; col < width; col += 8)
       {
         __m128i vsrc0 = _mm_load_si128((const __m128i *)&src0[col]);
         __m128i vsrc1 = _mm_load_si128((const __m128i *)&src1[col]);

         vsrc0 = _mm_sub_epi16(_mm_slli_epi16(vsrc0, 1), vsrc1);
         _mm_store_si128((__m128i *)&src0[col], vsrc0);
       }

       src0 += src0Stride;
       src1 += src1Stride;
     }
   }
 }
 else if (W == 4)
 {
   for (int row = 0; row < height; row += 2)
   {
     __m128i vsrc0 = _mm_loadl_epi64((const __m128i *)src0);
     __m128i vsrc1 = _mm_loadl_epi64((const __m128i *)src1);
     __m128i vsrc0_2 = _mm_loadl_epi64((const __m128i *)(src0 + src0Stride));
     __m128i vsrc1_2 = _mm_loadl_epi64((const __m128i *)(src1 + src1Stride));

     vsrc0 = _mm_unpacklo_epi64(vsrc0, vsrc0_2);
     vsrc1 = _mm_unpacklo_epi64(vsrc1, vsrc1_2);

     vsrc0 = _mm_sub_epi16(_mm_slli_epi16(vsrc0, 1), vsrc1);
     _mm_storel_epi64((__m128i *)src0, vsrc0);
     _mm_storel_epi64((__m128i *)(src0 + src0Stride), _mm_unpackhi_epi64(vsrc0, vsrc0));

     src0 += (src0Stride << 1);
     src1 += (src1Stride << 1);
   }
 }
 else
 {
   THROW("Unsupported size");
 }
}
#endif

template<bool doShift, bool shiftR, typename T> static inline void do_shift( T &vreg, int num );
#if USE_AVX2
template<> inline void do_shift<true,  true , __m256i>( __m256i &vreg, int num ) { vreg = _mm256_srai_epi32( vreg, num ); }
template<> inline void do_shift<true,  false, __m256i>( __m256i &vreg, int num ) { vreg = _mm256_slli_epi32( vreg, num ); }
template<> inline void do_shift<false, true , __m256i>( __m256i &vreg, int num ) { }
template<> inline void do_shift<false, false, __m256i>( __m256i &vreg, int num ) { }
#endif
template<> inline void do_shift<true,  true , __m128i>( __m128i &vreg, int num ) { vreg = _mm_srai_epi32( vreg, num ); }
template<> inline void do_shift<true,  false, __m128i>( __m128i &vreg, int num ) { vreg = _mm_slli_epi32( vreg, num ); }
template<> inline void do_shift<false, true , __m128i>( __m128i &vreg, int num ) { }
template<> inline void do_shift<false, false, __m128i>( __m128i &vreg, int num ) { }

template<bool mult, typename T> static inline void do_mult( T& vreg, T& vmult );
template<> inline void do_mult<false, __m128i>( __m128i&, __m128i& ) { }
#if USE_AVX2
template<> inline void do_mult<false, __m256i>( __m256i&, __m256i& ) { }
#endif
template<> inline void do_mult<true,   __m128i>( __m128i& vreg, __m128i& vmult ) { vreg = _mm_mullo_epi32   ( vreg, vmult ); }
#if USE_AVX2
template<> inline void do_mult<true,   __m256i>( __m256i& vreg, __m256i& vmult ) { vreg = _mm256_mullo_epi32( vreg, vmult ); }
#endif

template<bool add, typename T> static inline void do_add( T& vreg, T& vadd );
template<> inline void do_add<false, __m128i>( __m128i&, __m128i& ) { }
#if USE_AVX2
template<> inline void do_add<false, __m256i>( __m256i&, __m256i& ) { }
#endif
template<> inline void do_add<true,  __m128i>( __m128i& vreg, __m128i& vadd ) { vreg = _mm_add_epi32( vreg, vadd ); }
#if USE_AVX2
template<> inline void do_add<true,  __m256i>( __m256i& vreg, __m256i& vadd ) { vreg = _mm256_add_epi32( vreg, vadd ); }
#endif

template<bool clip, typename T> static inline void do_clip( T& vreg, T& vbdmin, T& vbdmax );
template<> inline void do_clip<false, __m128i>( __m128i&, __m128i&, __m128i& ) { }
#if USE_AVX2
template<> inline void do_clip<false, __m256i>( __m256i&, __m256i&, __m256i& ) { }
#endif
template<> inline void do_clip<true,  __m128i>( __m128i& vreg, __m128i& vbdmin, __m128i& vbdmax ) { vreg = _mm_min_epi16   ( vbdmax, _mm_max_epi16   ( vbdmin, vreg ) ); }
#if USE_AVX2
template<> inline void do_clip<true,  __m256i>( __m256i& vreg, __m256i& vbdmin, __m256i& vbdmax ) { vreg = _mm256_min_epi16( vbdmax, _mm256_max_epi16( vbdmin, vreg ) ); }
#endif


template<X86_VEXT vext, int W, bool doAdd, bool mult, bool doShift, bool shiftR, bool clip>
void linTf_SSE( const Pel* src, int srcStride, Pel* dst, int dstStride, int width, int height, int scale, int shift, int offset, const ClpRng& clpRng )
{
  if( vext >= AVX2 && ( width & 7 ) == 0 && W == 8 )
  {
#if USE_AVX2
    __m256i vzero    = _mm256_setzero_si256();
    __m256i vbdmin   = _mm256_set1_epi16( clpRng.min );
    __m256i vbdmax   = _mm256_set1_epi16( clpRng.max );
    __m256i voffset  = _mm256_set1_epi32( offset );
    __m256i vscale   = _mm256_set1_epi32( scale );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 8 )
      {
        __m256i val;
        val = _mm256_cvtepi16_epi32       (  _mm_loadu_si128( ( const __m128i * )&src[col] ) );
        do_mult<mult, __m256i>            ( val, vscale );
        do_shift<doShift, shiftR, __m256i>( val, shift );
        do_add<doAdd, __m256i>            ( val, voffset );
        val = _mm256_packs_epi32          ( val, vzero );
        do_clip<clip, __m256i>            ( val, vbdmin, vbdmax );
        val = _mm256_permute4x64_epi64    ( val, ( 0 << 0 ) + ( 2 << 2 ) + ( 1 << 4 ) + ( 1 << 6 ) );

        _mm_storeu_si128                  ( ( __m128i * )&dst[col], _mm256_castsi256_si128( val ) );
      }

      src += srcStride;
      dst += dstStride;
    }
#endif
  }
  else
  {
    __m128i vzero   = _mm_setzero_si128();
    __m128i vbdmin  = _mm_set1_epi16   ( clpRng.min );
    __m128i vbdmax  = _mm_set1_epi16   ( clpRng.max );
    __m128i voffset = _mm_set1_epi32   ( offset );
    __m128i vscale  = _mm_set1_epi32   ( scale );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 4 )
      {
        __m128i val;
        val = _mm_loadl_epi64             ( ( const __m128i * )&src[col] );
        val = _mm_cvtepi16_epi32          ( val );
        do_mult<mult, __m128i>            ( val, vscale );
        do_shift<doShift, shiftR, __m128i>( val, shift );
        do_add<doAdd, __m128i>            ( val, voffset );
        val = _mm_packs_epi32             ( val, vzero );
        do_clip<clip, __m128i>            ( val, vbdmin, vbdmax );

        _mm_storel_epi64                  ( ( __m128i * )&dst[col], val );
      }

      src += srcStride;
      dst += dstStride;
    }
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template<X86_VEXT vext, int W>
void linTf_SSE_entry( const Pel* src, int srcStride, Pel* dst, int dstStride, int width, int height, int scale, unsigned shift, int offset, const ClpRng& clpRng, bool clip )
{
  int fn = ( offset == 0 ? 16 : 0 ) + ( scale == 1 ? 8 : 0 ) + ( shift == 0 ? 4 : 0 ) /*+ ( shift < 0 ? 2 : 0 )*/ + ( !clip ? 1 : 0 );

  switch( fn )
  {
  case  0: linTf_SSE<vext, W, true,  true,  true,  true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case  1: linTf_SSE<vext, W, true,  true,  true,  true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case  2: linTf_SSE<vext, W, true,  true,  true,  false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case  3: linTf_SSE<vext, W, true,  true,  true,  false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case  4: linTf_SSE<vext, W, true,  true,  false, true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case  5: linTf_SSE<vext, W, true,  true,  false, true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case  6: linTf_SSE<vext, W, true,  true,  false, false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case  7: linTf_SSE<vext, W, true,  true,  false, false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case  8: linTf_SSE<vext, W, true,  false, true,  true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case  9: linTf_SSE<vext, W, true,  false, true,  true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case 10: linTf_SSE<vext, W, true,  false, true,  false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case 11: linTf_SSE<vext, W, true,  false, true,  false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case 12: linTf_SSE<vext, W, true,  false, false, true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case 13: linTf_SSE<vext, W, true,  false, false, true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case 14: linTf_SSE<vext, W, true,  false, false, false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case 15: linTf_SSE<vext, W, true,  false, false, false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case 16: linTf_SSE<vext, W, false, true,  true,  true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case 17: linTf_SSE<vext, W, false, true,  true,  true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case 18: linTf_SSE<vext, W, false, true,  true,  false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case 19: linTf_SSE<vext, W, false, true,  true,  false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case 20: linTf_SSE<vext, W, false, true,  false, true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case 21: linTf_SSE<vext, W, false, true,  false, true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case 22: linTf_SSE<vext, W, false, true,  false, false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case 23: linTf_SSE<vext, W, false, true,  false, false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case 24: linTf_SSE<vext, W, false, false, true,  true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case 25: linTf_SSE<vext, W, false, false, true,  true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case 26: linTf_SSE<vext, W, false, false, true,  false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case 27: linTf_SSE<vext, W, false, false, true,  false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  case 28: linTf_SSE<vext, W, false, false, false, true,  true >( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
  case 29: linTf_SSE<vext, W, false, false, false, true,  false>( src, srcStride, dst, dstStride, width, height, scale,  shift, offset, clpRng ); break;
//  case 30: linTf_SSE<vext, W, false, false, false, false, true >( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
//  case 31: linTf_SSE<vext, W, false, false, false, false, false>( src, srcStride, dst, dstStride, width, height, scale, -shift, offset, clpRng ); break;
  default:
    THROW( "Unknown parametrization of the linear transformation" );
    break;
  }
}

template<X86_VEXT vext, int W>
void copyClip_SSE( const int16_t* src, int srcStride, int16_t* dst, int dstStride, int width, int height, const ClpRng& clpRng )
{
  if( vext >= AVX2 && ( width & 15 ) == 0 && W == 8 )
  {
#if USE_AVX2
    __m256i vbdmin   = _mm256_set1_epi16( clpRng.min );
    __m256i vbdmax   = _mm256_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 16 )
      {
        __m256i val = _mm256_loadu_si256  ( ( const __m256i * ) &src[col] );
        val = _mm256_min_epi16            ( vbdmax, _mm256_max_epi16( vbdmin, val ) );
        _mm256_storeu_si256               ( ( __m256i * )&dst[col], val );
      }

      src += srcStride;
      dst += dstStride;
    }
#endif
  }
  else if( W == 8 )
  {
    __m128i vbdmin = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax = _mm_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 8 )
      {
        __m128i val = _mm_loadu_si128 ( ( const __m128i * ) &src[col] );
        val = _mm_min_epi16           ( vbdmax, _mm_max_epi16( vbdmin, val ) );
        _mm_storeu_si128              ( ( __m128i * )&dst[col], val );
      }

      src += srcStride;
      dst += dstStride;
    }
  }
  else
  {
    __m128i vbdmin  = _mm_set1_epi16( clpRng.min );
    __m128i vbdmax  = _mm_set1_epi16( clpRng.max );

    for( int row = 0; row < height; row++ )
    {
      for( int col = 0; col < width; col += 4 )
      {
        __m128i val;
        val = _mm_loadl_epi64   ( ( const __m128i * )&src[col] );
        val = _mm_min_epi16     ( vbdmax, _mm_max_epi16( vbdmin, val ) );
        _mm_storel_epi64        ( ( __m128i * )&dst[col], val );
      }

      src += srcStride;
      dst += dstStride;
    }
  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template<X86_VEXT vext, int W>
void transposeNxN_SSE( const Pel* src, int srcStride, Pel* dst, int dstStride )
{
  if( W == 4 )
  {
    __m128i va, vb, vc, vd;

    va = _mm_loadl_epi64( ( const __m128i* ) src ); src += srcStride;
    vb = _mm_loadl_epi64( ( const __m128i* ) src ); src += srcStride;
    vc = _mm_loadl_epi64( ( const __m128i* ) src ); src += srcStride;
    vd = _mm_loadl_epi64( ( const __m128i* ) src );

    __m128i va01b01 = _mm_unpacklo_epi16( va,      vb );
    __m128i va23b23 = _mm_unpackhi_epi64( va01b01, vb );
    __m128i vc01d01 = _mm_unpacklo_epi16( vc,      vd );
    __m128i vc23d23 = _mm_unpackhi_epi64( vc01d01, vd );

    va = _mm_unpacklo_epi32( va01b01, vc01d01 );
    vb = _mm_unpackhi_epi64( va,      va );
    vc = _mm_unpacklo_epi32( va23b23, vc23d23 );
    vd = _mm_unpackhi_epi64( vc,      vc );

    _mm_storel_epi64( ( __m128i* ) dst, va ); dst += dstStride;
    _mm_storel_epi64( ( __m128i* ) dst, vb ); dst += dstStride;
    _mm_storel_epi64( ( __m128i* ) dst, vc ); dst += dstStride;
    _mm_storel_epi64( ( __m128i* ) dst, vd );
  }
  else if( W == 8 )
  {
  
  __m128i va, vb, vc, vd, ve, vf, vg, vh;

    va = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    vb = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    vc = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    vd = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    ve = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    vf = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    vg = _mm_loadu_si128( ( const __m128i* ) src ); src += srcStride;
    vh = _mm_loadu_si128( ( const __m128i* ) src );

    __m128i va01b01 = _mm_unpacklo_epi16( va, vb );
    __m128i va23b23 = _mm_unpackhi_epi16( va, vb );
    __m128i vc01d01 = _mm_unpacklo_epi16( vc, vd );
    __m128i vc23d23 = _mm_unpackhi_epi16( vc, vd );
    __m128i ve01f01 = _mm_unpacklo_epi16( ve, vf );
    __m128i ve23f23 = _mm_unpackhi_epi16( ve, vf );
    __m128i vg01h01 = _mm_unpacklo_epi16( vg, vh );
    __m128i vg23h23 = _mm_unpackhi_epi16( vg, vh );

    va = _mm_unpacklo_epi32( va01b01, vc01d01 );
    vb = _mm_unpackhi_epi32( va01b01, vc01d01 );
    vc = _mm_unpacklo_epi32( va23b23, vc23d23 );
    vd = _mm_unpackhi_epi32( va23b23, vc23d23 );
    ve = _mm_unpacklo_epi32( ve01f01, vg01h01 );
    vf = _mm_unpackhi_epi32( ve01f01, vg01h01 );
    vg = _mm_unpacklo_epi32( ve23f23, vg23h23 );
    vh = _mm_unpackhi_epi32( ve23f23, vg23h23 );

    va01b01 = _mm_unpacklo_epi64( va, ve );
    va23b23 = _mm_unpackhi_epi64( va, ve );
    vc01d01 = _mm_unpacklo_epi64( vb, vf );
    vc23d23 = _mm_unpackhi_epi64( vb, vf );
    ve01f01 = _mm_unpacklo_epi64( vc, vg );
    ve23f23 = _mm_unpackhi_epi64( vc, vg );
    vg01h01 = _mm_unpacklo_epi64( vd, vh );
    vg23h23 = _mm_unpackhi_epi64( vd, vh );

    _mm_storeu_si128( ( __m128i* ) dst, va01b01 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, va23b23 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, vc01d01 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, vc23d23 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, ve01f01 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, ve23f23 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, vg01h01 ); dst += dstStride;
    _mm_storeu_si128( ( __m128i* ) dst, vg23h23 );

  }
#if USE_AVX2

  _mm256_zeroupper();
#endif
}

template< X86_VEXT vext >
void applyPROF_SSE(Pel* dstPel, int dstStride, const Pel* srcPel, int srcStride, int width, int height, const Pel* gradX, const Pel* gradY, int gradStride, const int* dMvX, const int* dMvY, int dMvStride, const bool& bi, int shiftNum, Pel offset, const ClpRng& clpRng)
{
  CHECKD( width != 4 || height != 4, "block width error!");

  const int dILimit = 1 << std::max<int>(clpRng.bd + 1, 13);

#if USE_AVX2
  __m256i mm_dmvx, mm_dmvy, mm_gradx, mm_grady, mm_dI, mm_dI0, mm_src;
  __m256i mm_offset = _mm256_set1_epi16( offset );
  __m256i vibdimin  = _mm256_set1_epi16( clpRng.min );
  __m256i vibdimax  = _mm256_set1_epi16( clpRng.max );
  __m256i mm_dimin  = _mm256_set1_epi32( -dILimit );
  __m256i mm_dimax  = _mm256_set1_epi32( dILimit - 1 );

  const int *vX0 = dMvX, *vY0 = dMvY;
  const Pel *gX0 = gradX, *gY0 = gradY;

  // first two rows
  mm_dmvx = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadu_si128( ( const __m128i * ) vX0 ) ), _mm_loadu_si128( ( const __m128i * )( vX0 + dMvStride ) ), 1 );
  mm_dmvy = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadu_si128( ( const __m128i * ) vY0 ) ), _mm_loadu_si128( ( const __m128i * )( vY0 + dMvStride ) ), 1 );

  mm_dmvx = _mm256_packs_epi32( mm_dmvx, _mm256_setzero_si256() );
  mm_dmvy = _mm256_packs_epi32( mm_dmvy, _mm256_setzero_si256() );

  mm_gradx = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadl_epi64( ( __m128i* )gX0 ) ), _mm_loadl_epi64( ( __m128i* )( gX0 + gradStride ) ), 1 );
  mm_grady = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadl_epi64( ( __m128i* )gY0 ) ), _mm_loadl_epi64( ( __m128i* )( gY0 + gradStride ) ), 1 );
  
  mm_dI0   = _mm256_madd_epi16( _mm256_unpacklo_epi16( mm_dmvx, mm_dmvy ), _mm256_unpacklo_epi16( mm_gradx, mm_grady ) );
  mm_dI0   = _mm256_min_epi32( mm_dimax, _mm256_max_epi32( mm_dimin, mm_dI0 ) );

  // next two rows
  vX0 += ( dMvStride << 1 ); vY0 += ( dMvStride << 1 ); gX0 += ( gradStride << 1 ); gY0 += ( gradStride << 1 );
  
  mm_dmvx = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadu_si128( ( const __m128i * ) vX0 ) ), _mm_loadu_si128( ( const __m128i * )( vX0 + dMvStride ) ), 1 );
  mm_dmvy = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadu_si128( ( const __m128i * ) vY0 ) ), _mm_loadu_si128( ( const __m128i * )( vY0 + dMvStride ) ), 1 );

  mm_dmvx = _mm256_packs_epi32( mm_dmvx, _mm256_setzero_si256() );
  mm_dmvy = _mm256_packs_epi32( mm_dmvy, _mm256_setzero_si256() );

  mm_gradx = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadl_epi64( ( __m128i* )gX0 ) ), _mm_loadl_epi64( ( __m128i* )( gX0 + gradStride ) ), 1 );
  mm_grady = _mm256_inserti128_si256( _mm256_castsi128_si256( _mm_loadl_epi64( ( __m128i* )gY0 ) ), _mm_loadl_epi64( ( __m128i* )( gY0 + gradStride ) ), 1 );
  
  mm_dI    = _mm256_madd_epi16( _mm256_unpacklo_epi16( mm_dmvx, mm_dmvy ), _mm256_unpacklo_epi16( mm_gradx, mm_grady ) );
  mm_dI    = _mm256_min_epi32( mm_dimax, _mm256_max_epi32( mm_dimin, mm_dI ) );

  // combine four rows
  mm_dI = _mm256_packs_epi32( mm_dI0, mm_dI );
  const Pel* src0 = srcPel + srcStride;
  mm_src = _mm256_inserti128_si256(
    _mm256_castsi128_si256(_mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)srcPel), _mm_loadl_epi64((const __m128i *)(srcPel + (srcStride << 1))))),
    _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)src0), _mm_loadl_epi64((const __m128i *)(src0 + (srcStride << 1)))),
    1
  );
  mm_dI = _mm256_add_epi16(mm_dI, mm_src);
  if (!bi)
  {
    mm_dI = _mm256_srai_epi16(_mm256_adds_epi16(mm_dI, mm_offset), shiftNum);
    mm_dI = _mm256_min_epi16(vibdimax, _mm256_max_epi16(vibdimin, mm_dI));
  }

  // store final results
  __m128i dITmp = _mm256_extractf128_si256(mm_dI, 1);
  Pel* dst0 = dstPel;
  _mm_storel_epi64((__m128i *)dst0, _mm256_castsi256_si128(mm_dI));
  dst0 += dstStride; _mm_storel_epi64((__m128i *)dst0, dITmp);
  dst0 += dstStride; _mm_storel_epi64((__m128i *)dst0, _mm_unpackhi_epi64(_mm256_castsi256_si128(mm_dI), _mm256_castsi256_si128(mm_dI)));
  dst0 += dstStride; _mm_storel_epi64((__m128i *)dst0, _mm_unpackhi_epi64(dITmp, dITmp));
#else
  __m128i mm_dmvx, mm_dmvy, mm_gradx, mm_grady, mm_dI, mm_dI0;
  __m128i mm_offset = _mm_set1_epi16( offset );
  __m128i vibdimin  = _mm_set1_epi16( clpRng.min );
  __m128i vibdimax  = _mm_set1_epi16( clpRng.max );
  __m128i mm_dimin  = _mm_set1_epi32( -dILimit );
  __m128i mm_dimax  = _mm_set1_epi32( dILimit - 1 );

  for( int h = 0; h < height; h += 2 )
  {
    const int* vX = dMvX;
    const int* vY = dMvY;
    const Pel* gX = gradX;
    const Pel* gY = gradY;
    const Pel* src = srcPel;
    Pel*       dst = dstPel;

    // first row
    mm_dmvx  = _mm_packs_epi32( _mm_loadu_si128( ( const __m128i * ) vX ), _mm_setzero_si128() );
    mm_dmvy  = _mm_packs_epi32( _mm_loadu_si128( ( const __m128i * ) vY ), _mm_setzero_si128() );
    mm_gradx = _mm_loadl_epi64( ( __m128i* ) gX );
    mm_grady = _mm_loadl_epi64( ( __m128i* ) gY );
    mm_dI0   = _mm_madd_epi16 ( _mm_unpacklo_epi16( mm_dmvx, mm_dmvy ), _mm_unpacklo_epi16( mm_gradx, mm_grady ) );
    mm_dI0   = _mm_min_epi32  ( mm_dimax, _mm_max_epi32( mm_dimin, mm_dI0 ) );

    // second row
    mm_dmvx  = _mm_packs_epi32( _mm_loadu_si128( ( const __m128i * ) ( vX + dMvStride ) ), _mm_setzero_si128() );
    mm_dmvy  = _mm_packs_epi32( _mm_loadu_si128( ( const __m128i * ) ( vY + dMvStride ) ), _mm_setzero_si128() );
    mm_gradx = _mm_loadl_epi64( ( __m128i* ) ( gX + gradStride ) );
    mm_grady = _mm_loadl_epi64( ( __m128i* ) ( gY + gradStride ) );
    mm_dI    = _mm_madd_epi16 ( _mm_unpacklo_epi16( mm_dmvx, mm_dmvy ), _mm_unpacklo_epi16( mm_gradx, mm_grady ) );
    mm_dI    = _mm_min_epi32  ( mm_dimax, _mm_max_epi32( mm_dimin, mm_dI ) );

    // combine both rows
    mm_dI = _mm_packs_epi32( mm_dI0, mm_dI );
    mm_dI = _mm_add_epi16  ( _mm_unpacklo_epi64( _mm_loadl_epi64( ( const __m128i * )src ), _mm_loadl_epi64( ( const __m128i * )( src + srcStride ) ) ), mm_dI );
    if (!bi)
    {
      mm_dI = _mm_srai_epi16(_mm_adds_epi16(mm_dI, mm_offset), shiftNum);
      mm_dI = _mm_min_epi16(vibdimax, _mm_max_epi16(vibdimin, mm_dI));
    }

    _mm_storel_epi64( ( __m128i * )  dst,                                   mm_dI );
    _mm_storel_epi64( ( __m128i * )( dst + dstStride ), _mm_unpackhi_epi64( mm_dI, mm_dI ) );

    dMvX   += (dMvStride  << 1);
    dMvY   += (dMvStride  << 1);
    gradX  += (gradStride << 1);
    gradY  += (gradStride << 1);
    srcPel += (srcStride  << 1);
    dstPel += (dstStride  << 1);
  }
#endif
}

template< X86_VEXT vext, bool PAD = true>
void gradFilter_SSE(const Pel* src, int srcStride, int width, int height, int gradStride, Pel* gradX, Pel* gradY, const int bitDepth) //exist in InterPredX86
{
  const Pel* srcTmp = src + srcStride + 1;
  Pel* gradXTmp = gradX + gradStride + 1;
  Pel* gradYTmp = gradY + gradStride + 1;

  int widthInside = width - 2 * BDOF_EXTEND_SIZE;
  int heightInside = height - 2 * BDOF_EXTEND_SIZE;
  int shift1 = std::max<int>(6, bitDepth - 6);
  __m128i mmShift1 = _mm_cvtsi32_si128(shift1);
  assert((widthInside & 3) == 0);

  if ((widthInside & 7) == 0)
  {
    for (int y = 0; y < heightInside; y++)
    {
      int x = 0;
      for (; x < widthInside; x += 8)
      {
        __m128i mmPixTop = _mm_sra_epi16(_mm_loadu_si128((__m128i*) (srcTmp + x - srcStride)), mmShift1);
        __m128i mmPixBottom = _mm_sra_epi16(_mm_loadu_si128((__m128i*) (srcTmp + x + srcStride)), mmShift1);
        __m128i mmPixLeft = _mm_sra_epi16(_mm_loadu_si128((__m128i*) (srcTmp + x - 1)), mmShift1);
        __m128i mmPixRight = _mm_sra_epi16(_mm_loadu_si128((__m128i*) (srcTmp + x + 1)), mmShift1);

        __m128i mmGradVer = _mm_sub_epi16(mmPixBottom, mmPixTop);
        __m128i mmGradHor = _mm_sub_epi16(mmPixRight, mmPixLeft);

        _mm_storeu_si128((__m128i *) (gradYTmp + x), mmGradVer);
        _mm_storeu_si128((__m128i *) (gradXTmp + x), mmGradHor);
      }
      gradXTmp += gradStride;
      gradYTmp += gradStride;
      srcTmp += srcStride;
    }
  }
  else
  {
    __m128i mmPixTop = _mm_sra_epi16(_mm_unpacklo_epi64(_mm_loadl_epi64((__m128i*) (srcTmp - srcStride)), _mm_loadl_epi64((__m128i*) (srcTmp))), mmShift1);
    for (int y = 0; y < heightInside; y += 2)
    {
      __m128i mmPixBottom = _mm_sra_epi16(_mm_unpacklo_epi64(_mm_loadl_epi64((__m128i*) (srcTmp + srcStride)), _mm_loadl_epi64((__m128i*) (srcTmp + (srcStride << 1)))), mmShift1);
      __m128i mmPixLeft = _mm_sra_epi16(_mm_unpacklo_epi64(_mm_loadl_epi64((__m128i*) (srcTmp - 1)), _mm_loadl_epi64((__m128i*) (srcTmp - 1 + srcStride))), mmShift1);
      __m128i mmPixRight = _mm_sra_epi16(_mm_unpacklo_epi64(_mm_loadl_epi64((__m128i*) (srcTmp + 1)), _mm_loadl_epi64((__m128i*) (srcTmp + 1 + srcStride))), mmShift1);

      __m128i mmGradVer = _mm_sub_epi16(mmPixBottom, mmPixTop);
      __m128i mmGradHor = _mm_sub_epi16(mmPixRight, mmPixLeft);

      _mm_storel_epi64((__m128i *) gradYTmp, mmGradVer);
      _mm_storel_epi64((__m128i *) (gradYTmp + gradStride), _mm_unpackhi_epi64(mmGradVer, mmGradHor));
      _mm_storel_epi64((__m128i *) gradXTmp, mmGradHor);
      _mm_storel_epi64((__m128i *) (gradXTmp + gradStride), _mm_unpackhi_epi64(mmGradHor, mmGradVer));

      mmPixTop = mmPixBottom;
      gradXTmp += gradStride << 1;
      gradYTmp += gradStride << 1;
      srcTmp += srcStride << 1;
    }
  }

  if (PAD)
  {
    gradXTmp = gradX + gradStride + 1;
    gradYTmp = gradY + gradStride + 1;
    for (int y = 0; y < heightInside; y++)
    {
      gradXTmp[-1] = gradXTmp[0];
      gradXTmp[widthInside] = gradXTmp[widthInside - 1];
      gradXTmp += gradStride;

      gradYTmp[-1] = gradYTmp[0];
      gradYTmp[widthInside] = gradYTmp[widthInside - 1];
      gradYTmp += gradStride;
    }

    gradXTmp = gradX + gradStride;
    gradYTmp = gradY + gradStride;
    ::memcpy(gradXTmp - gradStride, gradXTmp, sizeof(Pel)*(width));
    ::memcpy(gradXTmp + heightInside*gradStride, gradXTmp + (heightInside - 1)*gradStride, sizeof(Pel)*(width));
    ::memcpy(gradYTmp - gradStride, gradYTmp, sizeof(Pel)*(width));
    ::memcpy(gradYTmp + heightInside*gradStride, gradYTmp + (heightInside - 1)*gradStride, sizeof(Pel)*(width));
  }
}

template<X86_VEXT vext>
void applyLut_SIMD( const Pel* src, const ptrdiff_t srcStride, Pel* dst, const ptrdiff_t dstStride, int width, int height, const Pel* lut )
{
#if USE_AVX2
  // this implementation is only faster on modern CPUs
  if( ( width & 15 ) == 0 && ( height & 1 ) == 0 )
  {
    const __m256i vLutShuf = _mm256_setr_epi8( 0, 1, 4, 5, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 4, 5, 8, 9, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1 );

    for( int y = 0; y < height; y += 2 )
    {
      for( int x = 0; x < width; x += 16 )
      {
        __m256i vin16    = _mm256_loadu_si256       ( ( const __m256i * ) &src[x] );
                                                    
        __m256i vin32_1  = _mm256_unpacklo_epi16    ( vin16, _mm256_setzero_si256() );
        __m256i vin32_2  = _mm256_unpackhi_epi16    ( vin16, _mm256_setzero_si256() );

        __m256i vout32_1 = _mm256_i32gather_epi32   ( ( const int * ) lut, vin32_1, 2 );
        __m256i vout32_2 = _mm256_i32gather_epi32   ( ( const int * ) lut, vin32_2, 2 );

        vout32_1         = _mm256_shuffle_epi8      ( vout32_1, vLutShuf );
        vout32_2         = _mm256_shuffle_epi8      ( vout32_2, vLutShuf );

        __m256i vout16   = _mm256_unpacklo_epi64    ( vout32_1, vout32_2 );

        _mm256_storeu_si256( ( __m256i * ) &dst[x], vout16 );
        
        vin16            = _mm256_loadu_si256       ( ( const __m256i * ) &src[x + srcStride] );
                                                    
        vin32_1          = _mm256_unpacklo_epi16    ( vin16, _mm256_setzero_si256() );
        vin32_2          = _mm256_unpackhi_epi16    ( vin16, _mm256_setzero_si256() );
                         
        vout32_1         = _mm256_i32gather_epi32   ( ( const int * ) lut, vin32_1, 2 );
        vout32_2         = _mm256_i32gather_epi32   ( ( const int * ) lut, vin32_2, 2 );

        vout32_1         = _mm256_shuffle_epi8      ( vout32_1, vLutShuf );
        vout32_2         = _mm256_shuffle_epi8      ( vout32_2, vLutShuf );

        vout16           = _mm256_unpacklo_epi64    ( vout32_1, vout32_2 );

        _mm256_storeu_si256( ( __m256i * ) &dst[x + dstStride], vout16 );
      }

      src += ( srcStride << 1 );
      dst += ( dstStride << 1 );
    }

    _mm256_zeroupper();
  }
  else
#endif
  {
#define RSP_SGNL_OP( ADDR ) dst[ADDR] = lut[src[ADDR]]
#define RSP_SGNL_INC        src += srcStride; dst += dstStride;

    SIZE_AWARE_PER_EL_OP( RSP_SGNL_OP, RSP_SGNL_INC )

#undef RSP_SGNL_OP
#undef RSP_SGNL_INC
  }
}

template<X86_VEXT vext>
void fillPtrMap_SIMD( void** ptr, ptrdiff_t ptrStride, int width, int height, void* val )
{
  static_assert( sizeof( val ) == 8, "Only supported for 64bit systems!" );
  if( ( width & 3 ) == 0 )
  {
#if USE_AVX2
    __m256i vval = _mm256_set1_epi64x( ( int64_t ) val );

    while( height-- )
    {
      for( int x = 0; x < width; x += 4 ) _mm256_storeu_si256( ( __m256i* ) &ptr[x], vval );

      ptr += ptrStride;
    }
#else
    __m128i vval = _mm_set1_epi64x( ( int64_t ) val );

    while( height-- )
    {
      for( int x = 0; x < width; x += 4 )
      {
        _mm_storeu_si128( ( __m128i* ) &ptr[x + 0], vval );
        _mm_storeu_si128( ( __m128i* ) &ptr[x + 2], vval );
      }

      ptr += ptrStride;
    }
#endif
  }
  else if( ( width & 1 ) == 0 )
  {
    __m128i vval = _mm_set1_epi64x( ( int64_t ) val );

    while( height-- )
    {
      for( int x = 0; x < width; x += 2 ) _mm_storeu_si128( ( __m128i* ) &ptr[x], vval );

      ptr += ptrStride;
    }
  }
  else
  {
    while( height-- )
    {
      *ptr = val; ptr += ptrStride;
    }
  }
}

template<X86_VEXT vext>
void PelBufferOps::_initPelBufOpsX86()
{
  addAvg   = addAvg_SSE<vext>;
  reco     = recoCore_SSE<vext>;
  copyClip = copyClip_SSE<vext>;
  roundGeo = roundGeo_SSE<vext>;

  addAvg4  = addAvg_SSE<vext, 4>;
  addAvg8  = addAvg_SSE<vext, 8>;
  addAvg16 = addAvg_SSE<vext, 16>;

  copyClip4 = copyClip_SSE<vext, 4>;
  copyClip8 = copyClip_SSE<vext, 8>;

  reco4 = reco_SSE<vext, 4>;
  reco8 = reco_SSE<vext, 8>;

  linTf4 = linTf_SSE_entry<vext, 4>;
  linTf8 = linTf_SSE_entry<vext, 8>;

  copyBuffer = copyBufferSimd<vext>;
  padding    = paddingSimd<vext>;

#if ENABLE_SIMD_OPT_BCW
  removeHighFreq8 = removeHighFreq_SSE<vext, 8>;
  removeHighFreq4 = removeHighFreq_SSE<vext, 4>;
#endif

  transpose4x4   = transposeNxN_SSE<vext, 4>;
  transpose8x8   = transposeNxN_SSE<vext, 8>;
  profGradFilter = gradFilter_SSE<vext, false>; 
  applyPROF      = applyPROF_SSE<vext>;
  roundIntVector = roundIntVector_SIMD<vext>;

  mipMatrixMul_4_4 = mipMatrixMul_SSE<vext, 4, 4>;
  mipMatrixMul_8_4 = mipMatrixMul_SSE<vext, 8, 4>;
  mipMatrixMul_8_8 = mipMatrixMul_SSE<vext, 8, 8>;

  weightCiip = weightCiip_SSE<vext>;

  applyLut = applyLut_SIMD<vext>;

  fillPtrMap = fillPtrMap_SIMD<vext>;
}

template void PelBufferOps::_initPelBufOpsX86<SIMDX86>();

} // namespace vvenc

//! \}

#endif // ENABLE_SIMD_OPT_BUFFER
#endif // TARGET_SIMD_X86

