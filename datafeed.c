/*---------------------------------------------------------------------------
 
  FILENAME:
        datafeed.c
 
  PURPOSE:
        Define the datafeed API.
 
  REVISION HISTORY:
        Date            Engineer        Revision        Remarks
        12/16/2009      M.S. Teel       0               Original
        01/02/2011      M.S. Teel       1               Remove fixed point translation;
                        & P. Sanchez                    add htonf and ntohf utilities.
 
  NOTES:
        
 
  LICENSE:
        Copyright (c) 2009, Mark S. Teel (mark@teel.ws)
  
        This source code is released for free distribution under the terms 
        of the GNU General Public License.
  
----------------------------------------------------------------------------*/

//  ... System header files
#include <errno.h>
#include <stdint.h>


//  ... Local header files
#define DATAFEED_INSTANTIATE        TRUE
#include <datafeed.h>


//  ... methods

/* Start of slurped code.
   Originally:  http://beej.us/guide/bgnet/examples/pack.c
   Root URL:  http://beej.us/guide/bgnet
*/
uint32_t htonf(float f)
{
    uint32_t    p;
    uint32_t    sign;

    if (f < 0)
    {
        sign = 1;
        f = -f;
    }
    else
    {
        sign = 0;
    }

    p = ((((uint32_t)f)&0x7fff)<<16) | (sign<<31); // whole part and sign
    p |= (uint32_t)(((f - (int)f) * 65536.0f))&0xffff; // fraction

    return p;
}

float ntohf(uint32_t p)
{
    float       f = ((p>>16)&0x7fff); // whole part
    f += (p&0xffff) / 65536.0f; // fraction

    if (((p>>31)&0x1) == 0x1)
    {
        f = -f;
    } // sign bit set

    return f;
}
// end of slurped code

static USHORT swapShortNTOH(UCHAR* item)
{
    USHORT*     sh_item = (USHORT*)item;
    USHORT      retVal;

    retVal = ntohs(*sh_item);
    return retVal;
}

static USHORT swapShortHTON(UCHAR* item)
{
    USHORT*     sh_item = (USHORT*)item;
    USHORT      retVal;

    retVal = htons(*sh_item);
    return retVal;
}

static int ReadExact(RADSOCK_ID socket, void *bfr, int len, ULONG msTimeout)
{
    int             rval, index = 0;
    ULONG           cumTime = 0;
    ULONGLONG       readTime;
    UCHAR           *ptr = (UCHAR *)bfr;

    while (index < len && cumTime < msTimeout)
    {
        readTime = radTimeGetMSSinceEpoch ();
        rval = radSocketReadExact(socket, &ptr[index], len - index);
        if (rval < 0)
        {
            if (errno != EINTR && errno != EAGAIN)
            {
                radMsgLog (PRI_HIGH, "ReadExact ERROR: %s", strerror(errno));
                return ERROR;
            }
        }
        else
        {
            index += rval;
        }

        readTime = radTimeGetMSSinceEpoch () - readTime;
        cumTime += (ULONG)readTime;
        if (index < len && cumTime < msTimeout)
        {
            readTime = radTimeGetMSSinceEpoch ();
            radUtilsSleep (9);
            readTime = radTimeGetMSSinceEpoch () - readTime;
            cumTime += (ULONG)readTime;
        }
    }

    return ((index == len) ? len : 0);
}

int datafeedSyncStartOfFrame(RADSOCK_ID socket)
{
    USHORT          start;
    int             retVal;

    retVal = ReadExact(socket, (void *)&start, sizeof (USHORT), DF_WAIT_FIRST);
    if (retVal == ERROR)
    {
        radMsgLog (PRI_HIGH, "datafeed: socket read 1 error - abort!");
        return ERROR;
    }
    else if (retVal == 0)
    {
        return ERROR_ABORT;
    }
    else if (retVal != sizeof(USHORT))
    {
        return FALSE;
    }
    else if (start != DF_LOOP_START_FRAME[0])
    {
        return FALSE;
    }

    if (ReadExact(socket, (void *)&start, sizeof (USHORT), DF_WAIT_MORE)
            != sizeof (USHORT))
    {
        radMsgLog (PRI_HIGH, "datafeed: socket read 2 error - abort!");
        return ERROR;
    }
    else if (start != DF_LOOP_START_FRAME[1])
    {
        return FALSE;
    }

    if (ReadExact(socket, (void *)&start, sizeof (USHORT), DF_WAIT_MORE)
            != sizeof (USHORT))
    {
        radMsgLog (PRI_HIGH, "datafeed: socket read 3 error - abort!");
        return ERROR;
    }
    else if (start != DF_LOOP_START_FRAME[2])
    {
        return FALSE;
    }

    if (ReadExact(socket, (void *)&start, sizeof (USHORT), DF_WAIT_MORE)
            != sizeof (USHORT))
    {
        radMsgLog (PRI_HIGH, "datafeed: socket read 4 error - abort!");
        return ERROR;
    }
    else if (start == DF_LOOP_START_FRAME[3])
    {
        return (int)DF_LOOP_START_FRAME[3];
    }
    else if (start == DF_ARCHIVE_START_FRAME[3])
    {
        return (int)DF_ARCHIVE_START_FRAME[3];
    }
    else if (start == DF_RQST_ARCHIVE_START_FRAME[3])
    {
        return (int)DF_RQST_ARCHIVE_START_FRAME[3];
    }

    return FALSE;
}


int datafeedConvertLOOP_HTON(LOOP_PKT* dest, LOOP_PKT* src)
{
    USHORT*         tempshort;

    dest->barometer                     = htonf(src->barometer);
    dest->stationPressure               = htonf(src->stationPressure);
    dest->altimeter                     = htonf(src->altimeter);
    dest->inTemp                        = htonf(src->inTemp);
    dest->outTemp                       = htonf(src->outTemp);

    dest->inHumidity                    = htons(src->inHumidity);
    dest->outHumidity                   = htons(src->outHumidity);
    dest->windSpeed                     = htons(src->windSpeed);
    dest->windDir                       = htons(src->windDir);
    dest->windGust                      = htons(src->windGust);
    dest->windGustDir                   = htons(src->windGustDir);

    dest->rainRate                      = htonf(src->rainRate);
    dest->sampleRain                    = htonf(src->sampleRain);
    dest->sampleET                      = htonf(src->sampleET);

    dest->radiation                     = htons(src->radiation);

    dest->UV                            = htonf(src->UV);
    dest->dewpoint                      = htonf(src->dewpoint);
    dest->windchill                     = htonf(src->windchill);
    dest->heatindex                     = htonf(src->heatindex);
    dest->stormRain                     = htonf(src->stormRain);

    dest->stormStart                    = htonl(src->stormStart);

    dest->dayRain                       = htonf(src->dayRain);
    dest->monthRain                     = htonf(src->monthRain);
    dest->yearRain                      = htonf(src->yearRain);
    dest->dayET                         = htonf(src->dayET);
    dest->monthET                       = htonf(src->monthET);
    dest->yearET                        = htonf(src->yearET);
    dest->intervalAvgWCHILL             = htonf(src->intervalAvgWCHILL);

    dest->intervalAvgWSPEED             = htons(src->intervalAvgWSPEED);
    dest->yearRainMonth                 = htons(src->yearRainMonth);
    dest->rxCheckPercent                = htons(src->rxCheckPercent);
    dest->tenMinuteAvgWindSpeed         = htons(src->tenMinuteAvgWindSpeed);
    dest->forecastIcon                  = htons(src->forecastIcon);
    dest->forecastRule                  = htons(src->forecastRule);
    dest->txBatteryStatus               = htons(src->txBatteryStatus);
    dest->consBatteryVoltage            = htons(src->consBatteryVoltage);

    dest->extraTemp1                    = htonf(src->extraTemp1);
    dest->extraTemp2                    = htonf(src->extraTemp2);
    dest->extraTemp3                    = htonf(src->extraTemp3);
    dest->soilTemp1                     = htonf(src->soilTemp1);
    dest->soilTemp2                     = htonf(src->soilTemp2);
    dest->soilTemp3                     = htonf(src->soilTemp3);
    dest->soilTemp4                     = htonf(src->soilTemp4);
    dest->leafTemp1                     = htonf(src->leafTemp1);
    dest->leafTemp2                     = htonf(src->leafTemp2);

    tempshort = (USHORT*)&(dest->extraHumid1);
    *tempshort = swapShortHTON(&dest->extraHumid1);
    tempshort = (USHORT*)&(dest->soilMoist1);
    *tempshort = swapShortHTON(&dest->soilMoist1);
    tempshort = (USHORT*)&(dest->leafWet1);
    *tempshort = swapShortHTON(&dest->leafWet1);

    dest->wxt510Hail                    = htonf(src->wxt510Hail);
    dest->wxt510Hailrate                = htonf(src->wxt510Hailrate);
    dest->wxt510HeatingTemp             = htonf(src->wxt510HeatingTemp);
    dest->wxt510HeatingVoltage          = htonf(src->wxt510HeatingVoltage);
    dest->wxt510SupplyVoltage           = htonf(src->wxt510SupplyVoltage);
    dest->wxt510ReferenceVoltage        = htonf(src->wxt510ReferenceVoltage);
    dest->wxt510RainDuration            = htonf(src->wxt510RainDuration);
    dest->wxt510RainPeakRate            = htonf(src->wxt510RainPeakRate);
    dest->wxt510HailDuration            = htonf(src->wxt510HailDuration);
    dest->wxt510HailPeakRate            = htonf(src->wxt510HailPeakRate);
    dest->wxt510Rain                    = htonf(src->wxt510Rain);
    dest->wmr918Pool                    = htonf(src->wmr918Pool);

    tempshort = (USHORT*)&(dest->wmr918Humid3);
    *tempshort = swapShortHTON(&dest->wmr918Humid3);
    tempshort = (USHORT*)&(dest->wmr918WindBatteryStatus);
    *tempshort = swapShortHTON(&dest->wmr918WindBatteryStatus);
    tempshort = (USHORT*)&(dest->wmr918OutTempBatteryStatus);
    *tempshort = swapShortHTON(&dest->wmr918OutTempBatteryStatus);
    tempshort = (USHORT*)&(dest->wmr918poolTempBatteryStatus);
    *tempshort = swapShortHTON(&dest->wmr918poolTempBatteryStatus);
    tempshort = (USHORT*)&(dest->wmr918extra2BatteryStatus);
    *tempshort = swapShortHTON(&dest->wmr918extra2BatteryStatus);

    return OK;
}

int datafeedConvertLOOP_NTOH(LOOP_PKT* dest, LOOP_PKT* src)
{
    USHORT*         tempshort;

    dest->barometer                     = ntohf(src->barometer);
    dest->stationPressure               = ntohf(src->stationPressure);
    dest->altimeter                     = ntohf(src->altimeter);
    dest->inTemp                        = ntohf(src->inTemp);
    dest->outTemp                       = ntohf(src->outTemp);

    dest->inHumidity                    = ntohs(src->inHumidity);
    dest->outHumidity                   = ntohs(src->outHumidity);
    dest->windSpeed                     = ntohs(src->windSpeed);
    dest->windDir                       = ntohs(src->windDir);
    dest->windGust                      = ntohs(src->windGust);
    dest->windGustDir                   = ntohs(src->windGustDir);

    dest->rainRate                      = ntohf(src->rainRate);
    dest->sampleRain                    = ntohf(src->sampleRain);
    dest->sampleET                      = ntohf(src->sampleET);

    dest->radiation                     = ntohs(src->radiation);

    dest->UV                            = ntohf(src->UV);
    dest->dewpoint                      = ntohf(src->dewpoint);
    dest->windchill                     = ntohf(src->windchill);
    dest->heatindex                     = ntohf(src->heatindex);
    dest->stormRain                     = ntohf(src->stormRain);

    dest->stormStart                    = ntohl(src->stormStart);

    dest->dayRain                       = ntohf(src->dayRain);
    dest->monthRain                     = ntohf(src->monthRain);
    dest->yearRain                      = ntohf(src->yearRain);
    dest->dayET                         = ntohf(src->dayET);
    dest->monthET                       = ntohf(src->monthET);
    dest->yearET                        = ntohf(src->yearET);
    dest->intervalAvgWCHILL             = ntohf(src->intervalAvgWCHILL);

    dest->intervalAvgWSPEED             = ntohs(src->intervalAvgWSPEED);
    dest->yearRainMonth                 = ntohs(src->yearRainMonth);
    dest->rxCheckPercent                = ntohs(src->rxCheckPercent);
    dest->tenMinuteAvgWindSpeed         = ntohs(src->tenMinuteAvgWindSpeed);
    dest->forecastIcon                  = ntohs(src->forecastIcon);
    dest->forecastRule                  = ntohs(src->forecastRule);
    dest->txBatteryStatus               = ntohs(src->txBatteryStatus);
    dest->yearRainMonth                 = ntohs(src->yearRainMonth);
    dest->consBatteryVoltage            = ntohs(src->consBatteryVoltage);

    dest->extraTemp1                    = ntohf(src->extraTemp1);
    dest->extraTemp2                    = ntohf(src->extraTemp2);
    dest->extraTemp3                    = ntohf(src->extraTemp3);
    dest->soilTemp1                     = ntohf(src->soilTemp1);
    dest->soilTemp2                     = ntohf(src->soilTemp2);
    dest->soilTemp3                     = ntohf(src->soilTemp3);
    dest->soilTemp4                     = ntohf(src->soilTemp4);
    dest->leafTemp1                     = ntohf(src->leafTemp1);
    dest->leafTemp2                     = ntohf(src->leafTemp2);

    tempshort = (USHORT*)&(dest->extraHumid1);
    *tempshort = swapShortNTOH(&dest->extraHumid1);
    tempshort = (USHORT*)&(dest->soilMoist1);
    *tempshort = swapShortNTOH(&dest->soilMoist1);
    tempshort = (USHORT*)&(dest->leafWet1);
    *tempshort = swapShortNTOH(&dest->leafWet1);

    dest->wxt510Hail                    = ntohf(src->wxt510Hail);
    dest->wxt510Hailrate                = ntohf(src->wxt510Hailrate);
    dest->wxt510HeatingTemp             = ntohf(src->wxt510HeatingTemp);
    dest->wxt510HeatingVoltage          = ntohf(src->wxt510HeatingVoltage);
    dest->wxt510SupplyVoltage           = ntohf(src->wxt510SupplyVoltage);
    dest->wxt510ReferenceVoltage        = ntohf(src->wxt510ReferenceVoltage);
    dest->wxt510RainDuration            = ntohf(src->wxt510RainDuration);
    dest->wxt510RainPeakRate            = ntohf(src->wxt510RainPeakRate);
    dest->wxt510HailDuration            = ntohf(src->wxt510HailDuration);
    dest->wxt510HailPeakRate            = ntohf(src->wxt510HailPeakRate);
    dest->wxt510Rain                    = ntohf(src->wxt510Rain);
    dest->wmr918Pool                    = ntohf(src->wmr918Pool);

    tempshort = (USHORT*)&(dest->wmr918Humid3);
    *tempshort = swapShortNTOH(&dest->wmr918Humid3);
    tempshort = (USHORT*)&(dest->wmr918WindBatteryStatus);
    *tempshort = swapShortNTOH(&dest->wmr918WindBatteryStatus);
    tempshort = (USHORT*)&(dest->wmr918OutTempBatteryStatus);
    *tempshort = swapShortNTOH(&dest->wmr918OutTempBatteryStatus);
    tempshort = (USHORT*)&(dest->wmr918poolTempBatteryStatus);
    *tempshort = swapShortNTOH(&dest->wmr918poolTempBatteryStatus);
    tempshort = (USHORT*)&(dest->wmr918extra2BatteryStatus);
    *tempshort = swapShortNTOH(&dest->wmr918extra2BatteryStatus);

    return OK;
}

int datafeedConvertArchive_HTON(ARCHIVE_PKT* dest, ARCHIVE_PKT* src)
{
    int         index;

    dest->dateTime              = htonl(src->dateTime);
    dest->usUnits               = htonl(src->usUnits);
    dest->interval              = htonl(src->interval);

    for (index = 0; index < DATA_INDEX_MAX; index ++)
    {
        dest->value[index]      = htonf(src->value[index]);
    }

    return OK;
}

int datafeedConvertArchive_NTOH(ARCHIVE_PKT* dest, ARCHIVE_PKT* src)
{
    int         index;

    dest->dateTime              = ntohl(src->dateTime);
    dest->usUnits               = ntohl(src->usUnits);
    dest->interval              = ntohl(src->interval);

    for (index = 0; index < DATA_INDEX_MAX; index ++)
    {
        dest->value[index]      = ntohf(src->value[index]);
    }

    return OK;
}

