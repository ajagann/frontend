
// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <numeric>
#include <sstream>

#include "../include/hebench_dotproduct.h"
#include "benchmarks/datagen_helper/include/datagen_helper.h"
#include "include/hebench_math_utils.h"

namespace hebench {
namespace TestHarness {
namespace DotProduct {

//------------------------------------
// class BenchmarkDescriptionCategory
//------------------------------------

hebench::APIBridge::WorkloadParamType::WorkloadParamType
    BenchmarkDescriptionCategory::WorkloadParameterType[BenchmarkDescriptionCategory::WorkloadParameterCount] = {
        hebench::APIBridge::WorkloadParamType::UInt64
    };

std::uint64_t BenchmarkDescriptionCategory::fetchVectorSize(const std::vector<hebench::APIBridge::WorkloadParam> &w_params)
{
    assert(WorkloadParameterCount == 1);
    assert(OpParameterCount == 2);
    assert(OpResultCount == 1);

    std::uint64_t retval;

    if (w_params.size() < WorkloadParameterCount)
    {
        std::stringstream ss;
        ss << "Insufficient workload parameters in 'w_params'. Expected " << WorkloadParameterCount
           << ", but " << w_params.size() << "received.";
        throw std::invalid_argument(IL_LOG_MSG_CLASS(ss.str()));
    } // end if

    // validate parameters
    for (std::size_t i = 0; i < WorkloadParameterCount; ++i)
        if (w_params[i].data_type != WorkloadParameterType[i])
        {
            std::stringstream ss;
            ss << "Invalid type for workload parameter " << i
               << ". Expected type ID " << WorkloadParameterType[i] << ", but " << w_params[i].data_type << " received.";
            throw std::invalid_argument(IL_LOG_MSG_CLASS(ss.str()));
        } // end if
        else if (w_params[i].u_param <= 0)
        {
            std::stringstream ss;
            ss << "Invalid number of elements for vector in workload parameter " << i
               << ". Expected positive integer, but " << w_params[i].u_param << " received.";
            throw std::invalid_argument(IL_LOG_MSG_CLASS(ss.str()));
        } // end else if

    retval = w_params.at(0).u_param;

    return retval;
}

std::string BenchmarkDescriptionCategory::matchBenchmarkDescriptor(const hebench::APIBridge::BenchmarkDescriptor &bench_desc,
                                                                   const std::vector<hebench::APIBridge::WorkloadParam> &w_params) const
{
    std::stringstream ss;

    // return name if benchmark is supported
    if (bench_desc.workload == hebench::APIBridge::Workload::DotProduct)
    {
        try
        {
            std::uint64_t vector_size = fetchVectorSize(w_params);
            ss << BaseWorkloadName << " " << vector_size;
        }
        catch (...)
        {
            // invalid workload
            ss = std::stringstream();
        }
    } // end if

    return ss.str();
}

//---------------------------
// class DataGeneratorHelper
//---------------------------

/**
 * @brief Static helper class to generate vector data for all supported data types.
 */
class DataGeneratorHelper : public hebench::TestHarness::DataGeneratorHelper
{
private:
    IL_DECLARE_CLASS_NAME(DotProduct::DataGeneratorHelper)

public:
    static void vectorDotProduct(hebench::APIBridge::DataType data_type,
                                 void *result, const void *a, const void *b,
                                 std::uint64_t elem_count);

protected:
    DataGeneratorHelper() {}

private:
    template <class T>
    static void vectorDotProduct(T &result, const T *a, const T *b, std::uint64_t elem_count)
    {
        result = std::inner_product(a, a + elem_count, b, T());
    }
};

void DataGeneratorHelper::vectorDotProduct(hebench::APIBridge::DataType data_type,
                                           void *result, const void *a, const void *b,
                                           std::uint64_t elem_count)
{
    if (!result)
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Invalid null 'p_result'."));

    switch (data_type)
    {
    case hebench::APIBridge::DataType::Int32:
        vectorDotProduct<std::int32_t>(*reinterpret_cast<std::int32_t *>(result),
                                       reinterpret_cast<const std::int32_t *>(a), reinterpret_cast<const std::int32_t *>(b),
                                       elem_count);
        break;

    case hebench::APIBridge::DataType::Int64:
        vectorDotProduct<std::int64_t>(*reinterpret_cast<std::int64_t *>(result),
                                       reinterpret_cast<const std::int64_t *>(a), reinterpret_cast<const std::int64_t *>(b),
                                       elem_count);
        break;

    case hebench::APIBridge::DataType::Float32:
        vectorDotProduct<float>(*reinterpret_cast<float *>(result),
                                reinterpret_cast<const float *>(a), reinterpret_cast<const float *>(b),
                                elem_count);
        break;

    case hebench::APIBridge::DataType::Float64:
        vectorDotProduct<double>(*reinterpret_cast<double *>(result),
                                 reinterpret_cast<const double *>(a), reinterpret_cast<const double *>(b),
                                 elem_count);
        break;

    default:
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Unknown data type."));
        break;
    } // end switch
}

//---------------------
// class DataGenerator
//---------------------

DataGenerator::Ptr DataGenerator::create(std::uint64_t vector_size,
                                         std::uint64_t batch_size_a,
                                         std::uint64_t batch_size_b,
                                         hebench::APIBridge::DataType data_type)
{
    DataGenerator::Ptr retval = DataGenerator::Ptr(new DataGenerator());
    retval->init(vector_size, batch_size_a, batch_size_b, data_type);
    return retval;
}

void DataGenerator::init(std::uint64_t vector_size,
                         std::uint64_t batch_size_a,
                         std::uint64_t batch_size_b,
                         hebench::APIBridge::DataType data_type)
{
    // Load/generate and initialize the data for vector element-wise addition:
    // C = A . B

    // number of samples in each input parameter and output
    std::size_t batch_sizes[InputDim0 + OutputDim0] = {
        batch_size_a,
        batch_size_b,
        batch_size_a * batch_size_b
    };
    // initialize base data (data packs)
    PartialDataLoader::init(InputDim0, batch_sizes, OutputDim0);

    // compute buffer size in bytes for each vector
    std::uint64_t buffer_sizes[InputDim0 + OutputDim0];
    // input
    for (std::size_t i = 0; i < InputDim0; ++i)
    {
        buffer_sizes[i] = vector_size * PartialDataLoader::sizeOf(data_type);
    } // end for
    // output
    for (std::size_t i = InputDim0; i < InputDim0 + OutputDim0; ++i)
    {
        // an output vector has a single component (result of dot product)
        buffer_sizes[i] = PartialDataLoader::sizeOf(data_type);
    } // end for

    // allocate memory for each vector buffer
    allocate(buffer_sizes, // sizes (in bytes) for each input vector
             InputDim0, // number of input vectors
             buffer_sizes + InputDim0, // sizes (in bytes) for each output vector
             OutputDim0); // number of output vectors

    // at this point all NativeDataBuffers have been allocated and pointed to the correct locations

    // fill up each vector data

    // input
    for (std::size_t vector_i = 0; vector_i < InputDim0; ++vector_i)
    {
        for (std::uint64_t i = 0; i < batch_sizes[vector_i]; ++i)
        {
            // generate the data
            DataGeneratorHelper::generateRandomVectorN(data_type,
                                                       getParameterData(vector_i).p_buffers[i].p,
                                                       vector_size, 0.0, 10.0);
        } // end for
    } // end for

    // output
    //#pragma omp parallel for collapse(2)
    for (std::uint64_t a_i = 0; a_i < batch_sizes[0]; ++a_i)
    {
        for (std::uint64_t b_i = 0; b_i < batch_sizes[1]; ++b_i)
        {
            // find the index for the result buffer based on the input indices
            std::uint64_t ppi[] = { a_i, b_i };
            std::uint64_t r_i   = getResultIndex(ppi);

            // generate the data
            DataGeneratorHelper::vectorDotProduct(data_type,
                                                  getResultData(0).p_buffers[r_i].p, // C
                                                  getParameterData(0).p_buffers[a_i].p, // A
                                                  getParameterData(1).p_buffers[b_i].p, // B
                                                  vector_size);
        } // end for
    } // end for

    // all data has been generated at this point
}

} // namespace DotProduct
} // namespace TestHarness
} // namespace hebench
