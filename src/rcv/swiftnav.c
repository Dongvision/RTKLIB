/*------------------------------------------------------------------------------
 * switnav.c : Swift Navigation Binary Protocol decoder
 *
 *          Copyright (C) 2017
 *
 * reference :
 *     [1]
 *
 * version : $Revision: 1.0 $ $Date: 2017/01/30 09:00:00 $
 *
 * history : 2017/01/30  1.0  begin writing
  *-----------------------------------------------------------------------------*/
#include "rtklib.h"

#include <stdint.h>
#include <math.h>

static const char rcsid[]="$Id: Swiftnav SBP,v 1.0 2017/02/01 FT $";

#define SBP_SYNC1   0x55        /* SBP message header sync */

#define ID_MEASEPOCH       0x004A /* SBP message id: observation */
#define ID_MSGEPHGPS       0x0081 /* SBP message id: GPS L1 C/A navigation message */
#define ID_MSGIONGPS       0x0090 /* SBP message id: GPS ionospheric parameters */


/* get fields (little-endian) ------------------------------------------------*/
#define U1(p) (*((uint8_t *)(p)))
#define I1(p) (*((char *)(p)))
static uint16_t  U2(uint8_t *p) {uint16_t u; memcpy(&u,p,2); return u;}
static uint32_t  U4(uint8_t *p) {uint32_t   u; memcpy(&u,p,4); return u;}
static float     R4(uint8_t *p) {float          r; memcpy(&r,p,4); return r;}
static double    R8(uint8_t *p) {double         r; memcpy(&r,p,8); return r;}
static int32_t   I4(uint8_t *p) {signed int     u; memcpy(&u,p,4); return u;}
static short     I2(uint8_t *p) {short          i; memcpy(&i,p,2); return i;}

/* checksum lookup table -----------------------------------------------------*/
static const uint32_t CRC_16CCIT_LookUp[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

/* it's easy to derive a function for the values below, but I'd rather map the table explicitly from the RTCM standard document */
static const uint32_t puRtcmPhaseRangeLockTimeTable[16] = {
    0, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288
};

static const uint8_t decoding_table[ 256 ] =
{
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x3F,
   0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
   0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x00,0x00,0x00,0x00,0x00,
   0x00,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
   0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static uint8_t puPayloadTmp[256];


static int Base64_Decode( uint8_t *_pcData, uint32_t _uDataLen, uint8_t *_puDecodedData, uint32_t *_puDecodedDataLen ) {
   uint32_t i, j;
   uint32_t output_length;
   uint32_t a, b, c, d, t;

   if ( NULL == _puDecodedData ) {
      return -1;
   }

   if ( 0 != (_uDataLen % 4) ) {
      return -1;
   }

   output_length = _uDataLen / 4 * 3;

   if ( '=' == _pcData[_uDataLen - 1] ) {
      output_length--;
   }
   if ( '=' == _pcData[_uDataLen - 2] ) {
      output_length--;
   }

   if ( output_length > (*_puDecodedDataLen) ) {
      /* Not enough space in output buffer */
      return -1;
   }

   (*_puDecodedDataLen) = output_length;

   for ( i = 0, j = 0; i < _uDataLen; ) {
      a = ('=' == _pcData[i]) ? 0 : decoding_table[ _pcData[i] ]; i++;
      b = ('=' == _pcData[i]) ? 0 : decoding_table[ _pcData[i] ]; i++;
      c = ('=' == _pcData[i]) ? 0 : decoding_table[ _pcData[i] ]; i++;
      d = ('=' == _pcData[i]) ? 0 : decoding_table[ _pcData[i] ]; i++;

      t = ( a << 3 * 6 )
        + ( b << 2 * 6 )
        + ( c << 1 * 6 )
        + ( d << 0 * 6 );

      if ( j < output_length ) {
         _puDecodedData[j++] = (t >> 2 * 8) & 0xFF;
      }
      if ( j < output_length ) {
         _puDecodedData[j++] = (t >> 1 * 8) & 0xFF;
      }
      if ( j < output_length ) {
         _puDecodedData[j++] = (t >> 0 * 8) & 0xFF;
      }
   } /* for() */
   return 0;
} /* Base64_Decode() */


/* SBP checksum calculation --------------------------------------------------*/
static uint16_t sbp_checksum(uint8_t *buff, int len)
{
  int i;
  uint16_t crc = 0;
  for (i=0; i<len; i++) {
    crc = (crc << 8) ^ CRC_16CCIT_LookUp[ (crc >> 8) ^ buff[i] ];
  }
  return crc;
}

/* 8-bit week -> full week ---------------------------------------------------*/
static void adj_utcweek(gtime_t time, double *utc)
{
  int week;

  if (utc[3]>=256.0) return;
  time2gpst(time,&week);
  utc[3]+=week/256*256;
  if      (utc[3]<week-128) utc[3]+=256.0;
  else if (utc[3]>week+128) utc[3]-=256.0;
}
/* adjust daily roll-over of time ---------------------------------------------*/
static gtime_t adjday(gtime_t time, double tod)
{
  double ep[6],tod_p;
  time2epoch(time,ep);
  tod_p=ep[3]*3600.0+ep[4]*60.0+ep[5];
  if      (tod<tod_p-43200.0) tod+=86400.0;
  else if (tod>tod_p+43200.0) tod-=86400.0;
  ep[3]=ep[4]=ep[5]=0.0;
  return timeadd(epoch2time(ep),tod);
}


/* flush observation data buffer ---------------------------------------------*/
static int flushobuf(raw_t *raw) {
    gtime_t time0={0};
    int i,j,n=0;

    trace(3,"flushobuf: n=%d\n", raw->obuf.n);

    /* copy observation data buffer */
    for (i=0; i<raw->obuf.n && i<MAXOBS; i++) {
        if (!satsys(raw->obuf.data[i].sat,NULL)) continue;
        if (raw->obuf.data[i].time.time==0) continue;
        raw->obs.data[n++] = raw->obuf.data[i];
    }
    raw->obs.n=n;

    /* clear observation data buffer */
    for (i=0;i<MAXOBS;i++) {
        raw->obuf.data[i].time=time0;
        for (j=0;j<NFREQ+NEXOBS;j++) {
            raw->obuf.data[i].L[j]=raw->obuf.data[i].P[j]=0.0;
            raw->obuf.data[i].D[j]=0.0;
            raw->obuf.data[i].SNR[j]=raw->obuf.data[i].LLI[j]=0;
            raw->obuf.data[i].code[j]=CODE_NONE;
        }
    }
    for (i=0;i<MAXSAT;i++) raw->prCA[i]=raw->dpCA[i]=0.0;
    return n>0 ? 1:0;
}
/* clear buffer --------------------------------------------------------------*/
static void clearbuff(raw_t *raw) {
    raw->buff[0] = 0;
    raw->len = raw->nbyte = 0;
}

/* appropriate calculation of LLI for SBP */
static uint8_t calculate_loss_of_lock(double dt, uint32_t prev_lock_time, uint32_t curr_lock_time) {
  if (prev_lock_time > curr_lock_time) {
/*    fprintf(stderr, "prev_lock_time %d curr_lock_time %d\n", prev_lock_time, curr_lock_time);*/
    return 1;
  }
  else if ((prev_lock_time == curr_lock_time) && (dt >= prev_lock_time)) {
    /*fprintf(stderr, "2\n");*/
    return 1;
  }
  else if ((prev_lock_time == curr_lock_time) && (dt < prev_lock_time)) {
    return 0;
  }
  else if ((prev_lock_time < curr_lock_time) && \
      (dt >= (2 * curr_lock_time - prev_lock_time))) {
    /*fprintf(stderr, "3\n");*/
    return 1;
  }
  else if ((prev_lock_time < curr_lock_time) && \
     (curr_lock_time < dt && dt < (2 * curr_lock_time - prev_lock_time))) {
    /*fprintf(stderr, "4\n");*/
    return 1;
  }
  else if ((prev_lock_time < curr_lock_time) && (dt <= curr_lock_time)) return 0;
  else {
    /*fprintf(stderr, "5\n");*/
    return 1;
  }
}

/* decode SBF measurements message (observables) -----------------------------*/
static int decode_msgobs(raw_t *raw){
  gtime_t time;
  double tow, dResTow, dPseudoRng, dCarrPhase, dDoppler, dDeltaTime;
  int16_t i,ii,sat,n,week;
  uint8_t *p=(raw->buff)+6;                   /* jump to TOW location */
  uint8_t  uNobs, uLockInfo;
  uint32_t sys, uPrevLockTime=0, uCurrLockTime=0;
  uint8_t   uFlags, uSatId, uBandCode, uCN0, uCode, uFreq, uSlip, uHalfC;
  int iDidFlush=0, iSatFound=0;

  trace(4,"SBF decode_msgobs: len=%d\n",raw->len);

  /* Get time information */
  tow      = U4(p);         /* TOW in ms */
  dResTow  = I4(p+4);       /* residual Time Of Week */
  week     = U2(p+8);       /* GPS week */
  uNobs    = p[10];         /* number of observations in message */
  /*  uSeqSize = uNobs>>4; */
  /*  uSeqIdx  = uNobs&0xf; */
  uNobs    = ((raw->len) - 19) / 17;

  time = gpst2time(week, tow*0.001 + dResTow*1e-9);
  /* start new observation period */
  dDeltaTime = fabs(timediff(time, raw->time));
  if ((dDeltaTime)>1E-6) {
    n = 0;
    iDidFlush = flushobuf(raw);
  } else {
    n = raw->obuf.n;
    iDidFlush = 0;
  }

  /* set the pointer from TOW to the beginning of sub-block */
  p = p + 11;

  /* add observations */
  for (i=0; i<uNobs && i<MAXOBS; i++, p+=17) {

    dPseudoRng  = U4(p) *0.02;      /* pseudorange observation in 2cm units */
    dCarrPhase  = I4(p+4);          /* carrier phase integer cycles */
    dCarrPhase += p[ 8] /256.0;     /* carrier phase fractional cycles */
    dDoppler    = I2(p+9);          /* Doppler shift in integer Hz */
    dDoppler   += p[11] /256.0;     /* fractional part of Doppler shift */
    uCN0        = p[12];            /* C/N0 */
    uLockInfo   = p[13] & 0xf;      /* lock time */
    uFlags      = p[14];            /* observation flags */
    uSatId      = p[15];
    uBandCode   = p[16];

    /* phase polarity flip option (-INVCP) */
    if (strstr(raw->opt, "INVCP")) {
      dCarrPhase = -dCarrPhase;
    }

    switch (uBandCode) {
    case 0: /* GPS L1C/A */
      uCode = CODE_L1C;  sys = SYS_GPS; uFreq = 0; break;
    case 1: /* GPS L2CM */
      uCode = CODE_L2S;  sys = SYS_GPS; uFreq = 1; break;
    case 2: /* SBAS L1C/A */
      uCode = CODE_L1C;  sys = SYS_SBS; uFreq = 0; break;
    case 3: /* Glonass L1C/A */
      uCode = CODE_L1C;  sys = SYS_GLO; uFreq = 0; break;
    case 4: /* Glonass L2C/A */
      uCode = CODE_L2C;  sys = SYS_GLO; uFreq = 1; break;
    case 5: /* GPS L1P */
      uCode = CODE_L1P;  sys = SYS_GPS; uFreq = 0; break;
    case 6: /* GPS L2P */
      uCode = CODE_L2P;  sys = SYS_GPS; uFreq = 1; break;
    default:
      uCode = CODE_NONE; sys = SYS_GPS; uFreq = 0;
      break;
    }

    /* store satellite number */
    sat = satno(sys, uSatId);
    if (sat == 0) {
      continue;
    }

    iSatFound = 0;
    for (ii=0; ii<n; ii++) {
      if (raw->obuf.data[ii].sat == sat) {
        iSatFound = 1;
        break;
      }
    }

/*    fprintf(stderr, "%.1f (raw->halfc[%d][%d]) %u  uLockInfo %u \n",
        0.001*tow,
        sat-1, uFreq, (raw->halfc[sat-1][uFreq]),
        uLockInfo);*/

    raw->obuf.data[ii].time = time;
    raw->obuf.data[ii].sat  = (unsigned char)sat;

    /* store signal info */
    if (uFreq < NFREQ+NEXOBS) {
      raw->obuf.data[ii].L[uFreq]    = dCarrPhase;
      raw->obuf.data[ii].P[uFreq]    = dPseudoRng;
      raw->obuf.data[ii].D[uFreq]    = (float) dDoppler;
      raw->obuf.data[ii].SNR[uFreq]  = uCN0;
      raw->obuf.data[ii].code[uFreq] = uCode;

      uPrevLockTime = puRtcmPhaseRangeLockTimeTable[(raw->halfc[sat-1][uFreq])];
      uCurrLockTime = puRtcmPhaseRangeLockTimeTable[uLockInfo];
      uSlip  = (uLockInfo==0) ? 0x1 : 0x0;
      uSlip |= calculate_loss_of_lock(dDeltaTime*1000.0, uPrevLockTime, uCurrLockTime);
      uHalfC = (uFlags & 0x4) ? 0:1;
      if (uHalfC) {
        uSlip |= 0x2; /* half-cycle ambiguity */
      }

      raw->obuf.data[ii].LLI[uFreq] |= uSlip;
      raw->halfc[sat-1][uFreq] = uLockInfo;
    }

    /* Receiver channel goes up */
    if (!iSatFound) n++;
  }
  raw->time = time;
  raw->obuf.n = n;
  return iDidFlush;
}


/* adjust weekly roll-over of gps time ----------------------------------------*/
static gtime_t adjweek(gtime_t time, double tow)
{
  double tow_p;
  int week;
  tow_p=time2gpst(time,&week);
  if      (tow<tow_p-302400.0) tow+=604800.0;
  else if (tow>tow_p+302400.0) tow-=604800.0;
  return gpst2time(week,tow);
}


/* decode SBP nav message for GPS (navigation data) --------------------------*/
static int decode_gpsnav(raw_t *raw) {

  uint8_t *puiTmp = (raw->buff)+6;
  eph_t eph={0};
  double toc;
  uint8_t prn, sat;
  uint16_t uWeekE, uWeekC, uRes;

  trace(4,"SBP decode_gpsnav: len=%d\n",raw->len);

  if ((raw->len)<187) {
    trace(2,"SBP decode_gpsnav frame length error: len=%d\n",raw->len);
    return -1;
  }

  prn = U2(puiTmp)+1;         /* GPS coded as PRN-1 */
  sat = satno(SYS_GPS,prn);

  if (sat == 0) return -1;

  if (!((prn>=1)&&(prn<=37))){
    trace(2,"SBP decode_gpsnav prn error: sat=%d\n",prn);
    return -1;
  }

  eph.code   = U1(puiTmp +   2);
  uRes       = U1(puiTmp +   3);
  eph.toes   = U4(puiTmp +   4);
  uWeekE     = U2(puiTmp +   8);
  eph.sva    = (int)R8(puiTmp +  10);          /* URA */
  eph.fit    = U4(puiTmp +  14) ? 0 : 4;
  eph.flag   = U1(puiTmp +  15);

  eph.tgd[0] = R8(puiTmp +  24);
  eph.crs    = R8(puiTmp +  32);
  eph.crc    = R8(puiTmp +  40);
  eph.cuc    = R8(puiTmp +  48);
  eph.cus    = R8(puiTmp +  56);
  eph.cic    = R8(puiTmp +  64);
  eph.cis    = R8(puiTmp +  72);

  eph.deln   = R8(puiTmp +  80);
  eph.M0     = R8(puiTmp +  88);
  eph.e      = R8(puiTmp +  96);
  eph.A      = pow(R8(puiTmp + 104), 2);
  eph.OMG0   = R8(puiTmp + 112);
  eph.OMGd   = R8(puiTmp + 120);
  eph.omg    = R8(puiTmp + 128);
  eph.i0     = R8(puiTmp + 136);
  eph.idot   = R8(puiTmp + 144);

  eph.f0     = R8(puiTmp + 152);
  eph.f1     = R8(puiTmp + 160);
  eph.f2     = R8(puiTmp + 168);

  toc        = U4(puiTmp + 176);
  uWeekC     = U2(puiTmp + 180); /* WN */
  eph.iode   = U1(puiTmp + 182);
  eph.iodc   = U2(puiTmp + 183);


  eph.week = adjgpsweek(uWeekE);
  eph.toe = gpst2time(eph.week,eph.toes);
  eph.toc = gpst2time(uWeekC,toc);
  eph.ttr = raw->time;

  if (!strstr(raw->opt,"EPHALL")) {
    if ((eph.iode==raw->nav.eph[sat-1].iode) &&
        (eph.iodc==raw->nav.eph[sat-1].iodc)) return 0;
  }

  eph.sat=sat;
  raw->nav.eph[sat-1]=eph;
  raw->ephsat=sat;
  return 2;
}


/* decode SBF gpsion --------------------------------------------------------*/
static int decode_gpsion(raw_t *raw){

  uint8_t *puiTmp = (raw->buff)+6;
  uint32_t uTowMs;
  uint16_t uWeek;

  trace(4,"SBP decode_gpsion: len=%d\n",raw->len);

  if ((raw->len)<72) {
    trace(2,"SBP decode_gpsion frame length error: len=%d\n",raw->len);
    return -1;
  }

  /* Get time information */
  uTowMs   = U4(puiTmp +  0);       /* TOW in ms */
  uWeek    = I4(puiTmp +  4);       /* Week number */

  raw->nav.ion_gps[0] = R8(puiTmp +  6);
  raw->nav.ion_gps[1] = R8(puiTmp + 14);
  raw->nav.ion_gps[2] = R8(puiTmp + 18);
  raw->nav.ion_gps[3] = R8(puiTmp + 22);
  raw->nav.ion_gps[4] = R8(puiTmp + 30);
  raw->nav.ion_gps[5] = R8(puiTmp + 38);
  raw->nav.ion_gps[6] = R8(puiTmp + 46);
  raw->nav.ion_gps[7] = R8(puiTmp + 54);

  return 9;
}



/* decode SBF raw message --------------------------------------------------*/
static int decode_sbp(raw_t *raw)
{
  uint16_t crc, uCalcCrc;

  /* read the SBF block ID and revision */
  int type   = U2(raw->buff+1);
  int sender = U2(raw->buff+3);

  if ((sender==0) && (NULL==strstr(raw->opt,"CONVBASE"))) return 0;
  if ((sender!=0) && (NULL!=strstr(raw->opt,"CONVBASE"))) return 0;

  trace(3,"decode_sbp: type=%04x len=%d\n",type,raw->len);

  /* read the SBF block CRC */
  crc = U2(raw->buff+(raw->len)-2);

  /* checksum skipping first 4 bytes */
  uCalcCrc = sbp_checksum(raw->buff+1, raw->len-3);
  if (uCalcCrc !=  crc){
    trace(2,"SBP checksum error: type=%04x len=%d\n",type, raw->len);
    return -1;
  }

  if (raw->outtype) {
    sprintf(raw->msgtype,"SBP 0x%04X (%4d):",type, raw->len);
  }

  switch (type) {
  case ID_MEASEPOCH: return decode_msgobs(raw);
  case ID_MSGEPHGPS: return decode_gpsnav(raw);
  case ID_MSGIONGPS: return decode_gpsion(raw);

  default:
    trace(3,"decode_sbp: unused frame type=%04x len=%d\n",type,raw->len);
    /* there are many more SBF blocks to be extracted */
  }
  return 0;
}

/* sync to the beginning of a block ------------------------------------------*/
static int sync_sbp(uint8_t *buff, uint8_t data)
{
  buff[0] = data;
  return buff[0] == SBP_SYNC1;
}
/* input sbf raw data from stream ----------------------------------------------
 * get to the next sbf raw block from stream
 * args   : raw_t  *raw   IO     receiver raw data control struct
 *          uint8_t data I stream data (1byte)
 * return : status (-1: error message, 0: no message, 1: input observation data,
 *                  2: input ephemeris, 3: input sbas message,
 *                  9: input ion/utc parameter)
 *-----------------------------------------------------------------------------*/
extern int input_sbp(raw_t *raw, uint8_t data)
{
  trace(5,"input_sbp: data=%02x\n",data);

  if (raw->nbyte==0) {
    if (sync_sbp(raw->buff,data)) raw->nbyte=1;
    return 0;
  }
  raw->buff[raw->nbyte++]=data;

  if (raw->nbyte<6) return 0;

  if ((raw->len=(8 + raw->buff[5]))>MAXRAWLEN) {
    trace(2,"sbp length error: len=%d\n",raw->len);
    raw->nbyte=0;
    return -1;
  }
  if (raw->nbyte<raw->len) return 0;
  raw->nbyte=0;

  return decode_sbp(raw);
}


/* start input file ----------------------------------------------------------*/
static void startfile(raw_t *raw)
{
    raw->tod=-1;
    raw->obuf.n=0;
    raw->buff[0]=0;
}
/* end input file ------------------------------------------------------------*/
static int endfile(raw_t *raw)
{
    /* flush observation data buffer */
    if (!flushobuf(raw)) return -2;
    raw->obuf.n=0;
    return 1;
}

/* sbf raw block finder --------------------------------------------------------
 * get to the next sbf raw block from file
 * args   : raw_t  *raw   IO     receiver raw data control struct
 *          FILE   *fp    I      file pointer
 * return : status(-2: end of file, -1...9: same as above)
 *-----------------------------------------------------------------------------*/
extern int input_sbpf(raw_t *raw, FILE *fp)
{
  int i,data,stat;

  trace(4,"input_sbpf:\n");

  if (raw->flag) {
      startfile(raw);
      raw->flag=0;
  }

  /* go to the beginning of the first block */
  if (raw->nbyte==0) {
    for (i=0;;i++) {
      if ((data=fgetc(fp))==EOF) return endfile(raw);
      if (sync_sbp(raw->buff,(uint8_t)data)) break;
      if (i>=MAXRAWLEN) return 0;
    }
  }

  /* load block header content (8 bytes) in raw->buff */
  /* since we already read the first byte, we just read the next 5 bytes */
  if (fread(raw->buff+1,1,5,fp) < 5) return endfile(raw);
  raw->nbyte=6;

  /* decode the length of the block and store it in len */
  if ((raw->len = 8 + raw->buff[5]) >MAXRAWLEN) {
    trace(2,"sbp length error: len=%d\n", raw->len);
    raw->nbyte=0;
    return -1;
  }

  /* let's store in raw->buff the whole block of length len */
  /* 8 bytes have been already read, we read raw->len-8 more */
  if (fread(raw->buff+6,1,raw->len-6, fp)<(size_t)(raw->len-6)) return endfile(raw);

  /* decode SBF block */
  stat= decode_sbp(raw);

  clearbuff(raw);
  return stat;
}


/* sbf json block finder --------------------------------------------------------
 * get to the next meaningful sbf json message
 * args   : raw_t  *raw   IO     receiver raw data control struct
 *          FILE   *fp    I      file pointer
 * return : status(-2: end of file, -1...9: same as above)
 *-----------------------------------------------------------------------------*/
extern int input_sbpjsonf(raw_t *raw, FILE *fp)
{
  const char JSON_MSGTYPE_FIELD[] = {"\"msg_type\":"};
  const char JSON_SENDER_FIELD[]  = {"\"sender\":"};
  const char JSON_PAYLOAD_FIELD[] = {"\"payload\":"};
  const char JSON_CRC_FIELD[]  = {"\"crc\":"};
  uint8_t *pcPayloadBeg, *pcPayloadEnd;
  int stat,iRet;
  uint32_t uPayloadSize, uMsgType, uSender, uMsgCrc, uLength;
  char *pcTmp;

  trace(4,"input_sbpjsonf:\n");

  if (raw->flag) {
      startfile(raw);
      raw->flag=0;
  }

  memset(raw->buff, 0, MAXRAWLEN);
  pcTmp = fgets((char*)raw->buff, MAXRAWLEN, fp);
  if (NULL == pcTmp) {
    return endfile(raw);
  }

  pcTmp = strstr((char*)raw->buff, JSON_MSGTYPE_FIELD);
  if (NULL == pcTmp) return 0;
  iRet = sscanf(pcTmp + sizeof(JSON_MSGTYPE_FIELD), "%u", &uMsgType);
  if (0 == iRet) return 0;

  /* avoid parsing the payload if the message isn't supported in the first place */
  if ((uMsgType != ID_MEASEPOCH) &&
    (uMsgType != ID_MSGEPHGPS) &&
    (uMsgType != ID_MSGIONGPS)) {
    return 0;
  }

  /* sender in clear */
  pcTmp = strstr((char*)raw->buff, JSON_SENDER_FIELD);
  if (NULL == pcTmp) return 0;
  iRet = sscanf(pcTmp + sizeof(JSON_SENDER_FIELD), "%u", &uSender);
  if (0 == iRet) return 0;

  /* crc */
  pcTmp = strstr((char*)raw->buff, JSON_CRC_FIELD);
  if (NULL == pcTmp) return 0;
  iRet = sscanf(pcTmp + sizeof(JSON_CRC_FIELD), "%u", &uMsgCrc);
  if (0 == iRet) return 0;

  /* payload */
  pcTmp = strstr((char*)raw->buff, JSON_PAYLOAD_FIELD);
  if (NULL == pcTmp) return 0;
  pcTmp += sizeof(JSON_PAYLOAD_FIELD);

  pcPayloadBeg = (uint8_t*) strchr((char*)pcTmp,        '\"')+1;
  pcPayloadEnd = (uint8_t*) strchr((char*)pcPayloadBeg, '\"')-1;
  uPayloadSize = pcPayloadEnd - pcPayloadBeg + 1;
  /*iPayloadSize = 11 + 17*((iPayloadSize - 11)/17);*/
  memset(puPayloadTmp, 0, sizeof(puPayloadTmp));
  uLength = 256;
  Base64_Decode( pcPayloadBeg, uPayloadSize, puPayloadTmp, &uLength );

  raw->buff[0] = 0x55;                          /* sync char */
  raw->buff[1] = (uMsgType >> 0) & 0xFF;        /* msg type LSB */
  raw->buff[2] = (uMsgType >> 8) & 0xFF;        /* msg type MSB */
  raw->buff[3] = (uSender  >> 0) & 0xFF;        /* sender LSB */
  raw->buff[4] = (uSender  >> 8) & 0xFF;        /* sender MSB */
  raw->buff[5] = uLength;                       /* payload length */
  memcpy(raw->buff+6, puPayloadTmp, uLength);
  raw->buff[6+uLength] = (uMsgCrc >> 0) & 0xFF; /* CRC LSB */
  raw->buff[7+uLength] = (uMsgCrc >> 8) & 0xFF; /* CRC MSB */

  /* decode SBF block */
  raw->len = 8 + uLength;
  stat = decode_sbp(raw);

  clearbuff(raw);
  return stat;
}

