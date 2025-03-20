/*
 * Author: Ben Hatton (10903872, University of Manchester)
 * Description: main file for the runtime functionality for the new NVDLA runtime for SlimNNs for a single UMD
 * Date: 13/02/2025
 */

#include "DlaImageUtils.h"
#include "ErrorMacros.h"
#ifndef RUNTIME_TEST_H
#define RUNTIME_TEST_H
#include "RuntimeTest.h"
#endif

#include "nvdla/IRuntime.h"

#include "../../external/include/half.h"
#include "main.h"
#include "nvdla_os_inf.h"

#include "dlaerror.h"
#include "dlatypes.h"

#include <cstdio> // snprintf, fopen
#include <string>

#include <chrono>
#include <thread>

#define CONF_THRESH 0.6

#define OUTPUT_DIMG "output.dimg"

using namespace half_float;

static TestImageTypes getImageType(std::string imageFileName)
{
    TestImageTypes it = IMAGE_TYPE_UNKNOWN;
    std::string ext = imageFileName.substr(imageFileName.find_last_of(".") + 1);

    if (ext == "pgm")
    {
        it = IMAGE_TYPE_PGM;
    }
    else if (ext == "jpg")
    {
        it = IMAGE_TYPE_JPG;
    }

    return it;
}

static NvDlaError copyImageToInputTensor(const TestAppArgs* appArgs, TestInfo* i, void** pImgBuffer)
{
    NvDlaError e = NvDlaSuccess;

    NvDlaImage* R8Image = new NvDlaImage();
    NvDlaImage* FF16Image = NULL;

    std::string imgPath = /*i->inputImagesPath + */appArgs->inputName;
    TestImageTypes imageType = getImageType(imgPath);

    if (appArgs == NULL || i == NULL || pImgBuffer == NULL)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "NULL input parameter");

    if (!R8Image)
        ORIGINATE_ERROR(NvDlaError_InsufficientMemory);

    switch (imageType) {
        case IMAGE_TYPE_PGM:
            PROPAGATE_ERROR(PGM2DIMG(imgPath, R8Image));
            break;
        case IMAGE_TYPE_JPG:
            PROPAGATE_ERROR(JPEG2DIMG(imgPath, R8Image));
            break;
        default:
            NvDlaDebugPrintf("Unknown image type: %s", imgPath.c_str());
            goto fail;
    }

    FF16Image = i->inputImage;
    if (FF16Image == NULL)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "NULL input Image");
    
    PROPAGATE_ERROR(createFF16ImageCopy(appArgs, R8Image, FF16Image));
    PROPAGATE_ERROR(DIMG2DlaBuffer(FF16Image, pImgBuffer));

fail:
    if (R8Image != NULL && R8Image->m_pData != NULL)
        NvDlaFree(R8Image->m_pData);
    delete R8Image;

    return e;
}

static NvDlaError prepareOutputTensor(nvdla::IRuntime::NvDlaTensor* pTDesc, NvDlaImage* pOutImage, void** pOutBuffer)
{
    NvDlaError e = NvDlaSuccess;

    PROPAGATE_ERROR_FAIL(Tensor2DIMG(pTDesc, pOutImage));
    PROPAGATE_ERROR_FAIL(DIMG2DlaBuffer(pOutImage, pOutBuffer));

fail:
    return e;
}

static NvDlaError readLoadable(const TestAppArgs* appArgs, TestInfo* i, int loadableNum)
{
    NvDlaError e = NvDlaSuccess;
    NVDLA_UNUSED(appArgs);
    // std::string loadableName;
    NvDlaFileHandle file;
    NvDlaStatType finfo;
    size_t file_size;
    NvU8 *buf = 0;
    size_t actually_read = 0;
    NvDlaError rc;

    if (appArgs->loadableNames.at(loadableNum) == "")
        ORIGINATE_ERROR_FAIL(NvDlaError_NotInitialized, "No loadable found to load");

    rc = NvDlaFopen(appArgs->loadableNames.at(loadableNum).c_str(), NVDLA_OPEN_READ, &file);
    if (rc != NvDlaSuccess)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "fopen failed for %s\n", appArgs->loadableNames.at(loadableNum).c_str());
    
    rc = NvDlaFstat(file, &finfo);
    if (rc != NvDlaSuccess)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "couldn't get file stats for %s\n", appArgs->loadableNames.at(loadableNum).c_str());
    
    file_size = NvDlaStatGetSize(&finfo);
    if (!file_size)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "zero-length for %s\n", appArgs->loadableNames.at(loadableNum).c_str());
    
    buf = new NvU8[file_size];

    if (!buf)
        ORIGINATE_ERROR_FAIL(NvDlaError_InsufficientMemory, "couldn't allocate buffer for %s\n", appArgs->loadableNames.at(loadableNum).c_str());

    NvDlaFseek(file, 0, NvDlaSeek_Set);

    rc = NvDlaFread(file, buf, file_size, &actually_read);
    if (rc != NvDlaSuccess)
    {
        NvDlaFree(buf);
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "read error for %s\n", appArgs->loadableNames.at(loadableNum).c_str());
    }

    NvDlaFclose(file);
    if (actually_read != file_size)
    {
        NvDlaFree(buf);
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "read wrong size for buffer> %d\n", actually_read);
    }

    i->pData = buf;

fail:
    return e;
}

NvDlaError loadLoadable(const TestAppArgs* appArgs, TestInfo* i)
{
    NvDlaError e = NvDlaSuccess;

    nvdla::IRuntime* runtime = i->runtime;

    if (!runtime)  // Check runtime
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "getRuntime() failed");
    
    if (!runtime->load(i->pData, 0))  // load and check load
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "runtime->load failed");

fail:
    return e;
}

void unloadLoadable(const TestAppArgs* appArgs, TestInfo* i)
{
    NVDLA_UNUSED(appArgs);
    nvdla::IRuntime *runtime = NULL;

    runtime = i->runtime;
    if (runtime != NULL)
        runtime->unload();
}

NvDlaError setupInputBuffer(const TestAppArgs* appArgs, TestInfo* i, void** pInputBuffer)
{
    NvDlaError e = NvDlaSuccess;
    void *hMem = NULL;
    NvS32 numInputTensors = 0;
    nvdla::IRuntime::NvDlaTensor tDesc;

    nvdla::IRuntime* runtime = i->runtime;
    if (!runtime)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "getRuntime() failed");
    
    PROPAGATE_ERROR_FAIL(runtime->getNumInputTensors(&numInputTensors));

    i->numInputs = numInputTensors;

    if (numInputTensors < 1)
        goto fail;
    
    PROPAGATE_ERROR_FAIL(runtime->getInputTensorDesc(0, &tDesc));

    PROPAGATE_ERROR_FAIL(runtime->allocateSystemMemory(&hMem, tDesc.bufferSize, pInputBuffer));
    i->inputHandle = (NvU8 *)hMem;
    PROPAGATE_ERROR_FAIL(copyImageToInputTensor(appArgs, i, pInputBuffer));

    if (!runtime->bindInputTensor(0, hMem))
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "runtime->bindInputTensor() failed");

fail:
    return e;
}

static void cleanupInputBuffer(const TestAppArgs* appArgs, TestInfo* i)
{
    nvdla::IRuntime *runtime = NULL;
    NvS32 numInputTensors = 0;
    nvdla::IRuntime::NvDlaTensor tDesc;
    NvDlaError e = NvDlaSuccess;

    if (i->inputImage != NULL && i->inputImage->m_pData != NULL)
    {
        NvDlaFree(i->inputImage->m_pData);
        i->inputImage->m_pData = NULL;
    }

    runtime = i->runtime;
    if (runtime == NULL) return;

    e = runtime->getNumInputTensors(&numInputTensors);
    if (e != NvDlaSuccess) return;

    if (numInputTensors < 1) return;

    e = runtime->getInputTensorDesc(0, &tDesc);
    if (e != NvDlaSuccess) return;

    if (i->inputHandle == NULL) return;

    /* Free the allocated buffer */
    runtime->freeSystemMemory(i->inputHandle, tDesc.bufferSize);
    i->inputHandle = NULL;
    return;
}

NvDlaError setupOutputBuffer(const TestAppArgs* appArgs, TestInfo* i, void** pOutputBuffer)
{
    NVDLA_UNUSED(appArgs);

    NvDlaError e = NvDlaSuccess;
    void *hMem;
    NvS32 numOutputTensors = 0;
    nvdla::IRuntime::NvDlaTensor tDesc;
    NvDlaImage *pOutputImage = NULL;

    nvdla::IRuntime* runtime = i->runtime;
    if (!runtime)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "getRuntime() failed");
    
    PROPAGATE_ERROR_FAIL(runtime->getNumOutputTensors(&numOutputTensors));

    i->numOutputs = numOutputTensors;

    if (numOutputTensors < 1)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Expected number of output tensors of %u, found %u", 1, numOutputTensors);
    
    PROPAGATE_ERROR_FAIL(runtime->getOutputTensorDesc(0, &tDesc));
    PROPAGATE_ERROR_FAIL(runtime->allocateSystemMemory(&hMem, tDesc.bufferSize, pOutputBuffer));
    i->outputHandle = (NvU8 *)hMem;

    pOutputImage = i->outputImage;
    if (pOutputImage == NULL)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "NULL output Image");
    
    PROPAGATE_ERROR_FAIL(prepareOutputTensor(&tDesc, pOutputImage, pOutputBuffer));

    if (!runtime->bindOutputTensor(0, hMem))
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "runtime->bindOutputTensor() failed");

fail:
    return e;
}

static void cleanupOutputBuffer(const TestAppArgs* appArgs, TestInfo* i)
{
    nvdla::IRuntime *runtime = NULL;
    NvS32 numOutputTensors = 0;
    nvdla::IRuntime::NvDlaTensor tDesc;
    NvDlaError e = NvDlaSuccess;

    /* Do not clear outputImage if in server mode */
    if (!i->dlaServerRunning &&
        i->outputImage != NULL &&
        i->outputImage->m_pData != NULL)
    {
        NvDlaFree(i->outputImage->m_pData);
        i->outputImage->m_pData = NULL;
    }

    runtime = i->runtime;
    if (runtime == NULL) return;

    e = runtime->getNumOutputTensors(&numOutputTensors);
    if (e != NvDlaSuccess) return;

    e = runtime->getOutputTensorDesc(0, &tDesc);
    if (e != NvDlaSuccess) return;

    if (i->outputHandle == NULL) return;

    /* Free the allocated buffer */
    runtime->freeSystemMemory(i->outputHandle, tDesc.bufferSize);
    i->outputHandle = NULL;
    return;
}


NvF32 calc_confidence(std::vector<NvF32>* outputData)
{
    if (!outputData || outputData->size() <= 0)
    {
        NvDlaDebugPrintf("[ERROR] Invalid input data when calculating confidence\n");
        return 0.0f;
    }

    // Step 1: Normalise vector w/ softmax
    NvF32 sumExp = 0.0f;

    // 1.1: Get max logit
    NvF32 maxLogit = outputData->at(0);
    for (int i = 1; i < outputData->size(); i++)
    {
        if (outputData->at(i) > maxLogit)
        {
            maxLogit = outputData->at(i);
        }
    }

    // 1.2: Calculate sum of exponentials
    for (int i = 0; i < outputData->size(); i++)
    {
        sumExp += exp(outputData->at(i) - maxLogit);
    }

    // 1.3: Normalise
    for (int i = 0; i < outputData->size(); i++)
    {
        outputData->at(i) = exp(outputData->at(i) - maxLogit) / sumExp;
    }

    NvF32 top_one = 0;
    NvF32 top_two = 0;

    for (int i = 0; i < outputData->size(); i++)
    {
        if (outputData->at(i) > top_one)
        {
            top_two = top_one;
            top_one = outputData->at(i);
        }
        else if (outputData->at(i) > top_two)
        {
            top_two = outputData->at(i);
        }
    }
    NvDlaDebugPrintf("Top one: %f\n", top_one);
    NvDlaDebugPrintf("Top two: %f\n", top_two);
    return top_one - top_two;
}

NvDlaError runTest(const TestAppArgs* testAppArgs, TestInfo* i, NvF32* conf, bool* final)
{
    NvDlaError e = NvDlaSuccess;
    void* pInputBuffer = NULL;
    void* pOutputBuffer = NULL;
    std::vector<NvF32> outVec;

    NvDlaDebugPrintf("Running test...\n");

    nvdla::IRuntime* runtime = i->runtime;

    if (!runtime)
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "getRuntime() failed");

    i->inputImage = new NvDlaImage();
    i->outputImage = new NvDlaImage();

    NvDlaDebugPrintf("Setting up buffers...\n");
    PROPAGATE_ERROR_FAIL(setupInputBuffer(testAppArgs, i, &pInputBuffer));
    PROPAGATE_ERROR_FAIL(setupOutputBuffer(testAppArgs, i, &pOutputBuffer));

    NvDlaDebugPrintf("Submitting tasks...\n");

    if (!runtime->submit())
        ORIGINATE_ERROR(NvDlaError_BadParameter, "runtime->submit() failed");

    // *outputBuffer = i->outputImage;

    PROPAGATE_ERROR_FAIL(DlaBuffer2DIMG(&pOutputBuffer, i->outputImage));

    PROPAGATE_ERROR_FAIL(i->outputImage->to_float(&outVec));

    *conf = calc_confidence(&outVec);

    NvDlaDebugPrintf("Confidence: %f\n", *conf);
    NvDlaDebugPrintf("Raw output dump: %d\n", testAppArgs->rawOutputDump);

    if (*conf < CONF_THRESH)
    {
        NvDlaDebugPrintf("Confidence is too low, increasing partition\n");
        NvDlaDebugPrintf("Final: %d\n", *final);
        if (final)
        {
            NvDlaDebugPrintf("Cannot increase partition, on final partition, moving to export\n");
        }
        else
        {
            *final = false;
            goto fail;
        }
    }
    else
    {
        NvDlaDebugPrintf("Confidence is high enough, stopping\n");
        *final = true;
    }
    PROPAGATE_ERROR_FAIL(DIMG2DIMGFile(i->outputImage, OUTPUT_DIMG, true, testAppArgs->rawOutputDump));

fail:
    cleanupOutputBuffer(testAppArgs, i);
    if (!i->dlaServerRunning && i->outputImage != NULL)
    {
        delete i->outputImage;
        i->outputImage = NULL;
    }

    cleanupInputBuffer(testAppArgs, i);
    if (i->inputImage != NULL)
    {
        delete i->inputImage;
        i->inputImage = NULL;
    }

    return e;
}

NvDlaError run(const TestAppArgs* tAA, TestInfo* testInfo)
{
    NvDlaError e = NvDlaSuccess;

    int final_part = 0;
    bool final = false;
    NvF32 confidence = 0.0f;
    NvDlaDebugPrintf("creating new runtime contexts...\n");
    for (int i = 0; i < tAA->loadableNames.size(); i++)
    {
        NvDlaDebugPrintf("creating runtime context %d...\n", i);
        testInfo->runtime = nvdla::createRuntime();

        if (testInfo->runtime == NULL)  // Check context creation
            ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "createRuntime() failed");
        
        NvDlaDebugPrintf("loading runtime context %s...\n", tAA->loadableNames.at(i).c_str());
        if (!testInfo->dlaServerRunning)  // Check that server is not running
            PROPAGATE_ERROR_FAIL(readLoadable(tAA, testInfo, i));
        
        /* Load Loadable */
        PROPAGATE_ERROR_FAIL(loadLoadable(tAA, testInfo));

        /* Start Emulator */
        if (!testInfo->runtime->initEMU())
            ORIGINATE_ERROR(NvDlaError_DeviceNotFound, "runtime->initEMU() failed");
        
        NvDlaDebugPrintf("runtime context %d created\n", i);

        if (i == tAA->loadableNames.size() - 1)
        {
            final = true;
            NvDlaDebugPrintf("Final partition, at i=%d\n", i);
        }

        PROPAGATE_ERROR_FAIL(runTest(tAA, testInfo, &confidence, &final));
        final_part = i;

        if (final)
            break;
        if (confidence > CONF_THRESH)
            break;

        // Clean up current runtime context
        if (testInfo->runtime != NULL)
            testInfo->runtime->stopEMU();
        unloadLoadable(tAA, testInfo);

        if (!testInfo->dlaServerRunning && testInfo->pData != NULL)
        {
            delete[] testInfo->pData;
            testInfo->pData = NULL;
        }

        nvdla::destroyRuntime(testInfo->runtime);
    }

    // PROPAGATE_ERROR_FAIL(DIMG2DIMGFile(testInfoVec->at(final_part).outputImage, OUTPUT_DIMG, true, tAAvec->at(final_part).rawOutputDump));

fail:
    /* Stop Emulator */
    if (testInfo->runtime != NULL)
        testInfo->runtime->stopEMU();
    
    /* Unload Loadables */
    unloadLoadable(tAA, testInfo);

    /* Free if allocated in read Loadable */
    if (!testInfo->dlaServerRunning && testInfo->pData != NULL)
    {
        delete[] testInfo->pData;
        testInfo->pData = NULL;
    }

    /* Destroy Runtime */
    nvdla::destroyRuntime(testInfo->runtime);
    
    return e;
}