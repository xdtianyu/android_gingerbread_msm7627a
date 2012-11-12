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
#include "omxdectest.h"
#include "omxdectest_basic_test.h"

#ifndef OSCL_STRING_UTILS_H_INCLUDED
#include "oscl_string_utils.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef __UNIT_TEST_TEST_ARGS__
#include "unit_test_args.h"
#endif

#ifndef PVLOGGER_STDERR_APPENDER_H_INCLUDED
#include "pvlogger_stderr_appender.h"
#endif

#ifndef PVLOGGER_FILE_APPENDER_H_INCLUDED
#include "pvlogger_file_appender.h"
#endif

#ifndef PVLOGGER_TIME_AND_ID_LAYOUT_H_INCLUDED
#include "pvlogger_time_and_id_layout.h"
#endif

#ifndef OSCL_UTF8CONV_H
#include "oscl_utf8conv.h"
#endif

#include "text_test_interpreter.h"

#define PV_ARGSTR_LENGTH 128

#define PV_OMX_MAX_COMPONENT_NAME_LEN 128
#define FRAME_SIZE_FIELD  4

#define SYNC_WORD_LENGTH_MP3   11
#define INBUF_ARRAY_INDEX_SHIFT  (3)
#define INBUF_BIT_WIDTH         (1<<(INBUF_ARRAY_INDEX_SHIFT))
#define SYNC_WORD         (OMX_S32)0x7ff

#define NUMBER_TEST_CASES  20
#define PLAYBACK_TESTCASE  100

#define module(x, POW2)   (x&(POW2-1))

/* MPEG Header Definitions - ID Bit Values */

#define MPEG_1              0
#define MPEG_2              1
#define MPEG_2_5            2
#define INVALID_VERSION     -1

OMX_BOOL SilenceInsertionEnable = OMX_FALSE;

#define AAC_MONO_SILENCE_FRAME_SIZE 10
#define AAC_STEREO_SILENCE_FRAME_SIZE 11

static const OMX_U8 AAC_MONO_SILENCE_FRAME[]   = {0x01, 0x40, 0x20, 0x06, 0x4F, 0xDE, 0x02, 0x70, 0x0C, 0x1C};      // 10 bytes
static const OMX_U8 AAC_STEREO_SILENCE_FRAME[] = {0x21, 0x10, 0x05, 0x00, 0xA0, 0x19, 0x33, 0x87, 0xC0, 0x00, 0x7E}; // 11 bytes)


// The string to prepend to source filenames
#define SOURCENAME_PREPEND_STRING ""
#define SOURCENAME_PREPEND_WSTRING _STRLIT_WCHAR("")

// The string to prepend to output filenames
#define OUTPUTNAME_PREPEND_STRING " "
#define OUTPUTNAME_PREPEND_WSTRING _STRLIT_WCHAR("")

template<class DestructClass>
class LogAppenderDestructDealloc : public OsclDestructDealloc
{
    public:
        virtual void destruct_and_dealloc(OsclAny *ptr)
        {
            delete((DestructClass*)ptr);
        }
};

void getCmdLineArg(bool cmdline_iswchar, cmd_line* command_line, char* argstr, int arg_index, int num_elements)
{
    if (cmdline_iswchar)
    {
        oscl_wchar* argwstr = NULL;
        command_line->get_arg(arg_index, argwstr);
        oscl_UnicodeToUTF8(argwstr, oscl_strlen(argwstr), argstr, 128);
        argstr[127] = '\0';
    }
    else
    {
        char* tmpstr = NULL;
        command_line->get_arg(arg_index, tmpstr);
        int32 tmpstrlen = oscl_strlen(tmpstr) + 1;
        if (tmpstrlen > 128)
        {
            tmpstrlen = 128;
        }
        oscl_strncpy(argstr, tmpstr, tmpstrlen);
        argstr[tmpstrlen-1] = '\0';
    }
}

void usage(FILE* filehandle)
{
    fprintf(filehandle, "Usage: Input_file {options}\n");
    fprintf(filehandle, "Option{} \n");
    fprintf(filehandle, "-output {console out}: Output file for console messages - Default is Stderr\n");
    fprintf(filehandle, "-o      {fileout}: Output file to be generated \n");
    fprintf(filehandle, "-i      {input_file2}: 2nd input file for dynamic reconfig, etc.\n");
    fprintf(filehandle, "-r      {ref_file}:   Reference file for output verification\n");
    fprintf(filehandle, "-c/-n   {CodecType/ComponentName}\n");
    fprintf(filehandle, "        CodecType could be avc, aac, mpeg4, h263, wmv, rv, amr, mp3, wma, ra\n");
    fprintf(filehandle, "-t x y  {A range of test cases to run}\n ");
    fprintf(filehandle, "        To run one test case use same index for x and y - Default is ALL\n");
    fprintf(filehandle, "-s      {Sampling Freq} Sampling Frquecny in Hz for input file\n");
    fprintf(filehandle, "-f      {AMR Frame Format} AMR Frame Format\n");
    fprintf(filehandle, "-b      {AMR Band Mode} AMR band mode\n");
    fprintf(filehandle, "-p      {Playback} Execute playback test\n");
    fprintf(filehandle, "-l      log to file\n");
}


// local_main entry point for the code
int local_main(FILE* filehandle, cmd_line* command_line)
{
    OSCL_UNUSED_ARG(filehandle);

    FILE* pInputFile = NULL;
    FILE* pOutputFile = NULL;

    char InFileName[200] = "\0";
    char InFileName2[200] = "\0";
    char OutFileName[200] = "\0";
    char pRefFileName[200] = "\0";
    char ComponentFormat[10] = "\0";

    OMX_STRING ComponentName = NULL, Role = NULL;
    OMX_S32 ArgIndex = 0, FirstTest, LastTest;
    OMX_U32 Channels = 2;
    OMX_U32 PCMSamplingRate = 0;
    LastTest = -1;

    //File format and band mode for Without marker test case, to be specified by user as ip argument
    OMX_AUDIO_AMRFRAMEFORMATTYPE AmrInputFileType;
    OMX_AUDIO_AMRBANDMODETYPE AMRFileBandMode;

    OMX_BOOL AmrFormatFlag = OMX_FALSE;
    OMX_BOOL AmrModeFlag = OMX_FALSE;

    //Whether log commands should go in a file
    OMX_BOOL IsLogFile = OMX_FALSE;

    OMX_U32 FileType = 0;
    OMX_U32 BandMode = 0;

    int result = 0;

    // OSCL Initializations
    OsclBase::Init();
    OsclErrorTrap::Init();

    OsclMutex mem_lock_mutex; //create mutex to use as mem lock
    mem_lock_mutex.Create();

    OsclMem::Init();
    PVLogger::Init();

    bool cmdline_iswchar = command_line->is_wchar();

    int32 count = command_line->get_count();

    char argstr[128];

    if (count > 1)
    {
        ArgIndex = 0;
        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);

        oscl_strncpy(InFileName, argstr, oscl_strlen(argstr) + 1);
        pInputFile = fopen(InFileName, "rb");
        fprintf(filehandle, "Input File to be decoded: %s\n", InFileName);

        //default is to run all tests.
        FirstTest = 0;
        LastTest = NUMBER_TEST_CASES;

        ArgIndex = 1;

        while (ArgIndex < count)
        {
            getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);

            if ('-' == *(argstr))
            {
                switch (argstr[1])
                {
                    case 'o':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        oscl_strncpy(OutFileName, argstr, oscl_strlen(argstr) + 1);
                        ArgIndex++;
                        pOutputFile = fopen(OutFileName, "wb");
                    }
                    break;

                    case 'i':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        oscl_strncpy(InFileName2, argstr, oscl_strlen(argstr) + 1);
                        ArgIndex++;
                    }
                    break;

                    case 'r':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        oscl_strncpy(pRefFileName, argstr, oscl_strlen(argstr) + 1);
                        ArgIndex++;
                    }
                    break;

                    case 't':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        uint32 tmp; // use tmp to avoid type-punned warning whe calling PV_atoi
                        PV_atoi(argstr, 'd', tmp);
                        FirstTest = tmp;
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        PV_atoi(argstr, 'd', tmp);
                        LastTest = tmp;
                        ArgIndex++;
                    }
                    break;

                    //Below two input arguments are required in the WihtoutMarker bit test case
                    case 'f':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        if (0 == oscl_strcmp("rtp", argstr))
                        {
                            AmrInputFileType = OMX_AUDIO_AMRFrameFormatRTPPayload;
                        }
                        else if (0 == oscl_strcmp("fsf", argstr))
                        {
                            AmrInputFileType = OMX_AUDIO_AMRFrameFormatFSF;
                        }
                        else if (0 == oscl_strcmp("if2", argstr))
                        {
                            AmrInputFileType = OMX_AUDIO_AMRFrameFormatIF2;
                        }
                        else if (0 == oscl_strcmp("if1", argstr))
                        {
                            AmrInputFileType = OMX_AUDIO_AMRFrameFormatIF1;
                        }
                        else
                        {
                            /* Invalid input format type */
                            fprintf(filehandle, "Invalid AMR input format specified!\n");
                            return 1;
                        }

                        AmrFormatFlag = OMX_TRUE;
                        ArgIndex++;
                    }
                    break;

                    case 'b':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        if (0 == oscl_strcmp("nb", argstr))
                        {
                            //Narrow band Mode
                            AMRFileBandMode = OMX_AUDIO_AMRBandModeNB0;
                        }
                        else if (0 == oscl_strcmp("wb", argstr))
                        {
                            //Wide band Mode
                            AMRFileBandMode = OMX_AUDIO_AMRBandModeWB0;
                        }
                        else
                        {
                            /* Invalid input format type */
                            fprintf(filehandle, "Invalid AMR band mode specified!\n");
                            return 1;
                        }
                        AmrModeFlag = OMX_TRUE;
                        ArgIndex++;
                    }
                    break;

                    case 'n':
                    case 'c':
                    {
                        char* Result;
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        if ('n' == argstr[1])
                        {
                            ComponentName = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
                            getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                            oscl_strncpy(ComponentName, argstr, oscl_strlen(argstr) + 1);
                        }

                        Role = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));

                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "aac")))
                        {
                            oscl_strncpy(Role, "audio_decoder.aac", oscl_strlen("video_decoder.aac") + 1);
                            oscl_strncpy(ComponentFormat, "AAC", oscl_strlen("AAC") + 1);
                        }
                        else if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "wmv")))
                        {
                            oscl_strncpy(Role, "video_decoder.wmv", oscl_strlen("video_decoder.wmv") + 1);
                            oscl_strncpy(ComponentFormat, "WMV", oscl_strlen("WMV") + 1);
                        }
                        else if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "rv")))
                        {
                            oscl_strncpy(Role, "video_decoder.rv", oscl_strlen("video_decoder.rv") + 1);
                            oscl_strncpy(ComponentFormat, "RV", oscl_strlen("RV") + 1);
                        }
                        else if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "amr")))
                        {
                            oscl_strncpy(Role, "audio_decoder.amr", oscl_strlen("audio_decoder.amr") + 1);
                            oscl_strncpy(ComponentFormat, "AMR", oscl_strlen("AMR") + 1);
                        }
                        else if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "mp3")))
                        {
                            oscl_strncpy(Role, "audio_decoder.mp3", oscl_strlen("audio_decoder.mp3") + 1);
                            oscl_strncpy(ComponentFormat, "MP3", oscl_strlen("MP3") + 1);
                        }
                        else if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "wma")))
                        {
                            oscl_strncpy(Role, "audio_decoder.wma", oscl_strlen("audio_decoder.wma") + 1);
                            oscl_strncpy(ComponentFormat, "WMA", oscl_strlen("WMA") + 1);
                        }
                        else if (NULL != (Result = (OMX_STRING) oscl_strstr(argstr, "ra")))
                        {
                            oscl_strncpy(Role, "audio_decoder.ra", oscl_strlen("audio_decoder.ra") + 1);
                            oscl_strncpy(ComponentFormat, "RA", oscl_strlen("RA") + 1);
                        }
                        else
                        {
                            fprintf(filehandle, "Unsupported component type\n");
                            return 1;
                        }

                        ArgIndex++;
                    }
                    break;

                    //Same case body
                    case 'm':
                    case 'M':
                    {
                        ArgIndex++;
                        Channels = 1;
                    }
                    break;

                    //Same case body
                    case 'p':
                    case 'P':
                    {
                        ArgIndex++;
                        FirstTest = PLAYBACK_TESTCASE;
                        LastTest = PLAYBACK_TESTCASE;
                    }
                    break;

                    case 's':
                    case 'S':
                    {
                        ArgIndex++;
                        getCmdLineArg(cmdline_iswchar, command_line, argstr, ArgIndex, PV_ARGSTR_LENGTH);
                        uint32 samplingRate;
                        PV_atoi(argstr, 'd', samplingRate);
                        PCMSamplingRate = samplingRate;
                        ArgIndex++;
                    }
                    break;

                    case 'l':
                    {
                        ArgIndex++;
                        IsLogFile = OMX_TRUE;
                    }
                    break;

                    default:
                    {
                        usage(filehandle);

                        // Clean OSCL
                        PVLogger::Cleanup();
                        OsclErrorTrap::Cleanup();
                        OsclMem::Cleanup();
                        OsclBase::Cleanup();

                        return -1;
                    }
                    break;
                }
            }
            else
            {
                usage(filehandle);

                // Clean OSCL
                PVLogger::Cleanup();
                OsclErrorTrap::Cleanup();
                OsclMem::Cleanup();
                OsclBase::Cleanup();
                mem_lock_mutex.Close();
                return -1;
            }
        }
    }
    else
    {
        fprintf(filehandle, "\nArguments insufficient\n\n");
        usage(filehandle);

        return -1;
    }


    //Verify input file is specified
    if (NULL == pInputFile)
    {
        fprintf(filehandle, "Input file missing/corrupted, exit from here \n");

        if (ComponentName)
        {
            oscl_free(ComponentName);
        }

        if (Role)
        {
            oscl_free(Role);
        }

        // Clean OSCL
        PVLogger::Cleanup();
        OsclErrorTrap::Cleanup();
        OsclMem::Cleanup();
        OsclBase::Cleanup();
        mem_lock_mutex.Close();
        return -1;
    }

    //If no component specified, return from here
    if ((NULL == ComponentName) && (NULL == Role))
    {
        fprintf(filehandle, "User didn't specify any of the component to instantiate, Exit from here \n");
        // Clean OSCL
        PVLogger::Cleanup();
        OsclErrorTrap::Cleanup();
        OsclMem::Cleanup();
        OsclBase::Cleanup();
        mem_lock_mutex.Close();
        return -1;
    }

    //This scope operator will make sure that LogAppenderDestructDealloc is called before the Logger cleanup

    {
        //Enable the following code for logging
        PVLoggerAppender *appender = NULL;
        OsclRefCounter *refCounter = NULL;

        if (IsLogFile)
        {
            OSCL_wHeapString<OsclMemAllocator> LogFileName(OUTPUTNAME_PREPEND_WSTRING);
            LogFileName += _STRLIT_WCHAR("logfile.txt");
            appender = (PVLoggerAppender*)TextFileAppender<TimeAndIdLayout, 1024>::CreateAppender(LogFileName.get_str());
            OsclRefCounterSA<LogAppenderDestructDealloc<TextFileAppender<TimeAndIdLayout, 1024> > > *appenderRefCounter =
                new OsclRefCounterSA<LogAppenderDestructDealloc<TextFileAppender<TimeAndIdLayout, 1024> > >(appender);
            refCounter = appenderRefCounter;

            OsclSharedPtr<PVLoggerAppender> appenderPtr(appender, refCounter);

            //Log all the loggers
            PVLogger *rootnode = PVLogger::GetLoggerObject("");
            rootnode->AddAppender(appenderPtr);
            rootnode->SetLogLevel(PVLOGMSG_DEBUG);
        }
        if (0 == oscl_strcmp(ComponentFormat, "AMR"))
        {
            if (AMRFileBandMode)
               BandMode = (OMX_U32)AMRFileBandMode;
            else
               BandMode = (OMX_U32)OMX_AUDIO_AMRBandModeWB0;
            if (AmrFormatFlag)
              FileType = (OMX_U32)AmrInputFileType;
            else
               FileType = (OMX_U32)OMX_AUDIO_AMRFrameFormatFSF;
        }
        if (-1 == LastTest)
        {
            LastTest = NUMBER_TEST_CASES;
        }

        //create a test suite
        OmxDecTestSuite *pTestSuite = OSCL_NEW(OmxDecTestSuite, (filehandle, FirstTest, LastTest,
                                               InFileName, InFileName2,
                                               OutFileName, pRefFileName, ComponentName, Role,
                                               ComponentFormat, Channels, PCMSamplingRate, FileType, BandMode));

        const uint32 starttick = OsclTickCount::TickCount();   // get start time

        pTestSuite->run_test();     //Run the test

        double time = OsclTickCount::TicksToMsec(OsclTickCount::TickCount()) - OsclTickCount::TicksToMsec(starttick);
        fprintf(filehandle, "Total Execution time for file %s is : %f seconds", InFileName, time / 1000);

        //Create interpreter
        text_test_interpreter interp;

        //interpretating results and dumping them into a UnitTest_String
        _STRING rs = interp.interpretation(pTestSuite->last_result());

        //Print in filehandle
        fprintf(filehandle, "%s", rs.c_str());

        const test_result the_result = pTestSuite->last_result();
        //if the success count is different from the total test count then return 1,else 0
        result = (int)(the_result.success_count() != the_result.total_test_count());

        OSCL_DELETE(pTestSuite);

        if (ComponentName)
        {
            oscl_free(ComponentName);
        }

        if (Role)
        {
            oscl_free(Role);
        }

        if (pInputFile)
        {
            fclose(pInputFile);
        }

        if (pOutputFile)
        {
            fclose(pOutputFile);
        }
    }

    //Uninstall the scheduler
    OsclScheduler::Cleanup();

    // Clean OSCL
    PVLogger::Cleanup();
    OsclErrorTrap::Cleanup();
    OsclMem::Cleanup();
    OsclBase::Cleanup();
    mem_lock_mutex.Close();

    return result;
}


OmxDecTestSuite::OmxDecTestSuite(FILE *filehandle, const int32 &aFirstTest,
                                 const int32 &aLastTest,
                                 char aInFileName[], char aInFileName2[], char aOutFileName[],
                                 char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                 char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode)
        : iWrapper(NULL)
{
    iWrapper = OSCL_NEW(OmxDecTest_wrapper , (filehandle, aFirstTest, aLastTest, aInFileName, aInFileName2,
                        aOutFileName, aRefFileName, aName, aRole,
                        aFormat, aChannels, aPCMSamplingRate, aFileType, aBandMode));
    adopt_test_case(iWrapper);
}

OmxDecTest_wrapper::OmxDecTest_wrapper(FILE *filehandle, const int32 &aFirstTest,
                                       const int32 &aLastTest,
                                       char aInFileName[], char aInFileName2[], char aOutFileName[],
                                       char aRefFileName[], OMX_STRING aName, OMX_STRING aRole,
                                       char aFormat[], OMX_U32 aChannels, OMX_U32 aPCMSamplingRate, OMX_U32 aFileType, OMX_U32 aBandMode) :
        iTestApp(NULL),
        iCurrentTestNumber(0),
        iFirstTest(aFirstTest),
        iLastTest(aLastTest),
        iFilehandle(filehandle),
        iInitSchedulerFlag(OMX_FALSE),
        iName(aName),
        iRole(aRole),
        iNumberOfChannels(aChannels),
        iSamplingFrequency(aPCMSamplingRate),
        iFileType(aFileType),
        iBandMode(aBandMode),
        iTotalSuccess(0),
        iTotalError(0),
        iTotalFail(0)
{
    oscl_strncpy(iInFileName, aInFileName, oscl_strlen(aInFileName) + 1);
    oscl_strncpy(iInFileName2, aInFileName2, oscl_strlen(aInFileName2) + 1);
    oscl_strncpy(iOutFileName, aOutFileName, oscl_strlen(aOutFileName) + 1);
    oscl_strncpy(iRefFile, aRefFileName, oscl_strlen(aRefFileName) + 1);
    oscl_strncpy(iFormat, aFormat, oscl_strlen(aFormat) + 1);
}

void OmxDecTest_wrapper::test()
{
    //Run the tests
    FILE* pInputFile = NULL;
    FILE* pOutputFile = NULL;
    OMX_BOOL OutfileOpen = OMX_FALSE;

    iCurrentTestNumber = iFirstTest;
    while (iCurrentTestNumber <= iLastTest)
    {
        OutfileOpen = OMX_FALSE;
        // Shutdown PVLogger and scheduler before checking mem stats
        switch (iCurrentTestNumber)
        {
            case GET_ROLES_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: GET_ROLES_TEST \n", (int32)iCurrentTestNumber);
                iTestApp =  OSCL_NEW(OmxDecTestCompRole, (iFilehandle, pInputFile, pOutputFile,
                                     iOutFileName, iRefFile, iName, iRole,
                                     iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

           case BUFFER_NEGOTIATION_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: BUFFER_NEGOTIATION_TEST \n", (int32)iCurrentTestNumber);
                iTestApp = OSCL_NEW(OmxDecTestBufferNegotiation, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));

            }
            break;

           case INPUT_OUTPUT_BUFFER_BUSY_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: INPUT_OUTPUT_BUFFER_BUSY_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestBufferBusy, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case DYNAMIC_PORT_RECONFIG:
            {
                OMX_BOOL iCallbackFlag1, iCallbackFlag2;

                /* Run the testcase for FILE 1 */
                fprintf(iFilehandle, "\nStarting test %4d: DYNAMIC_PORT_RECONFIG for %s\n", (int32)iCurrentTestNumber, iInFileName);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestPortReconfig, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));

                if (OMX_FALSE == iInitSchedulerFlag)
                {
                    iTestApp->InitScheduler();
                    iInitSchedulerFlag = OMX_TRUE;
                }

                iTestApp->StartTestApp();

                iCallbackFlag1 = iTestApp->iPortSettingsFlag;

                OSCL_DELETE(iTestApp);
                iTestApp = NULL;

                fclose(pInputFile);
                pInputFile = NULL;

                if (pOutputFile)
                {
                    fclose(pOutputFile);
                    pOutputFile = NULL;
                }


                /* Run the test case for FILE 2 */
                if (0 != oscl_strcmp(iInFileName2, "\0"))
                {
                    pInputFile = fopen(iInFileName2, "rb");
                }

                if (NULL == pInputFile)
                {
                    fprintf(iFilehandle, "Cannot run this test for second input bitstream File Open Error\n");
                    fprintf(iFilehandle, "DYNAMIC_PORT_RECONFIG Fail\n");
                    break;
                }

                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }
                fprintf(iFilehandle, "\nStarting test %4d: DYNAMIC_PORT_RECONFIG for %s\n", (int32)iCurrentTestNumber, iInFileName2);

                iTestApp = OSCL_NEW(OmxDecTestPortReconfig, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));

                iTestApp->StartTestApp();

                iCallbackFlag2 = iTestApp->iPortSettingsFlag;
                OSCL_DELETE(iTestApp);
                iTestApp = NULL;

                fclose(pInputFile);
                pInputFile = NULL;

                if (pOutputFile)
                {
                    fclose(pOutputFile);
                    pOutputFile = NULL;
                }


                /*Verify the test case */
                if ((OMX_TRUE == iCallbackFlag1) || (OMX_TRUE == iCallbackFlag2))
                {
                    fprintf(iFilehandle, "\nDYNAMIC_PORT_RECONFIG Success\n");
                }
                else
                {
                    fprintf(iFilehandle, "\n No Port Settings Changed Callback arrived for either of the input bit-streams\n");
                    fprintf(iFilehandle, "DYNAMIC_PORT_RECONFIG Fail\n");
                }
            }
            break;

            case FLUSH_PORT_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: FLUSH_PORT_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                    OutfileOpen = OMX_TRUE;
                }

                iTestApp = OSCL_NEW(OmxDecTestFlushPort, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case MULTIPLE_INSTANCE_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: MULTIPLE_INSTANCE_TEST \n", (int32)iCurrentTestNumber);

                iTestApp = OSCL_NEW(OmxDecTestMultipleInstance, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;


            case EOS_AFTER_FLUSH_PORT_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: EOS_AFTER_FLUSH_PORT_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                    OutfileOpen = OMX_TRUE;
                }

                iTestApp = OSCL_NEW(OmxDecTestEosAfterFlushPort, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case ENDOFSTREAM_MISSING_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: ENDOFSTREAM_MISSING_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestEosMissing, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case PARTIAL_FRAMES_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: PARTIAL_FRAMES_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestPartialFrames, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case EXTRA_PARTIAL_FRAMES_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: EXTRA_PARTIAL_FRAMES_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestExtraPartialFrames, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;
            case PLAYBACK_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: PLAYBACK_TEST \n");

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestPlayback, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case PAUSE_RESUME_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: PAUSE_RESUME_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestPauseResume, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case PORT_RECONFIG_TRANSITION_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: PORT_RECONFIG_TRANSITION_TEST\n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                    OutfileOpen = OMX_TRUE;
                }

                iTestApp = OSCL_NEW(OmxDecTestPortReconfigTransitionTest, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case PORT_RECONFIG_TRANSITION_TEST_2:
            {
                fprintf(iFilehandle, "\nStarting test %4d: PORT_RECONFIG_TRANSITION_TEST_2\n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                    OutfileOpen = OMX_TRUE;
                }

                iTestApp = OSCL_NEW(OmxDecTestPortReconfigTransitionTest_2, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case PORT_RECONFIG_TRANSITION_TEST_3:
            {
                fprintf(iFilehandle, "\nStarting test %4d: PORT_RECONFIG_TRANSITION_TEST_3\n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                    OutfileOpen = OMX_TRUE;
                }

                iTestApp = OSCL_NEW(OmxDecTestPortReconfigTransitionTest_3, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case REPOSITIONING_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: REPOSITIONING_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestReposition, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            case NORMAL_SEQ_TEST:
            {
                fprintf(iFilehandle, "\nStarting test %4d: NORMAL_SEQ_TEST \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxComponentDecTest, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;


            case NORMAL_SEQ_TEST_USEBUFF:
            {
                fprintf(iFilehandle, "\nStarting test %4d: NORMAL_SEQ_TEST_USEBUFF \n", (int32)iCurrentTestNumber);

                pInputFile = fopen(iInFileName, "rb");
                if (0 != oscl_strcmp(iOutFileName, "\0"))
                {
                    pOutputFile = fopen(iOutFileName, "wb");
                }

                iTestApp = OSCL_NEW(OmxDecTestUseBuffer, (iFilehandle, pInputFile, pOutputFile,
                                    iOutFileName, iRefFile, iName, iRole,
                                    iFormat, iNumberOfChannels, iSamplingFrequency, iFileType, iBandMode, this));
            }
            break;

            default:
            {
                // don't do anything here
            }
            break;
        }


        if (iTestApp != NULL)
        {
            if (OMX_FALSE == iInitSchedulerFlag)
            {
                iTestApp->InitScheduler();
                iInitSchedulerFlag = OMX_TRUE;
            }

            iTestApp->StartTestApp();
            OSCL_DELETE(iTestApp);

            iTestApp = NULL;
            // Go to next test
            iCurrentTestNumber++;

            if (pInputFile)
            {
                fclose(pInputFile);
                pInputFile = NULL;
            }

            if (OMX_TRUE == OutfileOpen)
            {
                fclose(pOutputFile);
                pOutputFile = NULL;
            }
        }
        else
        {
            ++iCurrentTestNumber;
        }
    }
}


void OmxDecTest_wrapper::TestCompleted()
{
    // Print out the result for this test case
    const test_result the_result = this->last_result();
    fprintf(iFilehandle, "Results for Test Case %d:\n", iCurrentTestNumber);
    fprintf(iFilehandle, "  Successes %d, Failures %d\n"
            , the_result.success_count() - iTotalSuccess, the_result.failures().size() - iTotalFail);
    iTotalSuccess = the_result.success_count();
    iTotalFail = the_result.failures().size();
    iTotalError = the_result.errors().size();
}

OMX_ERRORTYPE OmxComponentDecTest::GetInput()
{
    OMX_U32 Index;
    OMX_S32 ReadCount;
    OMX_S32 Size;
    OMX_ERRORTYPE Status;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInput() - IN"));

    if (OMX_TRUE == iInputReady)
    {
        if (OMX_TRUE == iInputWasRefused)
        {
            Index = iInIndex; //use previously found and saved Index, do not read the file (this was done earlier)
            iInputWasRefused = OMX_FALSE; // reset the flag
        }
        else
        {
            Index = 0;
            while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
            {
                Index++;
            }

            if (iInBufferCount == Index)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInput() - Input buffer not available, OUT"));
                return OMX_ErrorNone;
            }

            if (!iFragmentInProgress)
            {
                iCurFrag = 0;
                Size = iInBufferSize;
                for (OMX_S32 kk = 0; kk < iNumSimFrags - 1; kk++)
                {
                    iSimFragSize[kk] = Size / iNumSimFrags;
                }
                iSimFragSize[iNumSimFrags-1] = Size - (iNumSimFrags - 1) * (Size / iNumSimFrags); // last fragment
            }

            Size = iSimFragSize[iCurFrag];
            iCurFrag++;
            if (iCurFrag == iNumSimFrags)
            {
                iFragmentInProgress = OMX_FALSE;
            }
            else
            {
                iFragmentInProgress = OMX_TRUE;
            }

            ReadCount = fread(ipInBuffer[Index]->pBuffer, 1, Size, ipInputFile);
            if (0 == ReadCount)
            {
                iStopProcessingInput = OMX_TRUE;
                iInIndex = Index;
                iLastInputFrame = iFrameNum;
                iFragmentInProgress = OMX_FALSE;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInput() - Input file has ended, OUT"));
                return OMX_ErrorNone;
            }
            else if (ReadCount < Size)
            {
                iStopProcessingInput = OMX_TRUE;
                iInIndex = Index;
                iLastInputFrame = iFrameNum;
                iFragmentInProgress = OMX_FALSE;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInput() - Last piece of data in input file"));
                Size = ReadCount;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInput() - Input data of %d bytes read from the file", Size));

            if (iFragmentInProgress)
            {
                ipInBuffer[Index]->nFlags  = 0;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInput() - Input buffer of index %d without any marker", Index));
            }
            else
            {
                iFrameNum++;
            }

            ipInBuffer[Index]->nFilledLen = Size;
        }

        ipInBuffer[Index]->nOffset = 0;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "OmxComponentDecTest::GetInput() - Calling EmptyThisBuffer with Alloc Len %d, Filled Len %d Offset %d TimeStamp %d",
                         ipInBuffer[Index]->nAllocLen, ipInBuffer[Index]->nFilledLen, ipInBuffer[Index]->nOffset, ipInBuffer[Index]->nTimeStamp));

        Status = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);

        if (OMX_ErrorNone == Status)
        {
            ipInputAvail[Index] = OMX_FALSE; // mark unavailable

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::GetInput() - Sent the input buffer sucessfully, OUT"));
            return OMX_ErrorNone;
        }
        else if (OMX_ErrorInsufficientResources == Status)
        {
            iInIndex = Index; // save the current input buffer, wait for call back
            iInputReady = OMX_FALSE;
            iInputWasRefused = OMX_TRUE;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInput() - EmptyThisBuffer failed with Error OMX_ErrorInsufficientResources, OUT"));
            return OMX_ErrorInsufficientResources;
        }
        else
        {
            iStopProcessingInput = OMX_TRUE;
            iLastInputFrame = iFrameNum;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInput() - Error, EmptyThisBuffer failed, OUT"));
            return OMX_ErrorNone;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInput() - Error, Input is not ready to be sent, OUT"));
        return OMX_ErrorInsufficientResources;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInput() - OUT"));
    return OMX_ErrorNone;
}


//Read the data from input file & pass it to the AAC component
OMX_ERRORTYPE OmxComponentDecTest::GetInputFrameAac()
{
    OMX_U32 Index;
    OMX_U32 Size;
    OMX_ERRORTYPE Status;
    OMX_U32 ReadCount;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInputFrameAac() - IN"));

    if (OMX_TRUE == iInputReady)
    {
        if (OMX_TRUE == iInputWasRefused)
        {
            Index = iInIndex; //use previously found and saved Index, do not read the file (this was done earlier)
            iInputWasRefused = OMX_FALSE; // reset the flag
        }
        else
        {
            Index = 0;
            while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
            {
                Index++;
            }

            if (iInBufferCount == Index)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameAac() - Input buffer not available, OUT"));
                return OMX_ErrorNone;
            }


            if (iHeaderSent)
            {
                if (!iFragmentInProgress)
                {
                    ReadCount = fread(&Size, 1, FRAME_SIZE_FIELD, ipInputFile); // read in 1st 4 bytes
                    iCurFrag = 0;

                    if (ReadCount < FRAME_SIZE_FIELD)
                    {
                        iStopProcessingInput = OMX_TRUE;
                        iInIndex = Index;
                        iLastInputFrame = iFrameNum;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - Input file has ended, OUT"));
                        return OMX_ErrorNone;
                    }

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameAac() - Next frame size is %d", Size));

                    ReadCount = fread(&iFrameTimeStamp, 1, FRAME_TIME_STAMP_FIELD, ipInputFile); // read in 2nd 4 bytes

                    if (ReadCount < FRAME_TIME_STAMP_FIELD)
                    {
                        iStopProcessingInput = OMX_TRUE;
                        iInIndex = Index;
                        iLastInputFrame = iFrameNum;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - Input file has ended, OUT"));
                        return OMX_ErrorNone;
                    }

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameAac() - Next frame time stamp is %d", iFrameTimeStamp));


                    for (OMX_S32 kk = 0; kk < iNumSimFrags - 1; kk++)
                    {
                        iSimFragSize[kk] = Size / iNumSimFrags;
                    }

                    iSimFragSize[iNumSimFrags - 1] = Size - (iNumSimFrags - 1) * (Size / iNumSimFrags); // last fragment

                }

                Size = iSimFragSize[iCurFrag];
                iCurFrag++;

                if (iCurFrag == iNumSimFrags)
                {
                    iFragmentInProgress = OMX_FALSE;
                }
                else
                {
                    iFragmentInProgress = OMX_TRUE;
                }

                ReadCount = fread(ipInBuffer[Index]->pBuffer, 1, Size, ipInputFile);

                if (ReadCount < Size)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - Input file read error, OUT"));

                    return OMX_ErrorNone;   // corrupted file !!
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAac() - Bytes read from input file %d", Size));

                ipInBuffer[Index]->nFilledLen = Size;
                ipInBuffer[Index]->nOffset = 0;  // for now all data starts at the beginning of the buffer

            }
            else
            {
                ReadCount = fread(&Size, 1, FRAME_SIZE_FIELD, ipInputFile); // read in 1st 4 bytes

                if (ReadCount < FRAME_SIZE_FIELD)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - Input file has ended, OUT"));
                    return OMX_ErrorNone;
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameAac() - Next frame size is %d", Size));

                ReadCount = fread(&iFrameTimeStamp, 1, FRAME_TIME_STAMP_FIELD, ipInputFile); // read in 2nd 4 bytes
                if (ReadCount < FRAME_TIME_STAMP_FIELD)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - Input file has ended, OUT"));
                    return OMX_ErrorNone;
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameAac() - Next frame time stamp is %d", iFrameTimeStamp));

                ReadCount = fread(ipInBuffer[Index]->pBuffer, 1, Size, ipInputFile);
                if (ReadCount < Size)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - Input file read error, OUT"));
                    return OMX_ErrorNone;   // corrupted file !!
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAac() - Bytes read from input file %d", Size));

                ipInBuffer[Index]->nFilledLen = Size;
                ipInBuffer[Index]->nOffset = 0;

                if (1 == iFrameNum)
                {
                    iHeaderSent = OMX_TRUE;
                }
            }

            if (iFragmentInProgress)
            {
                ipInBuffer[Index]->nFlags  = 0;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAac() - Input buffer of index %d without any marker", Index));
            }
            else
            {
                iFrameNum++;
                ipInBuffer[Index]->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAac() - Input buffer of index %d with OMX_BUFFERFLAG_ENDOFFRAME flag marked", Index));
            }

            //Testing Silence insertion at random places
            if (0 == (iFrameNum % 15) && (OMX_TRUE == SilenceInsertionEnable) && (1 == iNumSimFrags))
            {
                //Do silence insertion  // Stereo silence frame
                if (iNumberOfChannels > 1)
                {
                    Size = AAC_STEREO_SILENCE_FRAME_SIZE;
                    oscl_memcpy(ipInBuffer[Index]->pBuffer, &AAC_STEREO_SILENCE_FRAME, Size);
                    ipInBuffer[Index]->nFilledLen = AAC_STEREO_SILENCE_FRAME_SIZE;
                    ipInBuffer[Index]->nOffset = 0;
                }
                else
                {
                    Size = AAC_MONO_SILENCE_FRAME_SIZE;
                    oscl_memcpy(ipInBuffer[Index]->pBuffer, &AAC_MONO_SILENCE_FRAME, Size);
                    ipInBuffer[Index]->nFilledLen = AAC_MONO_SILENCE_FRAME_SIZE;
                    ipInBuffer[Index]->nOffset = 0;
                }
            }
        }


        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "OmxComponentDecTest::GetInputFrameAac() - Calling EmptyThisBuffer with Alloc Len %d, Filled Len %d Offset %d TimeStamp %d",
                         ipInBuffer[Index]->nAllocLen, ipInBuffer[Index]->nFilledLen, ipInBuffer[Index]->nOffset, ipInBuffer[Index]->nTimeStamp));

        ipInBuffer[Index]->nTimeStamp = (OMX_TICKS)iFrameTimeStamp;
        Status = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);
        if (OMX_ErrorNone == Status)
        {
            ipInputAvail[Index] = OMX_FALSE; // mark unavailable

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::GetInputFrameAac() - Sent the input buffer sucessfully, OUT"));
            return OMX_ErrorNone;
        }
        else if (OMX_ErrorInsufficientResources == Status)
        {
            iInIndex = Index; // save the current input buffer, wait for call back
            iInputReady = OMX_FALSE;
            iInputWasRefused = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - EmptyThisBuffer failed with Error OMX_ErrorInsufficientResources, OUT"));
            return OMX_ErrorInsufficientResources;
        }
        else
        {
            iStopProcessingInput = OMX_TRUE;
            iLastInputFrame = iFrameNum;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - ERROR, EmptyThisBuffer failed, OUT"));
            return OMX_ErrorNone;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAac() - ERROR, Input is not ready to be sent, OUT"));
        return OMX_ErrorInsufficientResources;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInputFrameAac() - OUT"));
    return OMX_ErrorNone;
}


//Read the data from input file & pass it to the AMR component
OMX_ERRORTYPE OmxComponentDecTest::GetInputFrameAmr()
{
    OMX_U32 Index;
    OMX_S32 Size;
    OMX_ERRORTYPE Status;
    OMX_S32 ReadCount;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInputFrameAmr() - IN"));

    if (OMX_TRUE == iInputReady)
    {
        if (OMX_TRUE == iInputWasRefused)
        {
            Index = iInIndex; //use previously found and saved Index, do not read the file (this was done earlier)
            iInputWasRefused = OMX_FALSE; // reset the flag
        }
        else
        {
            Index = 0;
            while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
            {
                Index++;
            }

            if (iInBufferCount == Index)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameAmr() - Input buffer not available, OUT"));
                return OMX_ErrorNone;
            }


            if (iHeaderSent)
            {
                if (!iFragmentInProgress)
                {
                    //First four bytes for the size of next frame
                    ReadCount = fread(&Size, 1, FRAME_SIZE_FIELD, ipInputFile); // read in 1st 4 bytes
                    iCurFrag = 0;

                    if (ReadCount < FRAME_SIZE_FIELD)
                    {
                        iStopProcessingInput = OMX_TRUE;
                        iInIndex = Index;
                        iLastInputFrame = iFrameNum;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAmr() - Input file has ended, OUT"));
                        return OMX_ErrorNone;
                    }

                    //Next four bytes for the timestamp of the frame
                    ReadCount = fread(&iFrameTimeStamp, 1, FRAME_SIZE_FIELD, ipInputFile);

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxComponentDecTest::GetInputFrameAmr() - Next frame size is %d and timestamp %d", Size, iFrameTimeStamp));

                    for (OMX_S32 kk = 0; kk < iNumSimFrags - 1; kk++)
                    {
                        iSimFragSize[kk] = Size / iNumSimFrags;
                    }

                    iSimFragSize[iNumSimFrags - 1] = Size - (iNumSimFrags - 1) * (Size / iNumSimFrags); // last fragment
                }

                Size = iSimFragSize[iCurFrag];
                iCurFrag++;

                if (iCurFrag == iNumSimFrags)
                {
                    iFragmentInProgress = OMX_FALSE;
                }
                else
                {
                    iFragmentInProgress = OMX_TRUE;
                }

                ReadCount = fread(ipInBuffer[Index]->pBuffer, 1, Size, ipInputFile);

                if (ReadCount < Size)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "OmxComponentDecTest::GetInputFrameAmr() - Input file read error, OUT"));

                    return OMX_ErrorNone;   // corrupted file !!
                }

                ipInBuffer[Index]->nFilledLen = Size;
                ipInBuffer[Index]->nOffset = 0;  // for now all data starts at the beginning of the buffer

            }
            else
            {
                ReadCount = fread(&Size, 1, FRAME_SIZE_FIELD, ipInputFile); // read in 1st 4 bytes
                if (ReadCount < FRAME_SIZE_FIELD)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "OmxComponentDecTest::GetInputFrameAmr() - Input file has ended, OUT"));

                    return OMX_ErrorNone;
                }

                //Next four bytes for the timestamp of the frame
                ReadCount = fread((OMX_S32*) & iFrameTimeStamp, 1, FRAME_SIZE_FIELD, ipInputFile);

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAmr() - Next frame size is %d and timestamp %d", Size, iFrameTimeStamp));


                ReadCount = fread(ipInBuffer[Index]->pBuffer, 1, Size, ipInputFile);
                if (ReadCount < Size)
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "OmxComponentDecTest::GetInputFrameAmr() - Input file read error, OUT"));
                    return OMX_ErrorNone;   // corrupted file !!
                }

                ipInBuffer[Index]->nFilledLen = Size;
                ipInBuffer[Index]->nOffset = 0;

                iHeaderSent = OMX_TRUE;
            }

            if (iFragmentInProgress)
            {
                ipInBuffer[Index]->nFlags  = 0;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAmr() - Input buffer of index %d without any marker", Index));
            }
            else
            {
                iFrameNum++;
                ipInBuffer[Index]->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameAmr() - Input buffer of index %d with OMX_BUFFERFLAG_ENDOFFRAME flag marked", Index));
            }
        }

        ipInBuffer[Index]->nTimeStamp = iFrameTimeStamp;
        Status = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);
        if (OMX_ErrorNone == Status)
        {
            ipInputAvail[Index] = OMX_FALSE; // mark unavailable

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::GetInputFrameAmr() - Sent the input buffer sucessfully, OUT"));
            return OMX_ErrorNone;
        }
        else if (OMX_ErrorInsufficientResources == Status)
        {
            iInIndex = Index; // save the current input buffer, wait for call back
            iInputReady = OMX_FALSE;
            iInputWasRefused = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAmr() - EmptyThisBuffer failed with Error OMX_ErrorInsufficientResources, OUT"));
            return OMX_ErrorInsufficientResources;
        }
        else
        {
            iStopProcessingInput = OMX_TRUE;
            iLastInputFrame = iFrameNum;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAmr() - ERROR, EmptyThisBuffer failed, OUT"));
            return OMX_ErrorNone;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameAmr() - ERROR, Input is not ready to be sent, OUT"));
        return OMX_ErrorInsufficientResources;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInputFrameAmr() - OUT"));

    return OMX_ErrorNone;
}
//Read the data from input file & pass it to the Mp3 component
OMX_ERRORTYPE OmxComponentDecTest::GetInputFrameMp3()
{
    OMX_U32 Index;
    OMX_S32 Size;
    OMX_ERRORTYPE Status;
    OMX_S32 FrameSize;

    OMX_S32 bytes_read;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInputFrameMp3() - IN"));

    if (OMX_TRUE == iInputReady)
    {
        if (OMX_TRUE == iInputWasRefused)
        {
            Index = iInIndex; //use previously found and saved Index, do not read the file (this was done earlier)
            iInputWasRefused = OMX_FALSE; // reset the flag
        }
        else
        {
            Index = 0;

            while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
            {
                Index++;
            }

            if (iInBufferCount == Index)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::GetInputFrameMp3() - Input buffer not available, OUT"));
                return OMX_ErrorNone;
            }

            if (iHeaderSent)
            {
                if (!iFragmentInProgress)
                {
                    do
                    {
                        bytes_read = ipMp3Bitstream->DecodeReadInput();
                        if (!bytes_read)
                        {
                            if (0 == ipMp3Bitstream->iInputBufferCurrentLength)
                            {
                                iStopProcessingInput = OMX_TRUE;
                                iInIndex = Index;
                                iLastInputFrame = iFrameNum;

                                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                                (0, "OmxComponentDecTest::GetInputFrameMp3() - ERROR, Frame boundaries can not be found, OUT"));
                                return OMX_ErrorNone;
                            }
                            else
                            {
                                FrameSize = ipMp3Bitstream->iInputBufferCurrentLength;
                                break;
                            }
                        }
                    }
                    while (ipMp3Bitstream->Mp3FrameSynchronization(&FrameSize));

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxComponentDecTest::GetInputFrameMp3() - Next frame size is %d", FrameSize));

                    Size = FrameSize;
                    iCurFrag = 0;
                    ipMp3Bitstream->iFragmentSizeRead = ipMp3Bitstream->iInputBufferUsedLength;
                    ipMp3Bitstream->iInputBufferUsedLength += FrameSize;

                    for (OMX_S32 kk = 0; kk < iNumSimFrags - 1; kk++)
                    {
                        iSimFragSize[kk] = Size / iNumSimFrags;
                    }

                    iSimFragSize[iNumSimFrags - 1] = Size - (iNumSimFrags - 1) * (Size / iNumSimFrags); // last fragment
                }

                Size = iSimFragSize[iCurFrag];
                iCurFrag++;

                if (iCurFrag == iNumSimFrags)
                {
                    iFragmentInProgress = OMX_FALSE;
                }
                else
                {
                    iFragmentInProgress = OMX_TRUE;
                }

                oscl_memcpy(ipInBuffer[Index]->pBuffer, &ipMp3Bitstream->ipBuffer[ipMp3Bitstream->iFragmentSizeRead], Size);
                ipInBuffer[Index]->nFilledLen = Size;
                ipInBuffer[Index]->nOffset = 0;  // for now all data starts at the beginning of the buffer

                ipMp3Bitstream->iFragmentSizeRead += Size;
            }
            else
        {
                do
                {
                    bytes_read = ipMp3Bitstream->DecodeReadInput();
                    if (!bytes_read)
                    {
                        if (0 == ipMp3Bitstream->iInputBufferCurrentLength)
                        {
                            iStopProcessingInput = OMX_TRUE;
                            iInIndex = Index;
                            iLastInputFrame = iFrameNum;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                            (0, "OmxComponentDecTest::GetInputFrameMp3() - ERROR, Frame boundaries can not be found, OUT"));

                            return OMX_ErrorNone;
                        }
                        else
                        {
                            FrameSize = ipMp3Bitstream->iInputBufferCurrentLength;
                            break;
                        }
                    }
                }
                while (ipMp3Bitstream->Mp3FrameSynchronization(&FrameSize));


                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameMp3() - Next frame size is %d", FrameSize));

                ipInBuffer[Index]->nFilledLen = FrameSize;
                ipInBuffer[Index]->nOffset = 0;  // for now all data starts at the beginning of the buffer
                oscl_memcpy(ipInBuffer[Index]->pBuffer, &ipMp3Bitstream->ipBuffer[ipMp3Bitstream->iInputBufferUsedLength], FrameSize);


                iHeaderSent = OMX_TRUE;
                ipMp3Bitstream->iInputBufferUsedLength += FrameSize;
            }

            if (iFragmentInProgress)
            {
                ipInBuffer[Index]->nFlags  = 0;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameMp3() - Input buffer of index %d without any marker", Index));
            }
            else
            {
                iFrameNum++;
                ipInBuffer[Index]->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::GetInputFrameMp3() - Input buffer of index %d with OMX_BUFFERFLAG_ENDOFFRAME flag marked", Index));

                if (ipMp3Bitstream->DecodeAdjustInput())
                {
                    iStopProcessingInput = OMX_TRUE;
                    iInIndex = Index;
                    iLastInputFrame = iFrameNum;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameMp3() - Input file has ended, OUT"));
                    return OMX_ErrorNone;
                }
            }
        }


        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "OmxComponentDecTest::GetInputFrameMp3() - Calling EmptyThisBuffer with Alloc Len %d, Filled Len %d Offset %d TimeStamp %d",
                         ipInBuffer[Index]->nAllocLen, ipInBuffer[Index]->nFilledLen, ipInBuffer[Index]->nOffset, ipInBuffer[Index]->nTimeStamp));



       Status = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);




        if (OMX_ErrorNone == Status)
        {
            ipInputAvail[Index] = OMX_FALSE; // mark unavailable

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::GetInputFrameMp3() - Sent the input buffer sucessfully, OUT"));
            return OMX_ErrorNone;
        }
        else if (OMX_ErrorInsufficientResources == Status)
        {
            iInIndex = Index; // save the current input buffer, wait for call back
            iInputReady = OMX_FALSE;
            iInputWasRefused = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameMp3() - EmptyThisBuffer failed with Error OMX_ErrorInsufficientResources, OUT"));
            return OMX_ErrorInsufficientResources;
        }
        else
        {
            iStopProcessingInput = OMX_TRUE;
            iLastInputFrame = iFrameNum;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameMp3() - ERROR, EmptyThisBuffer failed, OUT"));
            return OMX_ErrorNone;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::GetInputFrameMp3() - ERROR, Input is not ready to be sent, OUT"));
        return OMX_ErrorInsufficientResources;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::GetInputFrameMp3() - OUT"));
    return OMX_ErrorNone;
}


bool OmxComponentDecTest::WriteOutput(OMX_U8* aOutBuff, OMX_U32 aSize)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::WriteOutput() called, Num of bytes %d", aSize));
    OMX_U32 BytesWritten;
    if (ipOutputFile)
    {
        BytesWritten = fwrite(aOutBuff, sizeof(OMX_U8), aSize, ipOutputFile);
    }
    else
    {
        BytesWritten = aSize;
    }

    return (BytesWritten == aSize);
}


/*
 * Active Object class's Run () function
 * Control all the states of AO & sends openmax API's to the component
 */

void OmxComponentDecTest::Run()
{
    switch (iState)
    {
        case StateUnLoaded:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateUnLoaded IN"));

            OMX_ERRORTYPE Err;
            OMX_BOOL Status;

            if (!iCallbacks->initCallbacks())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::Run() - ERROR initCallbacks failed, OUT"));
                StopOnError();
                break;
            }

            ipAppPriv = (AppPrivateType*) oscl_malloc(sizeof(AppPrivateType));
            CHECK_MEM(ipAppPriv, "Component_Handle");
            ipAppPriv->Handle = NULL;


            //Allocate bitstream buffer for MP3 component
            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                ipMp3Bitstream = OSCL_NEW(Mp3BitstreamObject, (ipInputFile));
                CHECK_MEM(ipMp3Bitstream, "Bitstream_Buffer");
            }

            //This should be the first call to the component to load it.
            Err = OMX_MasterInit();
            CHECK_ERROR(Err, "OMX_MasterInit");
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::Run() - OMX_MasterInit done"));

            Status = PrepareComponent();

            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::Run() Error while loading component OUT"));
                iState = StateError;

                if (iInputParameters.inPtr)
                {
                    oscl_free(iInputParameters.inPtr);
                    iInputParameters.inPtr = NULL;
                }

                RunIfNotReady();
                break;
            }


#if PROXY_INTERFACE
            ipThreadSafeHandlerEventHandler = OSCL_NEW(OmxEventHandlerThreadSafeCallbackAO, (this, EVENT_HANDLER_QUEUE_DEPTH, "EventHandlerAO"));
            ipThreadSafeHandlerEmptyBufferDone = OSCL_NEW(OmxEmptyBufferDoneThreadSafeCallbackAO, (this, iInBufferCount, "EmptyBufferDoneAO"));
            ipThreadSafeHandlerFillBufferDone = OSCL_NEW(OmxFillBufferDoneThreadSafeCallbackAO, (this, iOutBufferCount, "FillBufferDoneAO"));

            if ((NULL == ipThreadSafeHandlerEventHandler) ||
                    (NULL == ipThreadSafeHandlerEmptyBufferDone) ||
                    (NULL == ipThreadSafeHandlerFillBufferDone))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::Run() - Error, ThreadSafe Callback Handler initialization failed, OUT"));

                iState = StateUnLoaded;
                OsclExecScheduler* sched = OsclExecScheduler::Current();
                sched->StopScheduler();
            }
#endif

            if (StateError != iState)
            {
                iState = StateLoaded;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateUnLoaded OUT, moving to next state"));
            }

            RunIfNotReady();
        }
        break;

        case StateLoaded:
        {
            OMX_ERRORTYPE Err;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateLoaded IN"));

            // allocate memory for ipInBuffer
            ipInBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iInBufferCount);
            CHECK_MEM(ipInBuffer, "InputBufferHeader");

            ipInputAvail = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iInBufferCount);
            CHECK_MEM(ipInputAvail, "InputBufferFlag");

            /* Initialize all the buffers to NULL */
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                ipInBuffer[ii] = NULL;
            }

            //allocate memory for output buffer
            ipOutBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iOutBufferCount);
            CHECK_MEM(ipOutBuffer, "OutputBuffer");

            ipOutReleased = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iOutBufferCount);
            CHECK_MEM(ipOutReleased, "OutputBufferFlag");

            /* Initialize all the buffers to NULL */
            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                ipOutBuffer[ii] = NULL;
            }

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
            CHECK_ERROR(Err, "SendCommand Loaded->Idle");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::Run() - Sent State Transition Command from Loaded->Idle"));

            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::Run() - Allocating %d input and %d output buffers", iInBufferCount, iOutBufferCount));

            //These calls are required because the control of in & out buffer should be with the testapp.
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipInBuffer[ii], iInputPortIndex, NULL, iInBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Input");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iInputPortIndex));

                ipInputAvail[ii] = OMX_TRUE;
                ipInBuffer[ii]->nInputPortIndex = iInputPortIndex;
            }

            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }

            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipOutBuffer[ii], iOutputPortIndex, NULL, iOutBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Output");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iOutputPortIndex));

                ipOutReleased[ii] = OMX_TRUE;

                ipOutBuffer[ii]->nOutputPortIndex = iOutputPortIndex;
                ipOutBuffer[ii]->nInputPortIndex = 0;
            }

            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateLoaded OUT, Moving to next state"));

        }
        break;

        case StateIdle:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateIdle IN"));

            OMX_ERRORTYPE Err = OMX_ErrorNone;

            /*Send an output buffer before dynamic reconfig */
            Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[0]);
            CHECK_ERROR(Err, "FillThisBuffer");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::Run() - FillThisBuffer command called for initiating dynamic port reconfiguration"));

            ipOutReleased[0] = OMX_FALSE;

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            CHECK_ERROR(Err, "SendCommand Idle->Executing");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::Run() - Sent State Transition Command from Idle->Executing"));

            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateIdle OUT"));
        }
        break;

        case StateDecodeHeader:
        {

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxComponentDecTest::Run() - StateDecodeHeader IN, Sending configuration input buffers to the component to start dynamic port reconfiguration"));

            if (!iFlagDecodeHeader)
            {
                (*this.*pGetInputFrame)();
                //For AAC component , send one more frame apart from the config frame, so that we can receive the callback
                if (0 == oscl_strcmp(iFormat, "AAC") || 0 == oscl_strcmp(iFormat, "AMR"))
                {
                    (*this.*pGetInputFrame)();
                }
                iFlagDecodeHeader = OMX_TRUE;

                //Proceed to executing state and if Port settings changed callback comes,
                //then do the dynamic port reconfiguration
                iState = StateExecuting;

                RunIfNotReady();
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateDecodeHeader OUT"));
        }
        break;

        case StateDisablePort:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateDisablePort IN"));

            if (!iDisableRun)
            {
                if (!iFlagDisablePort)
                {
                    Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandPortDisable, iOutputPortIndex, NULL);
                    CHECK_ERROR(Err, "SendCommand_PortDisable");

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxComponentDecTest::Run() - Sent Command for OMX_CommandPortDisable on port %d as a part of dynamic port reconfiguration", iOutputPortIndex));

                    iPendingCommands = 1;
                    iFlagDisablePort = OMX_TRUE;
                    RunIfNotReady();
                }
                else
                {
                    //Wait for all the buffers to be returned on output port before freeing them
                    //This wait is required because of the queueing delay in all the Callbacks
                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (OMX_FALSE == ipOutReleased[ii])
                        {
                            break;
                        }
                    }

                    if (ii != iOutBufferCount)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxComponentDecTest::Run() - Not all the output buffers returned by component yet, wait for it"));
                        RunIfNotReady();
                        break;
                    }

                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (ipOutBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Output_DynamicReconfig");
                            ipOutBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxComponentDecTest::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                        }
                    }

                    if (ipOutBuffer)
                    {
                        oscl_free(ipOutBuffer);
                        ipOutBuffer = NULL;
                    }

                    if (ipOutReleased)
                    {
                        oscl_free(ipOutReleased);
                        ipOutReleased = NULL;
                    }

                    if (StateError == iState)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "OmxComponentDecTest::Run() - Error occured in this state, StateDisablePort OUT"));
                        RunIfNotReady();
                        break;
                    }
                    iDisableRun = OMX_TRUE;
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateDisablePort OUT"));
        }
        break;

        case StateDynamicReconfig:
        {
            OMX_BOOL Status = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateDynamicReconfig IN"));

            Status = HandlePortReEnable();
            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::Run() - Error occured in this state, StateDynamicReconfig OUT"));
                iState = StateError;
                RunIfNotReady();
                break;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateDynamicReconfig OUT"));
        }
        break;

        case StateExecuting:
        {
            OMX_U32 Index;
            OMX_BOOL MoreOutput;
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateExecuting IN"));

            MoreOutput = OMX_TRUE;
            while (MoreOutput)
            {
                Index = 0;
                while (OMX_FALSE == ipOutReleased[Index] && Index < iOutBufferCount)
                {
                    Index++;
                }

                if (Index != iOutBufferCount)
                {
                    //This call is being made only once per frame
                    Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[Index]);
                    CHECK_ERROR(Err, "FillThisBuffer");
                    //Make this flag OMX_TRUE till u receive the callback for output buffer free
                    ipOutReleased[Index] = OMX_FALSE;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxComponentDecTest::Run() - FillThisBuffer command called for output buffer index %d", Index));
                }
                else
                {
                    MoreOutput = OMX_FALSE;
                }
            }


            if (!iStopProcessingInput || (OMX_ErrorInsufficientResources == iStatusExecuting))
            {
                // find available input buffer
                Index = 0;
                while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
                {
                    Index++;
                }

                if (Index != iInBufferCount)
                {
                    iStatusExecuting = (*this.*pGetInputFrame)();
                }
            }
            else if (OMX_FALSE == iEosFlagExecuting)
            {
                //Only send one successful dummy buffer with flag set to signal EOS
                Index = 0;
                while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
                {
                    Index++;
                }

                if (Index != iInBufferCount)
                {
                    ipInBuffer[Index]->nFlags |= OMX_BUFFERFLAG_EOS;
                    ipInBuffer[Index]->nFilledLen = 0;
                    Err = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);

                    CHECK_ERROR(Err, "EmptyThisBuffer_EOS");

                    ipInputAvail[Index] = OMX_FALSE; // mark unavailable
                    iEosFlagExecuting = OMX_TRUE;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxComponentDecTest::Run() - Input buffer sent to the component with OMX_BUFFERFLAG_EOS flag set"));
                }
            }
            else
            {
                //nothing to do here
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateExecuting OUT"));
            RunIfNotReady();
        }
        break;

        case StateStopping:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateStopping IN"));

            //stop execution by state transition to Idle state.
            if (!iFlagStopping)
            {
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
                CHECK_ERROR(Err, "SendCommand Executing->Idle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::Run() - Sent State Transition Command from Executing->Idle"));

                iPendingCommands = 1;
                iFlagStopping = OMX_TRUE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateStopping OUT"));
        }
        break;

        case StateCleanUp:
        {
            OMX_U32 ii;
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateCleanUp IN"));


            if (!iFlagCleanUp)
            {
                if (OMX_FALSE == VerifyAllBuffersReturned())
                {
                    // not all buffers have been returned yet, reschedule
                    RunIfNotReady();
                    break;
                }

                //Destroy the component by state transition to Loaded state
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
                CHECK_ERROR(Err, "SendCommand Idle->Loaded");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxComponentDecTest::Run() - Sent State Transition Command from Idle->Loaded"));

                iPendingCommands = 1;

                if (ipInBuffer)
                {
                    for (ii = 0; ii < iInBufferCount; ii++)
                    {
                        if (ipInBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iInputPortIndex, ipInBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Input");
                            ipInBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxComponentDecTest::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                        }
                    }

                    oscl_free(ipInBuffer);
                    ipInBuffer = NULL;
                }

                if (ipInputAvail)
                {
                    oscl_free(ipInputAvail);
                    ipInputAvail = NULL;
                }


                if (ipOutBuffer)
                {
                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (ipOutBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Output");
                            ipOutBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxComponentDecTest::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                        }
                    }
                    oscl_free(ipOutBuffer);
                    ipOutBuffer = NULL;

                }

                if (ipOutReleased)
                {
                    oscl_free(ipOutReleased);
                    ipOutReleased = NULL;
                }

                iFlagCleanUp = OMX_TRUE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateCleanUp OUT"));

        }
        break;


        /********* FREE THE HANDLE & CLOSE FILES FOR THE COMPONENT ********/
        case StateStop:
        {
            OMX_U8 TestName[] = "NORMAL_SEQ_TEST";
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateStop IN"));

            if (ipAppPriv)
            {
                if (ipAppPriv->Handle)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxComponentDecTest::Run() - Free the Component Handle"));

                    Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                    if (OMX_ErrorNone != Err)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::Run() - FreeHandle Error"));
                        iTestStatus = OMX_FALSE;
                    }
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxComponentDecTest::Run() - De-initialize the omx component"));

            Err = OMX_MasterDeinit();
            if (OMX_ErrorNone != Err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::Run() - OMX_MasterDeinit Error"));
                iTestStatus = OMX_FALSE;
            }

            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                if (ipMp3Bitstream)
                {
                    OSCL_DELETE(ipMp3Bitstream);
                    ipMp3Bitstream = NULL;
                }
            }

            if (iOutputParameters)
            {
                oscl_free(iOutputParameters);
                iOutputParameters = NULL;
            }

            if (ipAppPriv)
            {
                oscl_free(ipAppPriv);
                ipAppPriv = NULL;
            }

#if PROXY_INTERFACE
            if (ipThreadSafeHandlerEventHandler)
            {
                OSCL_DELETE(ipThreadSafeHandlerEventHandler);
                ipThreadSafeHandlerEventHandler = NULL;
            }

            if (ipThreadSafeHandlerEmptyBufferDone)
            {
                OSCL_DELETE(ipThreadSafeHandlerEmptyBufferDone);
                ipThreadSafeHandlerEmptyBufferDone = NULL;
            }

            if (ipThreadSafeHandlerFillBufferDone)
            {
                OSCL_DELETE(ipThreadSafeHandlerFillBufferDone);
                ipThreadSafeHandlerFillBufferDone = NULL;
            }
#endif

            if (ipOutputFile)
            {
                VerifyOutput(TestName);
            }
            else
            {
                if (OMX_FALSE == iTestStatus)
                {
#ifdef PRINT_RESULT
                    fprintf(iConsOutFile, "%s: Fail \n", TestName);
                    OMX_DEC_TEST(false);
                    iTestCase->TestCompleted();
#endif
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                    (0, "OmxComponentDecTest::Run() - %s : Fail", TestName));
                }
                else
                {
#ifdef PRINT_RESULT
                    fprintf(iConsOutFile, "%s: Success {Output file not available} \n", TestName);
                    OMX_DEC_TEST(true);
                    iTestCase->TestCompleted();
#endif
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                    (0, "OmxComponentDecTest::Run() - %s : Success {Output file not available}", TestName));
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateStop OUT"));

            iState = StateUnLoaded;
            OsclExecScheduler* sched = OsclExecScheduler::Current();
            sched->StopScheduler();
        }
        break;

        case StateError:
        {
            //Do all the cleanup's and exit from here
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateError IN"));

            iTestStatus = OMX_FALSE;

            if (ipInBuffer)
            {
                for (ii = 0; ii < iInBufferCount; ii++)
                {
                    if (ipInBuffer[ii])
                    {
                        OMX_FreeBuffer(ipAppPriv->Handle, iInputPortIndex, ipInBuffer[ii]);
                        ipInBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxComponentDecTest::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                    }
                }
                oscl_free(ipInBuffer);
                ipInBuffer = NULL;
            }

            if (ipInputAvail)
            {
                oscl_free(ipInputAvail);
                ipInputAvail = NULL;
            }

            if (ipOutBuffer)
            {
                for (ii = 0; ii < iOutBufferCount; ii++)
                {
                    if (ipOutBuffer[ii])
                    {
                        OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                        ipOutBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxComponentDecTest::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                    }
                }
                oscl_free(ipOutBuffer);
                ipOutBuffer = NULL;
            }

            if (ipOutReleased)
            {
                oscl_free(ipOutReleased);
                ipOutReleased = NULL;
            }

            iState = StateStop;
            RunIfNotReady();

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::Run() - StateError OUT"));

        }
        break;

        default:
        {
            break;
        }
    }
    return ;
}


void OmxComponentDecTest::VerifyOutput(OMX_U8 aTestName[])
{
    FILE* pRefFilePointer = NULL;
    FILE* pOutFilePointer = NULL;
    OMX_S32 FileSize1, FileSize2;
    OMX_S16 Num1, Num2, Diff = 0;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::VerifyOutput() IN"));

    //Check the output against the reference file specified
    fclose(ipOutputFile);

    if (('\0' != iRefFile[0]) && (OMX_TRUE == iTestStatus))
    {
        pRefFilePointer = fopen(iRefFile, "rb");
        pOutFilePointer = fopen(iOutFileName, "rb");

        if (NULL != pRefFilePointer && NULL != pOutFilePointer)
        {
            fseek(pOutFilePointer, 0, SEEK_END);
            FileSize1 = ftell(pOutFilePointer);
            fseek(pOutFilePointer, 0, SEEK_SET);

            fseek(pRefFilePointer, 0, SEEK_END);
            FileSize2 = ftell(pRefFilePointer);
            fseek(pRefFilePointer, 0, SEEK_SET);

            if (FileSize1 == FileSize2)
            {
                while (!feof(pOutFilePointer))
                {
                    fread(&Num1, 1, sizeof(OMX_S16), pOutFilePointer);
                    fread(&Num2, 1, sizeof(OMX_S16), pRefFilePointer);

                    Diff = OSCL_ABS(Num2 - Num1);
                    if (0 != Diff)
                    {
                        break;
                    }
                }

                if (0 != Diff)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                    (0, "OmxComponentDecTest::VerifyOutput() - %s : Fail {Output not bit-exact}", aTestName));

#ifdef PRINT_RESULT
                    fprintf(iConsOutFile, "%s: Fail {Output not bit-exact}\n", aTestName);
                    OMX_DEC_TEST(false);
                    iTestCase->TestCompleted();
#endif
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                    (0, "OmxComponentDecTest::VerifyOutput() - %s : Success {Output Bit-exact}", aTestName));

#ifdef PRINT_RESULT
                    fprintf(iConsOutFile, "%s: Success {Output Bit-exact}\n", aTestName);
                    OMX_DEC_TEST(true);
                    iTestCase->TestCompleted();
#endif
                }

            }
            else
            {
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Fail {Output not bit-exact}\n", aTestName);
                OMX_DEC_TEST(false);
                iTestCase->TestCompleted();
#endif
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxComponentDecTest::VerifyOutput() - %s : Fail {Output not bit-exact}", aTestName));
            }
        }
        else
        {
#ifdef PRINT_RESULT
            fprintf(iConsOutFile, "Cannot open reference/output file for verification\n");
#endif
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "OmxComponentDecTest::VerifyOutput() - Cannot open reference/output file for verification"));

            if (OMX_FALSE == iTestStatus)
            {
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Fail \n", aTestName);
                OMX_DEC_TEST(false);
                iTestCase->TestCompleted();
#endif

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxComponentDecTest::VerifyOutput() - %s : Fail", aTestName));
            }
            else
            {
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Success {Ref file not available} \n", aTestName);
                OMX_DEC_TEST(true);
                iTestCase->TestCompleted();
#endif

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxComponentDecTest::VerifyOutput() - %s : Success {Ref file not available}", aTestName));
            }
        }
    }
    else
    {
        if ('\0' == iRefFile[0])
        {
#ifdef PRINT_RESULT
            fprintf(iConsOutFile, "Reference file not available\n");
#endif
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "OmxComponentDecTest::VerifyOutput() - Reference file not available"));
        }

        if (OMX_FALSE == iTestStatus)
        {
#ifdef PRINT_RESULT
            fprintf(iConsOutFile, "%s: Fail \n", aTestName);
            OMX_DEC_TEST(false);
            iTestCase->TestCompleted();
#endif
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "OmxComponentDecTest::VerifyOutput() - %s : Fail", aTestName));
        }
        else
        {
#ifdef PRINT_RESULT
            fprintf(iConsOutFile, "%s: Success {Ref file not available} \n", aTestName);
            OMX_DEC_TEST(true);
            iTestCase->TestCompleted();
#endif
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                            (0, "OmxComponentDecTest::VerifyOutput() - %s : Success {Ref file not available}", aTestName));
        }
    }

    if (pRefFilePointer)
    {
        fclose(pRefFilePointer);
    }
    if (pOutFilePointer)
    {
        fclose(pOutFilePointer);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::VerifyOutput() OUT"));
    return;
}

void OmxComponentDecTest::StopOnError()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::StopOnError() called"));
    iState = StateError;
    RunIfNotReady();
}



OMX_BOOL OmxComponentDecTest::PrepareComponent()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::PrepareComponent IN"));

    OMX_ERRORTYPE Err;
    iInputParameters.inPtr = NULL;
    iInputParameters.cComponentRole = NULL;
    iInputParameters.inBytes = 0;

    OMX_U32 ii;
    OMX_S32 ReadCount;
    OMX_S32 Size;
    OMX_S32 TempTimeStamp;
    OMX_BOOL Status = OMX_TRUE;


    if (0 == oscl_strcmp(iFormat, "AAC") || 0 == oscl_strcmp(iFormat, "AMR")
            || 0 == oscl_strcmp(iFormat, "MP3") || 0 == oscl_strcmp(iFormat, "WMA") || 0 == oscl_strcmp(iFormat, "RA"))
    {
        iInputParameters.cComponentRole = iRole;
        iOutputParameters = (AudioOMXConfigParserOutputs*) oscl_malloc(sizeof(AudioOMXConfigParserOutputs));
    }
    else
    {
        iInputParameters.cComponentRole = iRole;
        iOutputParameters = (VideoOMXConfigParserOutputs*) oscl_malloc(sizeof(VideoOMXConfigParserOutputs));
    }

    if (NULL == iOutputParameters)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxComponentDecTest::PrepareComponent() Can't allocate memory for OMXConfigParser output!"));
        return OMX_FALSE;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "OmxComponentDecTest::PrepareComponent() Initializing OMX component and decoder for role %s", iInputParameters.cComponentRole));


    //Do not parse the config header for dynamic port reconfig test cases, as here we want
    //port reconfiguration to happen
    if (OMX_FALSE == iDynamicPortReconfigTest)
    {
        if (0 == oscl_strcmp(iFormat, "AAC"))
        {
            ReadCount = fread(&Size, 1, FRAME_SIZE_FIELD, ipInputFile); // read in 1st 4 bytes
            if (ReadCount < FRAME_SIZE_FIELD)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - Input file error while reading config information OUT"));
                return OMX_FALSE;
            }

            ReadCount = fread(&TempTimeStamp, 1, FRAME_TIME_STAMP_FIELD, ipInputFile); // read in 2nd 4 bytes
            if (ReadCount < FRAME_TIME_STAMP_FIELD)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - Input file error while reading config information OUT"));
                return OMX_FALSE;
            }

            iInputParameters.inPtr = (uint8*) oscl_malloc(sizeof(uint8) * Size);
            if (NULL == iInputParameters.inPtr)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - Error allocating input config parser OUT"));
                return OMX_FALSE;
            }

            iInputParameters.inBytes = Size;

            ReadCount = fread(iInputParameters.inPtr, 1, Size, ipInputFile);
            if (ReadCount < Size)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - Input file error while reading config information OUT"));
                return OMX_FALSE;
            }


            fseek(ipInputFile, 0, SEEK_SET);
        }

        if (iInputParameters.inBytes == 0 || iInputParameters.inPtr == NULL)
        {
            // in case of following formats - config codec data is expected to
            // be present in the query. If not, config parser cannot be called

            if (0 == oscl_strcmp(iFormat, "AAC") ||
                    0 == oscl_strcmp(iFormat, "WMA") ||
                    0 == oscl_strcmp(iFormat, "M4V") ||
                    0 == oscl_strcmp(iFormat, "H264") ||
                    0 == oscl_strcmp(iFormat, "WMV")    ||
                    0 == oscl_strcmp(iFormat, "RV"))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::PrepareComponent() Codec Config data is not present"));

                return OMX_FALSE;
            }
        }
    }


    if (NULL != iName)
    {
        iInputParameters.cComponentName = iName;
        if (OMX_FALSE == iDynamicPortReconfigTest)
        {
            Status = OMX_MasterConfigParser(&iInputParameters, iOutputParameters);
        }

        if (OMX_TRUE == Status)
        {
            Err = OMX_MasterGetHandle(&ipAppPriv->Handle, iName, (OMX_PTR) this , iCallbacks->getCallbackStruct());
            if ((Err != OMX_ErrorNone) || (NULL == ipAppPriv->Handle))
            {
                return OMX_FALSE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareComponent() - Got Handle for the component %s", iName));

            OMX_BOOL Result = OMX_FALSE;

            if (0 == oscl_strcmp(iFormat, "AAC") || 0 == oscl_strcmp(iFormat, "AMR")
                    || 0 == oscl_strcmp(iFormat, "MP3") || 0 == oscl_strcmp(iFormat, "WMA") || 0 == oscl_strcmp(iFormat, "RA"))
            {
                Result = NegotiateComponentParametersAudio();

            }

            if (OMX_FALSE == Result)
            {
                // error, free handle and try to find a different component
                OMX_MasterFreeHandle(ipAppPriv->Handle);
                ipAppPriv->Handle = NULL;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareComponent() - Error in NegotiateComponentParameters"));
                return OMX_FALSE;
            }
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareComponent() - Error returned by OMX_MasterConfigParser API"));
            ipAppPriv->Handle = NULL;
            return OMX_FALSE;
        }

        // free Input Parameters, Output parameters mare required for some test case later as well
        if (iInputParameters.inPtr)
        {
            oscl_free(iInputParameters.inPtr);
            iInputParameters.inPtr = NULL;
        }
    }
    else if (NULL != iRole)
    {
        //Determine the component first & then get the handle
        OMX_U32 NumComps = 0;
        OMX_STRING* pCompOfRole;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareComponent() - Finding out the components that can support the role %s", iRole));

        // call once to find out the number of components that can fit the role
        Err = OMX_MasterGetComponentsOfRole(iRole, &NumComps, NULL);

        if (OMX_ErrorNone != Err || NumComps < 1)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - ERROR, No component can handle the specified role %s", iRole));
            ipAppPriv->Handle = NULL;

            return OMX_FALSE;
        }

        pCompOfRole = (OMX_STRING*) oscl_malloc(NumComps * sizeof(OMX_STRING));
        if (NULL == pCompOfRole)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - ERROR allocating memoy to pCompOfRole"));
            ipAppPriv->Handle = NULL;

            return OMX_FALSE;
        }

        for (ii = 0; ii < NumComps; ii++)
        {
            pCompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            if (NULL == pCompOfRole[ii])
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - ERROR allocating memoy to pCompOfRole for index %d", ii));
                ipAppPriv->Handle = NULL;

                oscl_free(pCompOfRole);
                pCompOfRole = NULL;
                return OMX_FALSE;
            }
        }

        // call 2nd time to get the component names
        Err = OMX_MasterGetComponentsOfRole(iRole, &NumComps, (OMX_U8**) pCompOfRole);
        if (OMX_ErrorNone != Err)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - ERROR returned by GetComponentsOfRole API"));
            return OMX_FALSE;
        }

        for (ii = 0; ii < NumComps; ii++)
        {
            iInputParameters.cComponentName = pCompOfRole[ii];

            if (OMX_FALSE == iDynamicPortReconfigTest)
            {
                Status = OMX_MasterConfigParser(&iInputParameters, iOutputParameters);
            }

            if (OMX_TRUE == Status)
            {
                // try to create component
                Err = OMX_MasterGetHandle(&ipAppPriv->Handle, (OMX_STRING) pCompOfRole[ii], (OMX_PTR) this, iCallbacks->getCallbackStruct());
                // if successful, no need to continue
                if ((OMX_ErrorNone == Err) && (NULL != ipAppPriv->Handle))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareComponent() - Got Handle for the component %s", pCompOfRole[ii]));
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareComponent() - ERROR, Cannot get component %s handle, try another if possible", pCompOfRole[ii]));
                    continue;
                }

                OMX_BOOL Result;

                if (0 == oscl_strcmp(iFormat, "AAC") || 0 == oscl_strcmp(iFormat, "AMR")
                        || 0 == oscl_strcmp(iFormat, "MP3") || 0 == oscl_strcmp(iFormat, "WMA") || 0 == oscl_strcmp(iFormat, "RA"))
                {
                    Result = NegotiateComponentParametersAudio();

                }
                if (OMX_FALSE == Result)
                {
                    // error, free handle and try to find a different component
                    OMX_MasterFreeHandle(ipAppPriv->Handle);
                    ipAppPriv->Handle = NULL;
                    continue;
                }

                // found a component, and it passed all tests. no need to continue.
                break;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareComponent() - Error returned by OMX_MasterConfigParser API"));
                ipAppPriv->Handle = NULL;
            }
        }

        // whether successful or not, need to free CompOfRoles
        for (ii = 0; ii < NumComps; ii++)
        {
            oscl_free(pCompOfRole[ii]);
            pCompOfRole[ii] = NULL;
        }
        oscl_free(pCompOfRole);
        pCompOfRole = NULL;

        // whether successful or not, need to free InputParameters
        if (iInputParameters.inPtr)
        {
            oscl_free(iInputParameters.inPtr);
            iInputParameters.inPtr = NULL;
        }

        // check if there was a problem
        if ((Err != OMX_ErrorNone) || (NULL == ipAppPriv->Handle))
        {
            return OMX_FALSE;
        }
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::PrepareComponent OUT"));

    return OMX_TRUE;
}


OMX_BOOL OmxComponentDecTest::NegotiateComponentParametersAudio()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::NegotiateComponentParametersAudio In"));

    OMX_ERRORTYPE Err;
    uint32 NumPorts;
    uint32 ii;

    AudioOMXConfigParserOutputs *pOutputParameters;
    pOutputParameters = (AudioOMXConfigParserOutputs *)iOutputParameters;

    if (OMX_FALSE == iDynamicPortReconfigTest)
    {
        if (0 == oscl_strcmp(iFormat, "WMA"))
        {
            iNumberOfChannels = pOutputParameters->Channels;
            iPCMSamplingRate = pOutputParameters->SamplesPerSec;
        }
        else if (0 == oscl_strcmp(iFormat, "AAC"))
        {
            iNumberOfChannels = pOutputParameters->Channels;
            iPCMSamplingRate = pOutputParameters->SamplesPerSec;
        }
    }

    //This will initialize the size and version of the iPortInit structure
    INIT_GETPARAMETER_STRUCT(OMX_PORT_PARAM_TYPE, iPortInit);

    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamAudioInit, &iPortInit);
    NumPorts = iPortInit.nPorts;

    if (Err != OMX_ErrorNone || NumPorts < 2)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() There is insuffucient (%d) ports", NumPorts));
        return OMX_FALSE;
    }

    // loop through ports starting from the starting index to find index of the first input port
    for (ii = iPortInit.nStartPortNumber ; ii < iPortInit.nStartPortNumber + NumPorts; ii++)
    {
        // get port parameters, and determine if it is input or output
        // if there are more than 2 ports, the first one we encounter that has input direction is picked
        //This will initialize the size and version of the iParamPort structure
        INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);

        //port
        iParamPort.nPortIndex = ii;

        Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);

        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() Problem negotiating with port %d ", ii));

            return OMX_FALSE;
        }

        if (0 == iParamPort.nBufferCountMin)
        {
            /* a buffer count of 0 is not allowed */
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() - Error, GetParameter for OMX_IndexParamPortDefinition returned 0 min buffer count"));
            return OMX_FALSE;
        }

        if (iParamPort.nBufferCountMin > iParamPort.nBufferCountActual)
        {
            /* Min buff count can't be more than actual buff count */
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() - ERROR, GetParameter for OMX_IndexParamPortDefinition returned actual buffer count %d less than min buffer count %d", iParamPort.nBufferCountActual, iParamPort.nBufferCountMin));
            return OMX_FALSE;
        }

        if (iParamPort.eDir == OMX_DirInput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() Found Input port index %d ", ii));

            iInputPortIndex = ii;
            break;
        }
    }

    if (ii == iPortInit.nStartPortNumber + NumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() Cannot find any input port "));
        return OMX_FALSE;
    }


    // loop through ports starting from the starting index to find index of the first output port
    for (ii = iPortInit.nStartPortNumber ; ii < iPortInit.nStartPortNumber + NumPorts; ii++)
    {
        // get port parameters, and determine if it is input or output
        // if there are more than 2 ports, the first one we encounter that has output direction is picked

        INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
        iParamPort.nPortIndex = ii;

        Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);

        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() Problem negotiating with port %d ", ii));

            return OMX_FALSE;
        }

        if (0 == iParamPort.nBufferCountMin)
        {
            /* a buffer count of 0 is not allowed */
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() - Error, GetParameter for OMX_IndexParamPortDefinition returned 0 min buffer count"));
            return OMX_FALSE;
        }

        if (iParamPort.nBufferCountMin > iParamPort.nBufferCountActual)
        {
            /* Min buff count can't be more than actual buff count */
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() - ERROR, GetParameter for OMX_IndexParamPortDefinition returned actual buffer count %d less than min buffer count %d", iParamPort.nBufferCountActual, iParamPort.nBufferCountMin));
            return OMX_FALSE;
        }

        if (iParamPort.eDir == OMX_DirOutput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() Found Output port index %d ", ii));

            iOutputPortIndex = ii;
            break;
        }
    }
    if (ii == iPortInit.nStartPortNumber + NumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "OmxComponentDecTest::NegotiateComponentParametersAudio() Cannot find any output port "));
        return OMX_FALSE;
    }


    // now get input parameters

    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);

    //Input port
    iParamPort.nPortIndex = iInputPortIndex;
    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem negotiating with input port %d ", iInputPortIndex));
        return OMX_FALSE;
    }

    // preset the number of input buffers
    iInBufferCount = iParamPort.nBufferCountActual;  // use the value provided by component
    iInBufferSize = iParamPort.nBufferSize;

    if (OMX_TRUE == iExtraPartialFrameTest)
    {
        iInBufferCount = EXTRAPARTIALFRAME_INPUT_BUFFERS;
    }

    iParamPort.nBufferCountActual = iInBufferCount;

    // set the number of actual input buffers
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Inport buffers %d,size %d", iInBufferCount, iInBufferSize));



    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iInputPortIndex;
    // finalize setting input port parameters
    Err = OMX_SetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem setting parameters in input port %d ", iInputPortIndex));
        return OMX_FALSE;
    }


    // in case of WMA - config parser decodes config info and produces reliable numchannels and sampling rate
    // set these values now to prevent unnecessary port reconfig
    if (OMX_FALSE == iDynamicPortReconfigTest)
    {
        if (0 == oscl_strcmp(iFormat, "WMA"))
        {
            OMX_AUDIO_PARAM_PCMMODETYPE Audio_Pcm_Param;

            // First get the structure
            INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_PCMMODETYPE, Audio_Pcm_Param);
            Audio_Pcm_Param.nPortIndex = iOutputPortIndex; // we're looking for output port params

            Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
            if (Err != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem negotiating PCM parameters with output port %d ", iOutputPortIndex));
                return OMX_FALSE;
            }

            // set the sampling rate obtained from config parser
            Audio_Pcm_Param.nSamplingRate = iPCMSamplingRate; // can be set to 0 (if unknown)

            // set number of channels obtained from config parser
            Audio_Pcm_Param.nChannels = iNumberOfChannels;     // should be 1 or 2

            // Now, set the parameters
            INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_PCMMODETYPE, Audio_Pcm_Param);
            Audio_Pcm_Param.nPortIndex = iOutputPortIndex;

            Err = OMX_SetParameter(ipAppPriv->Handle, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
            if (Err != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem Setting PCM parameters with output port %d ", iOutputPortIndex));
                return OMX_FALSE;
            }
        }
    }

    // Codec specific info set/get: SamplingRate, formats etc.
    if (!GetSetCodecSpecificInfo())
    {
        return OMX_FALSE;
    }

    //Port 1 for output port
    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;

    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem negotiating with output port %d ", iOutputPortIndex));
        return OMX_FALSE;
    }

    // set number of output buffers and the size
    iOutBufferCount = iParamPort.nBufferCountActual;
    iParamPort.nBufferCountActual = iOutBufferCount;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Outport buffers %d,size %d", iOutBufferCount, iOutBufferSize));

    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;
    // finalize setting output port parameters
    Err = OMX_SetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem setting parameters in output port %d ", iOutputPortIndex));
        return OMX_FALSE;
    }

    //Set input audio format
    //This is needed since a single component could handle differents roles

    // Init to desire format
    if (0 == oscl_strcmp(iFormat, "AAC"))
    {
        iAudioCompressionFormat = OMX_AUDIO_CodingAAC;
    }
    else if (0 == oscl_strcmp(iFormat, "AMR"))
    {
        iAudioCompressionFormat = OMX_AUDIO_CodingAMR;
    }
    else if (0 == oscl_strcmp(iFormat, "MP3"))
    {
        iAudioCompressionFormat = OMX_AUDIO_CodingMP3;
    }
    else if (0 == oscl_strcmp(iFormat, "WMA"))
    {
        iAudioCompressionFormat = OMX_AUDIO_CodingWMA;
    }
    else if (0 == oscl_strcmp(iFormat, "RA"))
    {
        iAudioCompressionFormat = OMX_AUDIO_CodingRA;
    }
    else
    {
        // Illegal codec specified.
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem setting audio compression format"));
        return OMX_FALSE;
    }


    OMX_AUDIO_PARAM_PORTFORMATTYPE AudioPortFormat;
    INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_PORTFORMATTYPE, AudioPortFormat);
    AudioPortFormat.nPortIndex = iInputPortIndex;

    // Search the proper format index and set it.
    // Since we already know that the component has the role we need, search until finding the proper nIndex
    // if component does not find the format will return OMX_ErrorNoMore

    for (ii = 0; ii < PVOMXAUDIO_MAX_SUPPORTED_FORMAT; ii++)
    {
        AudioPortFormat.nIndex = ii;
        Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamAudioPortFormat, &AudioPortFormat);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem setting audio compression format"));
            return OMX_FALSE;
        }
        if (iAudioCompressionFormat == AudioPortFormat.eEncoding)
        {
            break;
        }
    }

    if (ii == PVOMXAUDIO_MAX_SUPPORTED_FORMAT)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() No audio compression format found"));
        return OMX_FALSE;

    }
    // Now set the format to confirm parameters
    Err = OMX_SetParameter(ipAppPriv->Handle, OMX_IndexParamAudioPortFormat, &AudioPortFormat);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::NegotiateComponentParametersAudio() Problem setting audio compression format"));
        return OMX_FALSE;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::NegotiateComponentParametersAudio Out"));

    return OMX_TRUE;
}



OMX_BOOL OmxComponentDecTest::GetSetCodecSpecificInfo()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::GetSetCodecSpecificInfo IN"));

    OMX_PTR CodecProfilePtr = NULL;
    OMX_INDEXTYPE CodecProfileIndx = OMX_IndexAudioStartUnused;

    OMX_AUDIO_PARAM_AACPROFILETYPE Audio_Aac_Param;
    OMX_AUDIO_PARAM_AMRTYPE Audio_Amr_Param;
    OMX_AUDIO_PARAM_MP3TYPE Audio_Mp3_Param;
    OMX_AUDIO_PARAM_WMATYPE Audio_Wma_Param;
    OMX_AUDIO_PARAM_RATYPE Audio_Ra_Param;
    OMX_ERRORTYPE Err = OMX_ErrorNone;


    // determine the proper index and structure (based on codec type)
    if (0 == oscl_strcmp(iFormat, "AAC"))
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Aac_Param;
        CodecProfileIndx = OMX_IndexParamAudioAac;
        Audio_Aac_Param.nPortIndex = iInputPortIndex;

        INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_AACPROFILETYPE, Audio_Aac_Param);
        pGetInputFrame = &OmxComponentDecTest::GetInputFrameAac;
    }
    else if (0 == oscl_strcmp(iFormat, "AMR"))
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Amr_Param;
        CodecProfileIndx = OMX_IndexParamAudioAmr;
        Audio_Amr_Param.nPortIndex = iInputPortIndex;

        INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_AMRTYPE, Audio_Amr_Param);
        pGetInputFrame = &OmxComponentDecTest::GetInputFrameAmr;
    }
    else if (0 == oscl_strcmp(iFormat, "MP3"))
    {
        CodecProfilePtr = (OMX_PTR) & Audio_Mp3_Param;
        CodecProfileIndx = OMX_IndexParamAudioMp3;
        Audio_Mp3_Param.nPortIndex = iInputPortIndex;

        INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_MP3TYPE, Audio_Mp3_Param);
        pGetInputFrame = &OmxComponentDecTest::GetInputFrameMp3;
    }



    // first get parameters:
    Err = OMX_GetParameter(ipAppPriv->Handle, CodecProfileIndx, CodecProfilePtr);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::GetSetCodecSpecificInfo() Problem getting codec profile parameter on input port %d ", iInputPortIndex));
        return OMX_FALSE;
    }


    // Set the default stream format and output frame size except for AMR where we can determine from the clip
    if (0 == oscl_strcmp(iFormat, "AAC"))
    {
        Audio_Aac_Param.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4ADTS;
        iSamplesPerFrame = Audio_Aac_Param.nFrameLength;
    }
    else if (0 == oscl_strcmp(iFormat, "AMR"))
    {
        if (OMX_FALSE == iNoMarkerBitTest)
        {
            /* Determine the file format type for AMR file */

            OMX_S32 AmrFileByte = 0;
            fread(&AmrFileByte, 1, 4, ipInputFile); // read in 1st 4 bytes

            if (2 == AmrFileByte)
            {
                iAmrFileType = OMX_AUDIO_AMRFrameFormatFSF;
                iSamplesPerFrame = PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME;
            }
            else if (4 == AmrFileByte)
            {
                iAmrFileType = OMX_AUDIO_AMRFrameFormatIF2;
                iSamplesPerFrame = PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME;
            }
            else if (6 == AmrFileByte)
            {
                iAmrFileType = OMX_AUDIO_AMRFrameFormatRTPPayload;
                iSamplesPerFrame = PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME;
            }
            else if (0 == AmrFileByte)
            {
                iAmrFileType = OMX_AUDIO_AMRFrameFormatConformance;
                iSamplesPerFrame = PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME;
            }
            else if (7 == AmrFileByte)
            {
                iAmrFileType = OMX_AUDIO_AMRFrameFormatRTPPayload;
                iAmrFileMode = OMX_AUDIO_AMRBandModeWB0;
                iSamplesPerFrame = PVOMXAUDIODEC_AMRWB_SAMPLES_PER_FRAME;

            }
            else if (8 == AmrFileByte)
            {
                iAmrFileType = OMX_AUDIO_AMRFrameFormatFSF;
                iAmrFileMode = OMX_AUDIO_AMRBandModeWB0;
                iSamplesPerFrame = PVOMXAUDIODEC_AMRWB_SAMPLES_PER_FRAME;
            }
            else
            {
                /* Invalid input format type */
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::GetSetCodecSpecificInfo() - Error, Invalid AMR input format %d, OUT", AmrFileByte));
                return OMX_FALSE;
            }
        }

        Audio_Amr_Param.eAMRFrameFormat = iAmrFileType;
        Audio_Amr_Param.eAMRBandMode = iAmrFileMode;
    }
    else if (0 == oscl_strcmp(iFormat, "MP3"))
    {
        iSamplesPerFrame = PVOMXAUDIODEC_MP3_DEFAULT_SAMPLES_PER_FRAME;
    }
   
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::GetSetCodecSpecificInfo() Unknown format in input port negotiation "));
        return OMX_FALSE;
    }

    // set parameters to inform the component of the stream type
    Err = OMX_SetParameter(ipAppPriv->Handle, CodecProfileIndx, CodecProfilePtr);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::GetSetCodecSpecificInfo() Problem setting codec profile parameter on input port %d ", iInputPortIndex));
        return OMX_FALSE;
    }


    // GET the output buffer params and sizes
    OMX_AUDIO_PARAM_PCMMODETYPE Audio_Pcm_Param;
    Audio_Pcm_Param.nPortIndex = iOutputPortIndex; // we're looking for output port params

    INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_PCMMODETYPE, Audio_Pcm_Param);


    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamAudioPcm, &Audio_Pcm_Param);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::GetSetCodecSpecificInfo() Problem negotiating PCM parameters with output port %d ", iOutputPortIndex));
        return OMX_FALSE;
    }


    // these are some initial default values that may change
    //iPCMSamplingRate = Audio_Pcm_Param.nSamplingRate; // can be set to 0 (if unknown)

    if (iPCMSamplingRate == 0) // use default sampling rate (i.e. 48000)
    {
        iPCMSamplingRate = PVOMXAUDIODEC_DEFAULT_SAMPLINGRATE;
    }

    iNumberOfChannels = Audio_Pcm_Param.nChannels;     // should be 1 or 2
    if (iNumberOfChannels != 1 && iNumberOfChannels != 2)
    {
        return OMX_FALSE;
    }


    if ((iSamplesPerFrame != 0) && ((iSamplesPerFrame * 1000) > iPCMSamplingRate))
        // if this iSamplesPerFrame is known and is large enough to ensure that the iMilliSecPerFrame calculation
        // below won't be set to 0.
    {
        // CALCULATE NumBytes per frame, Msec per frame, etc.

        uint32 NumBytesPerFrame = 2 * iSamplesPerFrame * iNumberOfChannels;
        uint32 MilliSecPerFrame = (iSamplesPerFrame * 1000) / iPCMSamplingRate;
        // Determine the size of each PCM output buffer. Size would be big enough to hold certain time amount of PCM data
        uint32 Numframes = PVOMXAUDIODEC_DEFAULT_OUTPUTPCM_TIME / MilliSecPerFrame;

        if (PVOMXAUDIODEC_DEFAULT_OUTPUTPCM_TIME % MilliSecPerFrame)
        {
            // If there is a remainder, include one more frame
            ++Numframes;
        }
        // set the output buffer size accordingly:
        iOutBufferSize = Numframes * NumBytesPerFrame;
    }
    else
    {
        iOutBufferSize = (2 * iNumberOfChannels * PVOMXAUDIODEC_DEFAULT_OUTPUTPCM_TIME * iPCMSamplingRate) / 1000; // assuming 16 bits per sample
    }

    //Port 1 for output port
    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;

    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXAudioDecNode::GetSetCodecSpecificInfo() Problem negotiating with output port %d ", iOutputPortIndex));
        return OMX_FALSE;
    }

    if (iOutBufferSize < iParamPort.nBufferSize)
    {
        // the OMX spec says that nBuffersize is a read only field, but the client is allowed to allocate
        // a buffer size larger than nBufferSize.
        iOutBufferSize = iParamPort.nBufferSize;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::GetSetCodecSpecificInfo OUT"));

    return OMX_TRUE;
}



OMX_BOOL OmxComponentDecTest::HandlePortReEnable()
{
    OMX_ERRORTYPE Err = OMX_ErrorNone;
    OMX_U32 ii;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::HandlePortReEnable() IN"));

    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;

    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Error while getting parameters for index OMX_IndexParamPortDefinition"));
        return OMX_FALSE;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::HandlePortReEnable() - GetParameter called for OMX_IndexParamPortDefinition on port %d", iParamPort.nPortIndex));

    //For all the components, at the time of port reconfiguration take the size from the component
    iOutBufferSize = iParamPort.nBufferSize;

    //Check the number of output buffers after port reconfiguration
    iOutBufferCount = iParamPort.nBufferCountActual;

    // do we need to increase the number of buffers?
    if (iOutBufferCount < iParamPort.nBufferCountMin)
    {
        iOutBufferCount = iParamPort.nBufferCountMin;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxComponentDecTest::HandlePortReEnable() new output buffers %d, size %d", iOutBufferCount, iOutBufferSize));


    // make sure to set the actual number of buffers (in case the number has changed)
    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;
    iParamPort.nBufferCountActual = iOutBufferCount;

    Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandPortEnable, iOutputPortIndex, NULL);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Error while sendind OMX_CommandPortEnable command"));
        return OMX_FALSE;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "OmxComponentDecTest::HandlePortReEnable() - Sent Command for OMX_CommandPortEnable on port %d as a part of dynamic port reconfiguration", iOutputPortIndex));

    iPendingCommands = 1;

    //allocate memory for output buffer
    ipOutBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iOutBufferCount);
    if (NULL == ipOutBuffer)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Error allocating memory for output buffer while dynamic port reconfiguration"));
        return OMX_FALSE;
    }

    ipOutReleased = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iOutBufferCount);
    if (NULL == ipOutBuffer)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Error allocating memory for output buffer flag while dynamic port reconfiguration"));
        return OMX_FALSE;
    }

    /* Initialize all the buffers to NULL */
    for (ii = 0; ii < iOutBufferCount; ii++)
    {
        ipOutBuffer[ii] = NULL;
    }


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "OmxComponentDecTest::HandlePortReEnable() - Allocating buffer again after port reconfigutauion has been complete"));

    for (ii = 0; ii < iOutBufferCount; ii++)
    {
        Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipOutBuffer[ii], iOutputPortIndex, NULL, iOutBufferSize);
        if (Err != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFOMXVideoDecNode::HandlePortReEnable() Error allocating output buffer %d using OMX_AllocateBuffer", ii));
            return OMX_FALSE;
        }

        ipOutReleased[ii] = OMX_TRUE;
        ipOutBuffer[ii]->nOutputPortIndex = iOutputPortIndex;
        ipOutBuffer[ii]->nInputPortIndex = 0;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "OmxComponentDecTest::HandlePortReEnable() - AllocateBuffer called for buffer index %d on port %d", ii, iOutputPortIndex));
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxComponentDecTest::HandlePortReEnable() OUT"));

    return OMX_TRUE;
}

OMX_S32 AVCBitstreamObject::Refill()
{
    OMX_S32 RemainBytes = iActualSize - iPos;

    if (RemainBytes > 0)
    {
        // move the remaining stuff to the beginning of iBuffer
        oscl_memcpy(&ipBuffer[0], &ipBuffer[iPos], RemainBytes);
        iPos = 0;
    }

    // read data
    if ((iActualSize = fread(&ipBuffer[RemainBytes], 1, iBufferSize - RemainBytes, ipAVCFile)) == 0)
    {
        iActualSize += RemainBytes;
        iPos = 0;
        return -1;
    }

    iActualSize += RemainBytes;
    iPos = 0;
    return 0;
}


AVCOMX_Status AVCBitstreamObject::GetNextFullNAL(OMX_U8** aNalBuffer, OMX_S32* aNalSize)
{
    // First check if the remaining size of the buffer can hold one full NAL
    if (iActualSize - iPos < MIN_NAL_SIZE)
    {
        if (Refill())
        {
            return AVCOMX_FAIL;
        }
    }

    OMX_U8* pBuff = ipBuffer + iPos;

    *aNalSize = iActualSize - iPos;

    AVCOMX_Status RetVal = AVCAnnexBGetNALUnit(pBuff, aNalBuffer, (int32*) aNalSize);

    if (AVCOMX_FAIL == RetVal)
    {
        return AVCOMX_FAIL;
    }

    if (AVCOMX_NO_NEXT_SC == RetVal)
    {
        Refill();
        pBuff = ipBuffer + iPos;

        *aNalSize = iActualSize - iPos;
        RetVal = AVCAnnexBGetNALUnit(pBuff, aNalBuffer, (int32*) aNalSize);
        if (AVCOMX_FAIL == RetVal)
        {
            return AVCOMX_FAIL; // After refill(), the data should be enough
        }
    }

    iPos += ((*aNalSize) + (OMX_S32)(*aNalBuffer - pBuff));
    return AVCOMX_SUCCESS;
}

AVCOMX_Status AVCBitstreamObject::AVCAnnexBGetNALUnit(uint8 *bitstream, uint8 **nal_unit,
        int32 *size)
{
    int i, j, FoundStartCode = 0;
    int end;

    i = 0;
    while (bitstream[i] == 0 && i < *size)
    {
        i++;
    }
    if (i >= *size)
    {
        *nal_unit = bitstream;
        return AVCOMX_FAIL; /* cannot find any start_code_prefix. */
    }
    else if (bitstream[i] != 0x1)
    {
        i = -1;  /* start_code_prefix is not at the beginning, continue */
    }

    i++;
    *nal_unit = bitstream + i; /* point to the beginning of the NAL unit */

    j = end = i;
    while (!FoundStartCode)
    {
        while ((j + 1 < *size) && (bitstream[j] != 0 || bitstream[j+1] != 0))  /* see 2 consecutive zero bytes */
        {
            j++;
        }
        end = j;   /* stop and check for start code */
        while (j + 2 < *size && bitstream[j+2] == 0) /* keep reading for zero byte */
        {
            j++;
        }
        if (j + 2 >= *size)
        {
            *size -= i;
            return AVCOMX_NO_NEXT_SC;  /* cannot find the second start_code_prefix */
        }
        if (bitstream[j+2] == 0x1)
        {
            FoundStartCode = 1;
        }
        else
        {
            /* could be emulation code 0x3 */
            j += 2; /* continue the search */
        }
    }

    *size = end - i;

    return AVCOMX_SUCCESS;
}


void AVCBitstreamObject::ResetInputStream()
{
    iPos = 0;
    iActualSize = 0;
}


int32 Mp3BitstreamObject::DecodeReadInput()
{
    int32      possibleElements = 0;
    int32      actualElements;
    int32      start;

    start = iInputBufferCurrentLength;    

    if (iInputBufferMaxLength > start)
    {
        possibleElements = iInputBufferMaxLength - start;
    }
    else if (start)
    {
        /*
         *  Synch search need to expand the seek over fresh data
         *  then increase the data read according to teh size of the size
         *  of the frame plus 2 extra bytes to read next sync word
         */
        iInputBufferMaxLength = iCurrentFrameLength;printf(" 111   iInputBufferMaxLength changed to  = %d\n", iCurrentFrameLength);

        if (iInputBufferMaxLength > BIT_BUFF_SIZE_MP3)
        {
            iInputBufferMaxLength = BIT_BUFF_SIZE_MP3;
        }
        possibleElements = iInputBufferMaxLength - start;
    }

    actualElements = fread(&(ipBuffer[start]), sizeof(ipBuffer[0]),
                           possibleElements, ipMp3File);

    /*
     * Increment the number of valid array elements by the actual amount
     * read in.
     */

    if (!actualElements)
    {
        oscl_memset(&(ipBuffer[start]), 0,
                    possibleElements * sizeof(ipBuffer[0]));
    }

    iInputBufferCurrentLength +=  actualElements;

    return actualElements;
}


int32 Mp3BitstreamObject::DecodeAdjustInput()
{
    int32   start;
    int32   length;

    start = iInputBufferUsedLength;

    length = iInputBufferMaxLength - start;

    if (length > 0)
    {
        oscl_memmove(&(ipBuffer[0]),                 /* destination */
                     &(ipBuffer[start]),            /* source      */
                     length * sizeof(ipBuffer[0])); /* length      */
    }

    iInputBufferUsedLength = 0;
    iInputBufferCurrentLength -= start;

    if (iInputBufferCurrentLength < 0)
    {
        return 1;
    }

    return 0;
}


int32 Mp3BitstreamObject::Mp3FrameSynchronization(OMX_S32* aFrameSize)
{
    uint16 val;
    OMX_BOOL Err;
    int32 retval = 1;
    int32 numBytes;

    pVars->pBuffer = ipBuffer;
    pVars->usedBits = (iInputBufferUsedLength << 3); // in bits
    pVars->inputBufferCurrentLength = (iInputBufferCurrentLength);

    Err = HeaderSync();

    if (OMX_TRUE == Err)
    {
        /* validate synchronization by checking two consecutive sync words
         * to avoid multiple bitstream accesses*/

        uint32 temp = GetNbits(21);
        // put back whole header
        pVars->usedBits -= 21 + SYNC_WORD_LENGTH_MP3;

        int32  version;

        switch (temp >> 19)
        {
            case 0:
                version = MPEG_2_5;
                break;
            case 2:
                version = MPEG_2;
                break;
            case 3:
                version = MPEG_1;
                break;
            default:
                version = INVALID_VERSION;
                break;
        }

        int32 freq_index = (temp << 20) >> 30;

        if (version != INVALID_VERSION && (freq_index != 3))
        {
            numBytes = (int32)(((int64)(Mp3Bitrate[version][(temp<<16)>>28] << 20) * InvFreq[freq_index]) >> 28);

            numBytes >>= (20 - version);

            if (version != MPEG_1)
            {
                numBytes >>= 1;
            }
            if ((temp << 22) >> 31)
            {
                numBytes++;
            }

            if (numBytes > (int32)pVars->inputBufferCurrentLength)
            {
                /* frame should account for padding and 2 bytes to check sync */
                iCurrentFrameLength = numBytes + 3;
                return 1;
            }
            else if (numBytes == (int32)pVars->inputBufferCurrentLength)
            {
                /* No enough data to validate, but current frame appears to be correct ( EOF case) */
                iInputBufferUsedLength = pVars->usedBits >> 3;
                *aFrameSize = numBytes;
                return 0;
            }
            else
            {
                int32 offset = pVars->usedBits + ((numBytes) << 3);

                offset >>= INBUF_ARRAY_INDEX_SHIFT;
                uint8    *pElem  = pVars->pBuffer + offset;
                uint16 tmp1 = *(pElem++);
                uint16 tmp2 = *(pElem);

                val = (tmp1 << 3);
                val |= (tmp2 >> 5);
            }
        }
        else
        {
            val = 0; // force mismatch
        }

        if (val == SYNC_WORD)
        {
            iInputBufferUsedLength = pVars->usedBits >> 3; ///  !!!!!
            *aFrameSize = numBytes;
            retval = 0;
        }
        else
        {
            iInputBufferCurrentLength = 0;
            *aFrameSize = 0;
            retval = 1;
        }
    }
    else
    {
        iInputBufferCurrentLength = 0;
    }

    return(retval);

}


OMX_BOOL Mp3BitstreamObject::HeaderSync()
{
    uint16 val;
    uint32    offset;
    uint32    bitIndex;
    uint8     Elem;
    uint8     Elem1;
    uint8     Elem2;
    uint32   returnValue;
    uint32 availableBits = (pVars->inputBufferCurrentLength << 3); // in bits

    // byte aligment
    pVars->usedBits = (pVars->usedBits + 7) & 8;

    offset = (pVars->usedBits) >> INBUF_ARRAY_INDEX_SHIFT;

    Elem  = *(pVars->pBuffer + offset);
    Elem1 = *(pVars->pBuffer + offset + 1);
    Elem2 = *(pVars->pBuffer + offset + 2);


    returnValue = (((uint32)(Elem)) << 16) |
                  (((uint32)(Elem1)) << 8) |
                  ((uint32)(Elem2));

    /* Remove extra high bits by shifting up */
    bitIndex = module(pVars->usedBits, INBUF_BIT_WIDTH);

    pVars->usedBits += SYNC_WORD_LENGTH_MP3;
    /* This line is faster than to mask off the high bits. */
    returnValue = 0xFFFFFF & (returnValue << (bitIndex));

    /* Move the field down. */

    val = (uint32)(returnValue >> (24 - SYNC_WORD_LENGTH_MP3));

    while (((val & SYNC_WORD) != SYNC_WORD) && (pVars->usedBits < availableBits))
    {
        val <<= 8;
        val |= GetUpTo9Bits(8);
    }

    if (SYNC_WORD == (val & SYNC_WORD))
    {
        return(OMX_TRUE);
    }
    else
    {
        return(OMX_FALSE);
    }
}

OMX_U32 Mp3BitstreamObject::GetNbits(OMX_S32 NeededBits)
{
    uint32    offset;
    uint32    bitIndex;
    uint8    *pElem;         /* Needs to be same type as pInput->pBuffer */
    uint8    *pElem1;
    uint8    *pElem2;
    uint8    *pElem3;
    uint32   returnValue = 0;

    if (!NeededBits)
    {
        return (returnValue);
    }

    offset = (pVars->usedBits) >> INBUF_ARRAY_INDEX_SHIFT;

    pElem  = pVars->pBuffer + offset;;
    pElem1 = pVars->pBuffer + offset + 1;
    pElem2 = pVars->pBuffer + offset + 2;
    pElem3 = pVars->pBuffer + offset + 3;


    returnValue = (((uint32) * (pElem)) << 24) |
                  (((uint32) * (pElem1)) << 16) |
                  (((uint32) * (pElem2)) << 8) |
                  ((uint32) * (pElem3));

    /* Remove extra high bits by shifting up */
    bitIndex = module(pVars->usedBits, INBUF_BIT_WIDTH);

    /* This line is faster than to mask off the high bits. */
    returnValue <<= bitIndex;

    /* Move the field down. */
    returnValue >>= 32 - NeededBits;

    pVars->usedBits += NeededBits;

    return (returnValue);
}

uint16 Mp3BitstreamObject::GetUpTo9Bits(int32 neededBits) /* number of bits to read from the bit stream 2 to 9 */
{

    uint32   offset;
    uint32   bitIndex;
    uint8    Elem;         /* Needs to be same type as pInput->pBuffer */
    uint8    Elem1;
    uint16   returnValue;

    offset = (pVars->usedBits) >> INBUF_ARRAY_INDEX_SHIFT;

    Elem  = *(pVars->pBuffer + offset);
    Elem1 = *(pVars->pBuffer + offset + 1);


    returnValue = (((uint16)(Elem)) << 8) |
                  ((uint16)(Elem1));

    /* Remove extra high bits by shifting up */
    bitIndex = module(pVars->usedBits, INBUF_BIT_WIDTH);

    pVars->usedBits += neededBits;
    /* This line is faster than to mask off the high bits. */
    returnValue = (returnValue << (bitIndex));

    /* Move the field down. */

    return (uint16)(returnValue >> (16 - neededBits));

}


void Mp3BitstreamObject::ResetInputStream()
{
    iInputBufferUsedLength = 0;
    iInputBufferCurrentLength = 0;
    iCurrentFrameLength = 0;
}


OMX_TICKS OmxComponentDecTest::ConvertTimestampIntoOMXTicks(const MediaClockConverter& src)
{
    // This is similar to mediaclockconverter set_value method - except without using the modulo for upper part of 64 bits

    // Timescale value cannot be zero
    OSCL_ASSERT(src.get_timescale() != 0);
    if (src.get_timescale() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxComponentDecTest::ConvertTimestampIntoOMXTicks Input timescale is 0"));

        return (OMX_TICKS) 0;
    }

    OSCL_ASSERT(iOmxTimeScale != 0);
    if (0 == iOmxTimeScale)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxComponentDecTest::ConvertTimestampIntoOMXTicks target timescale is 0"));

        return (OMX_TICKS) 0;
    }

    uint64 value = (uint64(src.get_wrap_count())) << 32;
    value += src.get_current_timestamp();
    // rounding up
    value = (uint64(value) * iOmxTimeScale + uint64(src.get_timescale() - 1)) / src.get_timescale();

    return (OMX_TICKS) value;
}

