/*
 * Author: Ben Hatton (1093872, University of Manchester)
 * Description: Header file for main_snn.cpp
 * Date: 13/02/2025
 */

#include "dlaerror.h"
#include "dlatypes.h"

#include "nvdla/IRuntime.h"
#include "DlaImageUtils.h"

#include "ErrorMacros.h"

#include "nvdla_inf.h"

#include <string>

#ifndef RUNTIME_TEST_H
#define RUNTIME_TEST_H
#include "RuntimeTest.h"
#endif

enum TestImageTypes
{
    IMAGE_TYPE_PGM = 0,
    IMAGE_TYPE_JPG = 1,
    IMAGE_TYPE_UNKNOWN = 2,
};

NvDlaError launchTest(const TestAppArgs* appArgs, std::vector<std::string> loadableNames);
NvDlaError testSetup(const TestAppArgs* appArgs, TestInfo* i);

NvDlaError run(const TestAppArgs* appArgs, TestInfo* i, std::vector<std::string> loadableNames);

NvDlaError loadLoadable(const TestAppArgs* appArgs, TestInfo* i);
NvDlaError setupBuffers(const TestAppArgs* appArgs, TestInfo* i);
NvDlaError runTest(const TestAppArgs* appArgs, TestInfo* i);

NvDlaError DIMG2DlaBuffer(const NvDlaImage* image, void** pBuffer);
NvDlaError DlaBuffer2DIMG(void** pBuffer, NvDlaImage* image);

NvDlaError Tensor2DIMG(const nvdla::IRuntime::NvDlaTensor* pTDesc, NvDlaImage* image);
NvDlaError createFF16ImageCopy(const TestAppArgs* appArgs, NvDlaImage* in, NvDlaImage* out);