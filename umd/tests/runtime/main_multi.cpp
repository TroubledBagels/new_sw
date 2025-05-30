/*
 * Author: Ben Hatton (10903872, University of Manchester)
 * Description: main file for the new NVDLA runtime for multiple UMDs
 * Date: 13/02/2025
 */

#include "ErrorMacros.h"
#ifndef RUNTIME_TEST_H
#define RUNTIME_TEST_H
#include "RuntimeTest.h"
#endif
#include "Server.h"

#include "nvdla_os_inf.h"

#include <cstring>
#include <iostream>
#include <vector>
#include <cstdlib> // system

#include <chrono>
#include <thread>

static TestAppArgs defaultTestAppArgs = TestAppArgs();

static NvDlaError testSetup(const TestAppArgs* appArgs, TestInfo* i)
{
    NvDlaError e = NvDlaSuccess;

    std::string imagePath = "";
    NvDlaStatType stat;

    // Do input paths exist?
    if (std::strcmp(appArgs->inputName.c_str(), "") != 0)
    {
        e = NvDlaStat(appArgs->inputPath.c_str(), &stat);
        if (e != NvDlaSuccess)
            ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Input path does not exist: \"%s\"", appArgs->inputPath.c_str());

        imagePath = /* appArgs->inputPath + "/images/" + */appArgs->inputName;
        e = NvDlaStat(imagePath.c_str(), &stat);
        if (e != NvDlaSuccess)
            ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Image path does not exist: \"%s/%s\"", imagePath.c_str());
    }

    return NvDlaSuccess;

fail:
    return e;
}

static NvDlaError launchServer(const TestAppArgs* appArgs) 
{
    NvDlaError e = NvDlaSuccess;
    TestInfo testInfo;

    testInfo.dlaServerRunning = false;
    PROPAGATE_ERROR_FAIL(runServer(appArgs, &testInfo));

fail:
    return e;
}

static NvDlaError launchTest(const std::vector<TestAppArgs>* appArgs)
{
    NvDlaError e = NvDlaSuccess;
    std::vector<TestInfo> testInfoVec = std::vector<TestInfo>(appArgs->size());

    for (int i = 0; i < appArgs->size(); i++)
    {
        testInfoVec[i].dlaServerRunning = false;
        PROPAGATE_ERROR_FAIL(testSetup(&appArgs->at(i), &testInfoVec[i]));
    }

    PROPAGATE_ERROR_FAIL(run(appArgs, &testInfoVec));

fail:
    return e;
}

static void printHelp(char* argv[]) {
        NvDlaDebugPrintf("Usage: %s [-options] --parts <int> (--loadable <loadable_file>)+\n", argv[0]);
        NvDlaDebugPrintf("where options include:\n");
        NvDlaDebugPrintf("    -h                    print this help message\n");
        NvDlaDebugPrintf("    -s                    launch test in server mode\n");
        NvDlaDebugPrintf("    --image <file>        input jpg/pgm file\n");
        NvDlaDebugPrintf("    --normalize <value>   normalize value for input image\n");
        NvDlaDebugPrintf("    --mean <value>        comma separated mean value for input image\n");
        NvDlaDebugPrintf("    --rawdump             dump raw dimg data\n");
        NvDlaDebugPrintf("    --parts <int>         number of loadables\n");
}

int main(int argc, char* argv[])
{
    NvDlaDebugPrintf("Slimmable Neural Network on NVDLA Runtime (Multi-UMD) Version 4.0\n");

    NvDlaDebugPrintf("Beginning test...\n");
    NvDlaError e = NvDlaError_TestApplicationFailed;
    int num_loadables = 0;
    bool showHelp = false;
    bool unknownArg = false;
    bool missingArg = false;
    bool inputPathSet = false;
    bool serverMode = false;
    NVDLA_UNUSED(inputPathSet);
    NvDlaDebugPrintf("Initialised variables\nDoing first pass on arguments\n");

    // First pass to check for how many loadables we have
    NvS32 ii = 1;
    while (true)
    {
        if (ii >= argc) 
            break;

        const char* arg = argv[ii];

        if (std::strcmp(arg, "--parts") == 0)
        {
            if (ii+1 >= argc) // Check that there is another parameter
            {
                showHelp = true;
                break;
            }

            num_loadables = atoi(argv[++ii]);
        }

        ii++;
    }

    if (num_loadables == 0) 
    {
        showHelp = true;
        missingArg = true;

        printHelp(argv);

        if (unknownArg || missingArg)
            return EXIT_FAILURE;
        else
            return EXIT_SUCCESS;
    }

    NvDlaDebugPrintf("Number of loadables: %d\n", num_loadables);
    
    std::vector<TestAppArgs> tAAvec = std::vector<TestAppArgs>(num_loadables);
    for (int i = 0; i < num_loadables; i++)
    {
        tAAvec[i] = defaultTestAppArgs;
    }

    NvDlaDebugPrintf("Initialised testAppArgs\nBeginning second pass on arguments...\n");
    
    // Second pass to set up the testAppArgs
    ii = 1;
    int loadable_counter = 0;
    while (true)
    {
        NvDlaDebugPrintf("ii: %d, arg: %s\n", ii, argv[ii]);
        if (ii >= argc)
            break;
        
        const char* arg = argv[ii];
        
        if (std::strcmp(arg, "-h") == 0)
        {
            showHelp = true;
            break;
        }
        if(std::strcmp(arg, "-s") == 0)
        {
            serverMode = true;
            break;
        }
        else if (std::strcmp(arg, "-i") == 0)
        {
            if (ii+1 >= argc)
            {
                NvDlaDebugPrintf("[ERROR] No input path provided\n");
                showHelp = true;
                break;
            }

            for (int i = 0; i < num_loadables; i++)
            {
                tAAvec[i].inputPath = std::string(argv[++ii]);
            }
            inputPathSet = true;
        }
        else if (std::strcmp(arg, "--image") == 0)
        {
            if (ii+1 >= argc)
            {
                NvDlaDebugPrintf("[ERROR] No image name provided\n");
                showHelp = true;
                break;
            }

            for (int i = 0; i < num_loadables; i++)
            {
                tAAvec[i].inputName = std::string(argv[ii+1]);
            }

            ii++;
        }
        else if (std::strcmp(arg, "--loadable") == 0)
        {
            if (ii+1 >= argc)
            {
                NvDlaDebugPrintf("[ERROR] No loadable name provided for %d\n", loadable_counter);
                showHelp = true;
                break;
            }

            tAAvec[loadable_counter++].loadableName = std::string(argv[++ii]);
        }
        else if (std::strcmp(arg, "--normalize") == 0)
        {
            if (ii+1 >= argc)
            {
                showHelp = true;
                break;
            }

            char *token;
            int i = 0;
            NvDlaDebugPrintf("STD values provided\n");
            token = strtok(argv[++ii], ",\n");

            while (token != NULL)
            {
                if (i > 3) {
                    NvDlaDebugPrintf("Number of STD values should not be greater than 4 \n");
                    showHelp = true;
                    break;
                }

                for (int j = 0; j < num_loadables; j++)
                {
                    tAAvec[j].normalize_value[i] = atof(token);
                }

                token = strtok(NULL, ",\n");
                i++;
            }
        }
        else if (std::strcmp(arg, "--mean") == 0)
        {
            if (ii+1 >= argc)
            {
                showHelp = true;
                break;
            }

            char *token;
            int i = 0;
            NvDlaDebugPrintf("Mean values provided\n");
            token = strtok(argv[++ii], ",\n");

            while (token != NULL)
            {
                if (i > 3) {
                    NvDlaDebugPrintf("Number of mean values should not be greater than 4 \n");
                    showHelp = true;
                    break;
                }

                for (int j = 0; j < num_loadables; j++)
                {
                    tAAvec[j].mean[i] = atof(token);
                }

                token = strtok(NULL, ",\n");
                i++;
            }
        }
        else if (std::strcmp(arg, "--rawdump") == 0)
        {
            NvDlaDebugPrintf("Raw output dump enabled\n");
            for (int i = 0; i < num_loadables; i++)
            {
                tAAvec[i].rawOutputDump = true;
            }
        }
        else if (std::strcmp(arg, "--parts") == 0)
        {
            ii++;
        }
        else
        {
            NvDlaDebugPrintf("Unknown argument: %s\n", arg);
            unknownArg = true;
            showHelp = true;
            break;
        }

        ii++;
    }

    if (num_loadables != loadable_counter)
    {
        showHelp = true;
        missingArg = true;
    }

    // Get details of each
    for (int i = 0; i < num_loadables; i++)
    {
        NvDlaDebugPrintf("Loadable %d\n", i);
        NvDlaDebugPrintf("Input path: %s\n", tAAvec[i].inputPath.c_str());
        NvDlaDebugPrintf("Input name: %s\n", tAAvec[i].inputName.c_str());
        NvDlaDebugPrintf("Loadable name: %s\n", tAAvec[i].loadableName.c_str());
        NvDlaDebugPrintf("STD values: %f, %f, %f, %f\n", tAAvec[i].normalize_value[0], tAAvec[i].normalize_value[1], tAAvec[i].normalize_value[2], tAAvec[i].normalize_value[3]);
        NvDlaDebugPrintf("Mean values: %f, %f, %f, %f\n", tAAvec[i].mean[0], tAAvec[i].mean[1], tAAvec[i].mean[2], tAAvec[i].mean[3]);
        NvDlaDebugPrintf("Raw output dump: %d\n", tAAvec[i].rawOutputDump);
    }

    NvDlaDebugPrintf("Finished second pass on arguments\nCorrect number of loadables provided\n");

    for (int i = 0; i < num_loadables; i++) {
        if (strcmp(tAAvec[i].loadableName.c_str(), "") == 0) {
            showHelp = true;
            missingArg = true;
            break;
        }
    }

    if (showHelp)
    {
        printHelp(argv);

        if (unknownArg || missingArg)
            return EXIT_FAILURE;
        else
            return EXIT_SUCCESS;
    }

    NvDlaDebugPrintf("No help required\nBeginning test...\n");

    if (serverMode)
    {
        NvDlaDebugPrintf("Server functionality not implemented\n");
        return EXIT_FAILURE;
        // e = launchServer(&tAAvec);
    }
    else
    {
        e = launchTest(&tAAvec);
    }

    if (e != NvDlaSuccess)
    {
        return EXIT_FAILURE;
    }
    else
    {
        NvDlaDebugPrintf("Test pass\n");
        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}