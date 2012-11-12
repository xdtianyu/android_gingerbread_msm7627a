/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/**
    @file OMX_testapp\include\omxdectest.h

*/

#ifndef OMXDECTEST_H_INCLUDED
#define OMXDECTEST_H_INCLUDED

#include <stdio.h>

#ifndef TEST_CASE_H
#include "test_case.h"
#endif

#ifndef OMXDECTESTBASE_H_INCLUDED
#include "omxdectestbase.h"
#endif

#if PROXY_INTERFACE

#ifndef OMX_THREADSAFE_CALLBACKS_H_INLCUDED
#include "omx_threadsafe_callbacks.h"
#endif

#endif

#ifndef __MEDIA_CLOCK_CONVERTER_H
#include "media_clock_converter.h"
#endif

// for Mp4 bitstream
#define BIT_BUFF_SIZE 8024000
#define FRAME_SIZE_FIELD  4
#define FRAME_TIME_STAMP_FIELD 4

#define EXTRAPARTIALFRAME_INPUT_BUFFERS 3

#define PRINT_RESULT


#include <utils/threads.h>
#include "oscl_thread.h"

// Qualcomm PCM Driver
#include "control.h"


class AVCBitstreamObject;
#ifdef INSERT_NAL_START_CODE
static unsigned char NAL_START_CODE[4] = {0, 0, 0, 1};
#define NAL_START_CODE_SIZE 4
#endif
// this definition should be in pv_omxdefs.h
//#define INSERT_NAL_START_CODE

//This macro helps to keep the TestCase pass/fail count
#define OMX_DEC_TEST( condition ) (this->iTestCase->test_is_true_stub( (condition), (#condition), __FILE__, __LINE__ ))

/* Macro to reset the structure for GetParemeter call and also to set the size and version of it*/
#define INIT_GETPARAMETER_STRUCT(name, str)\
    (str).nSize = sizeof (name);\
    (str).nVersion.s.nVersionMajor = SPECVERSIONMAJOR;\
    (str).nVersion.s.nVersionMinor = SPECVERSIONMINOR;\
    (str).nVersion.s.nRevision = SPECREVISION;\
    (str).nVersion.s.nStep = SPECSTEP;

//Macro to verify whether some error occured in the openmax API or not
//'e' is the return value of the API and 's' is the string to be printed with the error
#define CHECK_ERROR(e, s) \
    if (OMX_ErrorNone != (e))\
    {\
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s Error, Stop the test case", s)); \
        iState = StateError;\
        RunIfNotReady();\
        break;\
    }


//Macro to verify whether some error occured in memory allocation or not
//m is the pointer to which memory has been allocated and s is the string to be printed in case of error
#define CHECK_MEM(m, s) \
    if (NULL == (m))\
    {\
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "%s Error, Memory Allocation Failed", s)); \
        iState = StateError;\
        RunIfNotReady();\
        break;\
    }


class OmxComponentDecTest;

// test_base-based class which will run async tests on pvPlayer engine
class OmxDecTest_wrapper : public test_case

{
    public:
        OmxDecTest_wrapper(FILE *filehandle, const int32 &aFirstTest, const int32 &aLastTest,
                           char aInFileName[], char aInFileName2[], char aOutFileName[],
                           char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                           char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode);

        // From test_case
        virtual void test();
        //This function will help us to act like an observer
        void TestCompleted();

        enum DecTests
        {
            GET_ROLES_TEST = 0,
            BUFFER_NEGOTIATION_TEST,
            DYNAMIC_PORT_RECONFIG,
            PORT_RECONFIG_TRANSITION_TEST,
            PORT_RECONFIG_TRANSITION_TEST_2,
            PORT_RECONFIG_TRANSITION_TEST_3,
            FLUSH_PORT_TEST,
            EOS_AFTER_FLUSH_PORT_TEST,
            MULTIPLE_INSTANCE_TEST,
            NORMAL_SEQ_TEST,
            NORMAL_SEQ_TEST_USEBUFF,
            ENDOFSTREAM_MISSING_TEST,
            PARTIAL_FRAMES_TEST,
            EXTRA_PARTIAL_FRAMES_TEST,
            INPUT_OUTPUT_BUFFER_BUSY_TEST,
            PAUSE_RESUME_TEST,
            REPOSITIONING_TEST,

            PLAYBACK_TEST = 100
        };


    private:
        OmxComponentDecTest* iTestApp;
        int32 iCurrentTestNumber;
        int32 iFirstTest;
        int32 iLastTest;

        FILE *iFilehandle;
        OMX_BOOL iInitSchedulerFlag;

        char        iInFileName[200];
        char        iInFileName2[200];
        char        iOutFileName[200];
        char        iRefFile[200];
        OMX_STRING  iName;
        OMX_STRING  iRole;
        char        iFormat[10];
        uint32      iNumberOfChannels;
        uint32      iSamplingFrequency;
        uint32      iFileType;
        uint32      iBandMode;

        // For test results
        int32 iTotalSuccess;
        int32 iTotalError;
        int32 iTotalFail;
};


//Omx Decoder test suite
class OmxDecTestSuite : public test_case
{
    public:
        OmxDecTestSuite(FILE *filehandle, const int32 &aFirstTest,
                        const int32 &aLastTest,
                        char aInFileName[], char aInFileName2[], char aOutFileName[],
                        char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                        char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode);
    private:
        OmxDecTest_wrapper *iWrapper;
};


//Main AO class for the TestApplication
class OmxComponentDecTest : public OmxDecTestBase
{
    public:

        OmxComponentDecTest(FILE *filehandle, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                            char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                            char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) : OmxDecTestBase("OMX_Comp_DecTestApp")

        {
            iConsOutFile = filehandle;
            ipInputFile = aInputFile;
            ipOutputFile = aOutputFile;
            iName = aName;
            iRole = aRole;
            iNumberOfChannels = aChannels;
            iPCMSamplingRate = aPCMSamplingRate;
            iFileType = aFileType;
            iBandMode = aBandMode;

            oscl_strncpy(iOutFileName, aOutFileName, oscl_strlen(aOutFileName) + 1);
            oscl_strncpy(iRefFile, aRefFileName, oscl_strlen(aRefFileName) + 1);
            oscl_strncpy(iFormat, aFormat, oscl_strlen(aFormat) + 1);

            iTestStatus = OMX_TRUE;
            pGetInputFrame = NULL;
            iInputPortIndex = OMX_DirInput;
            iOutputPortIndex = OMX_DirOutput;
            iPortSettingsFlag = OMX_FALSE;
            iCallbackCounter = 0;
            //Default value
            iAmrFileType = OMX_AUDIO_AMRFrameFormatFSF;
            iAmrFileMode = OMX_AUDIO_AMRBandModeNB0;
            iFrameTimeStamp = 0;
            iNoMarkerBitTest = OMX_FALSE;
            iExtraPartialFrameTest = OMX_FALSE;
            iDynamicPortReconfigTest = OMX_FALSE;
            iDecTimeScale = 1000;
            iOmxTimeScale = 1000000;
            iInputTimestampClock.set_timescale(iDecTimeScale); // keep the timescale set to input timestamp

            iTestCase = aTestCase;
        }

        void VerifyOutput(OMX_U8 aTestName[]);

        //Keep it public to be accessed from main()
        OMX_BOOL iPortSettingsFlag;

        OmxDecTest_wrapper *iTestCase;

    protected:
        FILE*       iConsOutFile;
        FILE*       ipInputFile;
        FILE*       ipOutputFile;
        char        iOutFileName[200];
        char        iRefFile[200];
        char        iFormat[10];
        OMX_STRING  iName;
        OMX_STRING  iRole;
        OMX_BOOL    iTestStatus;
        OMX_S32     iInputPortIndex;
        OMX_S32     iOutputPortIndex;
        OMX_S32     iCallbackCounter;
        OMX_S32     iFrameTimeStamp;

        //For AMR Component //Input File type information for AMR decoder
        OMX_AUDIO_AMRFRAMEFORMATTYPE iAmrFileType;
        OMX_AUDIO_AMRBANDMODETYPE iAmrFileMode;

        // Audio parameters
        // the output buffer size is calculated from the parameters below
        uint32 iPCMSamplingRate;
        uint32 iNumberOfChannels;
        uint32 iFileType;
        uint32 iBandMode;
        uint32 iSamplesPerFrame;
        OMX_AUDIO_CODINGTYPE iAudioCompressionFormat;

        OMX_BOOL iNoMarkerBitTest;
        OMX_BOOL iExtraPartialFrameTest;
        OMX_BOOL iDynamicPortReconfigTest;

        OMX_U32 iDecTimeScale;
        OMX_U32 iOmxTimeScale;
        MediaClockConverter iInputTimestampClock;

        OMX_ERRORTYPE GetInput();
    public:
        //GetInputFrame routine would be different for different components
        OMX_ERRORTYPE GetInputFrameAvc();
        OMX_ERRORTYPE GetInputFrameMpeg4();
        OMX_ERRORTYPE GetInputFrameH263();
        OMX_ERRORTYPE GetInputFrameAac();
        OMX_ERRORTYPE GetInputFrameAmr();
        OMX_ERRORTYPE GetInputFrameWmv();
        OMX_ERRORTYPE GetInputFrameRvRa();
        OMX_ERRORTYPE GetInputFrameMp3();
        OMX_ERRORTYPE GetInputFrameWma();

        //Function pointer that will point to the correct component's function
        OMX_ERRORTYPE(OmxComponentDecTest::*pGetInputFrame)();

    protected:
        bool WriteOutput(OMX_U8* aOutBuff, OMX_U32 aSize);
        void StopOnError();

        OMX_BOOL PrepareComponent();
        OMX_BOOL NegotiateComponentParametersAudio();
        OMX_BOOL NegotiateComponentParametersVideo();
        OMX_BOOL HandlePortReEnable();

        OMX_BOOL GetSetCodecSpecificInfo();

        OMX_TICKS ConvertTimestampIntoOMXTicks(const MediaClockConverter& src);

    private:
        void Run();

};


typedef enum msgType
{
  CONTROL,
  DATA
}MsgType_t;


typedef struct info {
    MsgType_t msgType;
    int fd;
    OMX_COMPONENTTYPE    *hComponent;
    OMX_BUFFERHEADERTYPE *bufHdr;
}HostPCMInfo_t;

class OmxDecTestPlayback : public OmxComponentDecTest
{
    public:

        OmxDecTestPlayback(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                            char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                            char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
             iPCMDriverFD = -1;
             iSessionID = 0;
             iDeviceID = 0;
             iControl = 0;
       };

    private:

        void Run();

        int iPCMDriverFD;
        OMX_U16 iSessionID;
        OMX_U32 iDeviceID ;
        OMX_S32 iControl;
        char iDeviceStr[44];

        OMX_BOOL OpenPCMDriver();

        OMX_ERRORTYPE FillBufferDone(OMX_OUT OMX_HANDLETYPE aComponent,
            OMX_OUT OMX_PTR aAppData,
            OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer);
        OMX_BOOL PrepareAudioComponent();
        OMX_BOOL SetAudioParameters();
        OMX_ERRORTYPE ReadBuffer(OMX_U32 Index);
};

//TestApplication class to test the API OMX_UseBuffer
class OmxDecTestUseBuffer : public OmxComponentDecTest
{
    public:

        OmxDecTestUseBuffer(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                            char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                            char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        { };

    private:

        void Run();
};

//TestApplication class to test missing EOS case
class OmxDecTestEosMissing : public OmxComponentDecTest
{
    public:

        OmxDecTestEosMissing(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                             char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                             char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        { };

    private:

        void Run();
};

//TestApplication class to test sending chunks of input data without EndOfFrame marker
class OmxDecTestWithoutMarker : public OmxComponentDecTest
{
    public:

        OmxDecTestWithoutMarker(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                                char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OMX_AUDIO_AMRFRAMEFORMATTYPE aAmrFileType,
                                OMX_AUDIO_AMRBANDMODETYPE aAMRFileBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
            iAmrFileType = aAmrFileType;
            iAmrFileMode = aAMRFileBandMode;
        };

    private:

        void Run();
};

//TestApplication class to test sending partial frames to the component
class OmxDecTestPartialFrames : public OmxComponentDecTest
{
    public:

        OmxDecTestPartialFrames(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                                char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        { };

    private:

        void Run();
};

//TestApplication class to test more partial frames than the input buffers
class OmxDecTestExtraPartialFrames : public OmxComponentDecTest
{
    public:

        OmxDecTestExtraPartialFrames(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                                     char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                     char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        { };

    private:

        void Run();
};



//TestApplication class to simulate input/output buffer busy case
class OmxDecTestBufferBusy : public OmxComponentDecTest
{
    public:

        OmxDecTestBufferBusy(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                             char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                             char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        { };

    private:

        void Run();
};

//TestApplication class to pause the buffer processing inbetween and then resume
class OmxDecTestPauseResume : public OmxComponentDecTest
{
    public:

        OmxDecTestPauseResume(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                              char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                              char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels,aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
            iFrameCount = 0;
            iPauseCommandSent = OMX_FALSE;
        };

    private:

        void Run();

        OMX_U32 iFrameCount;
        OMX_BOOL iPauseCommandSent;
};


//TestApplication class to verify repositioning logic of the component
class OmxDecTestReposition : public OmxComponentDecTest
{
    public:

        OmxDecTestReposition(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                             char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                             char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
            iFrameCount = 0;
            iRepositionCommandSent = OMX_FALSE;
        };

    private:

        OMX_ERRORTYPE EventHandler(OMX_OUT OMX_HANDLETYPE aComponent,
                                   OMX_OUT OMX_PTR aAppData,
                                   OMX_OUT OMX_EVENTTYPE aEvent,
                                   OMX_OUT OMX_U32 aData1,
                                   OMX_OUT OMX_U32 aData2,
                                   OMX_OUT OMX_PTR aEventData);
        void Run();
        OMX_BOOL ResetStream();

        OMX_U32 iFrameCount;
        OMX_BOOL iRepositionCommandSent;

};


//AVC specific TestApplication class which will drop random NAL's in a bitstream
class OmxDecTestMissingNALTest : public OmxComponentDecTest
{
    public:

        OmxDecTestMissingNALTest(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                                 char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                 char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
            iFramesDropped = 0;
        };

    private:

        OMX_ERRORTYPE GetInputFrameAvc();
        void Run();
        OMX_S32 iFramesDropped;
};


//AVC specific TestApplication class to corrupt a few bits in some of the NAL's
class OmxDecTestCorruptNALTest : public OmxComponentDecTest
{
    public:

        OmxDecTestCorruptNALTest(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                                 char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                 char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
            iFramesCorrupt = 0;
            iBitError = 0;
        };

    private:

        OMX_ERRORTYPE GetInputFrameAvc();
        void Run();
        OMX_S32 iFramesCorrupt;
        OMX_S32 iBitError;
};

class OmxDecTestIncompleteNALTest : public OmxComponentDecTest
{
    public:

        OmxDecTestIncompleteNALTest(FILE* aConsOutFile, FILE* aInputFile, FILE* aOutputFile, char aOutFileName[],
                                    char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                    char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode, OmxDecTest_wrapper *aTestCase) :

                OmxComponentDecTest(aConsOutFile, aInputFile, aOutputFile, aOutFileName,
                                    aRefFileName, aName, aRole, aFormat,
                                    aChannels, aPCMSamplingRate, aFileType, aBandMode, aTestCase)
        {
            iIncompleteFrames = 0;
        };

    private:

        OMX_ERRORTYPE GetInputFrameAvc();
        void Run();
        OMX_S32 iIncompleteFrames;
};



enum AVCOMX_Status
{
    AVCOMX_SUCCESS = 0,
    AVCOMX_FAIL,
    AVCOMX_NO_NEXT_SC
};

typedef enum
{
    AVC_NALTYPE_SLICE = 1,  /* non-IDR non-data partition */
    AVC_NALTYPE_DPA = 2,    /* data partition A */
    AVC_NALTYPE_DPB = 3,    /* data partition B */
    AVC_NALTYPE_DPC = 4,    /* data partition C */
    AVC_NALTYPE_IDR = 5,    /* IDR NAL */
    AVC_NALTYPE_SEI = 6,    /* supplemental enhancement info */
    AVC_NALTYPE_SPS = 7,    /* sequence parameter set */
    AVC_NALTYPE_PPS = 8,    /* picture parameter set */
    AVC_NALTYPE_AUD = 9,    /* access unit delimiter */
    AVC_NALTYPE_EOSEQ = 10, /* end of sequence */
    AVC_NALTYPE_EOSTREAM = 11, /* end of stream */
    AVC_NALTYPE_FILL = 12   /* filler data */
} AVCOMXNalUnitType;

class AVCBitstreamObject
{

    public:
        //! constructor
        explicit AVCBitstreamObject(FILE *afp)
        {
            oscl_memset(this, 0, sizeof(AVCBitstreamObject));
            ipAVCFile = afp;
            iPos = 0;
            iActualSize = 0;
            iBufferSize = BIT_BUFF_SIZE_AVC;
            ipBuffer = new OMX_U8[iBufferSize];
            if (ipBuffer && ipAVCFile)
            {
                iStatus = OMX_TRUE;
            }
            else
            {
                iStatus = OMX_FALSE;
            }
        }

        AVCBitstreamObject()
        {
            oscl_memset(this, 0, sizeof(AVCBitstreamObject));
            iPos = 0;
            iActualSize = 0;
            iBufferSize = BIT_BUFF_SIZE_AVC;
            ipBuffer = new OMX_U8[iBufferSize];
            if (!ipBuffer)
            {
                iStatus = OMX_TRUE;
            }
            else
            {
                iStatus = OMX_FALSE;
            }
        }

        //! Destructor
        ~AVCBitstreamObject()
        {
            if (ipBuffer)
            {
                delete [] ipBuffer;
                ipBuffer = NULL;
            }
            oscl_memset(this, 0, sizeof(AVCBitstreamObject));
        }

        //! most important function to get next NAL data plus NAL size, also serves as the callback function of videoCtrl
        AVCOMX_Status GetNextFullNAL(OMX_U8** aNalBuffer, OMX_S32* aNalSize);
        AVCOMX_Status AVCAnnexBGetNALUnit(uint8 *bitstream, uint8 **nal_unit, int32 *size);

        void    ResetInputStream();

    private:

        //! read data from bitstream, this is the only function to read data from file
        OMX_S32 Refill();
        OMX_U8* ipBuffer;
        OMX_S32 iPos;
        OMX_S32 iBufferSize;
        OMX_S32 iActualSize;
        OMX_BOOL  iStatus;
        FILE  *ipAVCFile;
};


//MP3 specific tables to determine the frame boundaries

#define Qfmt_28(a)(int32(double(0x10000000)*a))

/*
 *  144000./s_freq
 */
const OMX_S32 InvFreq[4] =
{
    Qfmt_28(3.26530612244898),
    Qfmt_28(3.0),
    Qfmt_28(4.5),
    0
};


/* 1: MPEG-1, 0: MPEG-2 LSF, 1995-07-11 shn */
const OMX_S16  Mp3Bitrate[3][15] =
{
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
};



typedef struct
{
    uint8       *pBuffer;
    uint32      usedBits;
    uint32      inputBufferCurrentLength;
    uint32      offset;
} TmpMp3Bits;


class Mp3BitstreamObject
{
    public:
        //! constructor
        explicit Mp3BitstreamObject(FILE *afp)
        {
            oscl_memset(this, 0, sizeof(Mp3BitstreamObject));
            ipMp3File = afp;
            iInputBufferMaxLength = 512;
            pVars = new TmpMp3Bits;
            ipBuffer = new OMX_U8[BIT_BUFF_SIZE_MP3];
            if (ipBuffer && ipMp3File)
            {
                iStatus = OMX_TRUE;
                ipBufferOrig = ipBuffer;
            }
            else
            {
                iStatus = OMX_FALSE;
            }
        }

        Mp3BitstreamObject()
        {
            oscl_memset(this, 0, sizeof(Mp3BitstreamObject));
            iInputBufferMaxLength = 512;
            ipBuffer = new OMX_U8[BIT_BUFF_SIZE_MP3];
            pVars = new TmpMp3Bits;
            if (!ipBuffer)
            {
                iStatus = OMX_TRUE;
                ipBufferOrig = ipBuffer;
            }
            else
            {
                iStatus = OMX_FALSE;
            }
        }

        //! Destructor
        ~Mp3BitstreamObject()
        {
            if (ipBufferOrig)
            {
                delete [] ipBufferOrig;
                ipBufferOrig = NULL;
            }
            oscl_memset(this, 0, sizeof(Mp3BitstreamObject));
        }

        int32   DecodeReadInput();
        int32   DecodeAdjustInput();
        int32   Mp3FrameSynchronization(OMX_S32* aFrameSize);
        void    ResetInputStream();
        uint8*  ipBuffer;
        int32    iInputBufferUsedLength;
        OMX_S32 iFragmentSizeRead;
        int32     iInputBufferCurrentLength;

    private:
        OMX_BOOL HeaderSync();
        OMX_U32 GetNbits(OMX_S32 NeededBits);
        uint16 GetUpTo9Bits(int32 neededBits);

        int32     iInputBufferMaxLength;
        uint32    iCurrentFrameLength;

        OMX_U8* ipBufferOrig;
        TmpMp3Bits* pVars;
        FILE* ipMp3File;
        OMX_BOOL  iStatus;
};


#endif  // OMXDECTEST_H_INCLUDED
