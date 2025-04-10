/*
 * Author: Ben Hatton (1093872, University of Manchester)
 * Description: header file for the runtime functionality for the new NVDLA runtime for SNNs
 * Date: 13/02/2025
 */

#include "dlaerror.h"
#include "dlatypes.h"

#include "nvdla/IRuntime.h"

#include "DlaImage.h"
#include "ErrorMacros.h"

#include "nvdla_inf.h"

#include <string>
#include <vector>

struct TestAppArgs
{
    std::string inputPath;
    std::string inputName;
    std::vector<std::string> loadableNames;
    NvS32 serverPort;
    float normalize_value[4];
    float mean[4];
    bool rawOutputDump;

    TestAppArgs() :
        inputPath("./"),
        inputName(""),
        loadableNames(),
        serverPort(6666),
        normalize_value{1.0, 1.0, 1.0, 1.0},
        mean{0.0, 0.0, 0.0, 0.0},
        rawOutputDump(false)
    {}
};

struct TestInfo
{
    nvdla::IRuntime* runtime;
    std::string inputLoadablePath;
    NvU8 *inputHandle;
    NvU8 *outputHandle;
    NvU8 *pData;
    bool dlaServerRunning;
    NvS32 dlaRemoteSock;
    NvS32 dlaServerSock;
    NvU32 numInputs;
    NvU32 numOutputs;
    NvDlaImage* inputImage;
    NvDlaImage* outputImage;

    TestInfo() :
        runtime(NULL),
        inputLoadablePath(""),
        inputHandle(NULL),
        outputHandle(NULL),
        pData(NULL),
        dlaServerRunning(false),
        dlaRemoteSock(-1),
        dlaServerSock(-1),
        numInputs(0),
        numOutputs(0),
        inputImage(NULL),
        outputImage(NULL)
    {}
};

NvDlaError run(const TestAppArgs* tAA, TestInfo* testInfo);