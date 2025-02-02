
// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <sstream>

#include "../include/hebench_matmult.h"
#include "benchmarks/datagen_helper/include/datagen_helper.h"

namespace hebench {
namespace TestHarness {
namespace MatrixMultiply {

//------------------------------------
// class BenchmarkDescriptionCategory
//------------------------------------

hebench::APIBridge::WorkloadParamType::WorkloadParamType
    BenchmarkDescriptionCategory::WorkloadParameterType[BenchmarkDescriptionCategory::WorkloadParameterCount] = {
        hebench::APIBridge::WorkloadParamType::UInt64,
        hebench::APIBridge::WorkloadParamType::UInt64,
        hebench::APIBridge::WorkloadParamType::UInt64
    };

std::array<std::pair<std::uint64_t, std::uint64_t>, BenchmarkDescriptionCategory::OpParameterCount>
BenchmarkDescriptionCategory::fetchMatrixSizes(const std::vector<hebench::APIBridge::WorkloadParam> &w_params)
{
    assert(WorkloadParameterCount == 3);
    assert(OpParameterCount == 2);
    assert(OpResultCount == 1);

    std::array<std::pair<std::uint64_t, std::uint64_t>, OpParameterCount> retval;

    if (w_params.size() < WorkloadParameterCount)
    {
        std::stringstream ss;
        ss << "Insufficient workload parameters in 'w_params'. Expected " << WorkloadParameterCount
           << ", but " << w_params.size() << "received.";
        throw std::invalid_argument(IL_LOG_MSG_CLASS(ss.str()));
    } // end if

    // validate workload parameters
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
            ss << "Invalid matrix size in workload parameter " << i
               << ". Expected positive integer, but " << w_params[i].u_param << " received.";
            throw std::invalid_argument(IL_LOG_MSG_CLASS(ss.str()));
        } // end if

    retval.at(0) = std::make_pair(w_params.at(0).u_param, w_params.at(1).u_param);
    retval.at(1) = std::make_pair(w_params.at(1).u_param, w_params.at(2).u_param);

    return retval;
}

std::string BenchmarkDescriptionCategory::matchBenchmarkDescriptor(const hebench::APIBridge::BenchmarkDescriptor &bench_desc,
                                                                   const std::vector<hebench::APIBridge::WorkloadParam> &w_params) const
{
    std::stringstream ss;

    // return true if benchmark is supported
    if (bench_desc.workload == hebench::APIBridge::Workload::MatrixMultiply)
    {
        try
        {
            auto mat_dims = fetchMatrixSizes(w_params);
            ss << BaseWorkloadName << " ("
               << mat_dims[0].first << "x" << mat_dims[0].second << ") x ("
               << mat_dims[1].first << "x" << mat_dims[1].second << ")";
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
 * @brief Static helper class to generate matrix data for all supported data types.
 */
class DataGeneratorHelper
{
private:
    IL_DECLARE_CLASS_NAME(MatrixMultiply::DataGeneratorHelper)

public:
    static void generateRandomMatrixN(hebench::APIBridge::DataType data_type,
                                      void *mat_result, std::uint64_t rows, std::uint64_t cols,
                                      double mean, double stddev);
    static void matMul(hebench::APIBridge::DataType data_type,
                       void *mat_result, const void *mat_a, const void *mat_b,
                       std::uint64_t rows_a, std::uint64_t cols_a, std::uint64_t cols_b);

protected:
    DataGeneratorHelper() {}

private:
    template <class T>
    static void matMul(T *mat_result, const T *mat_a, const T *mat_b,
                       std::uint64_t rows_a, std::uint64_t cols_a, std::uint64_t cols_b)
    {
        // perform matrix multiplication (straight-forward way,
        // maybe optimize later)
        for (std::uint64_t row_a = 0; row_a < rows_a; ++row_a)
            for (std::uint64_t col_b = 0; col_b < cols_b; ++col_b)
            {
                mat_result[row_a * cols_b + col_b] = 0;
                for (std::uint64_t col_a = 0; col_a < cols_a; ++col_a)
                {
                    std::uint64_t row_b = col_a;
                    mat_result[row_a * cols_b + col_b] += mat_a[row_a * cols_a + col_a] * mat_b[row_b * cols_b + col_b];
                } // end for
            } // end for
    }
};

void DataGeneratorHelper::generateRandomMatrixN(APIBridge::DataType data_type,
                                                void *mat_result, std::uint64_t rows, std::uint64_t cols,
                                                double mean, double stddev)
{
    hebench::TestHarness::DataGeneratorHelper::generateRandomVectorN(data_type,
                                                                     mat_result, rows * cols,
                                                                     mean, stddev);
}

void DataGeneratorHelper::matMul(hebench::APIBridge::DataType data_type,
                                 void *mat_result,
                                 const void *mat_a, const void *mat_b,
                                 uint64_t rows_a, uint64_t cols_a, uint64_t cols_b)
{
    switch (data_type)
    {
    case hebench::APIBridge::DataType::Int32:
        matMul<std::int32_t>(reinterpret_cast<std::int32_t *>(mat_result),
                             reinterpret_cast<const std::int32_t *>(mat_a), reinterpret_cast<const std::int32_t *>(mat_b),
                             rows_a, cols_a, cols_b);
        break;

    case hebench::APIBridge::DataType::Int64:
        matMul<std::int64_t>(reinterpret_cast<std::int64_t *>(mat_result),
                             reinterpret_cast<const std::int64_t *>(mat_a), reinterpret_cast<const std::int64_t *>(mat_b),
                             rows_a, cols_a, cols_b);
        break;

    case hebench::APIBridge::DataType::Float32:
        matMul<float>(reinterpret_cast<float *>(mat_result),
                      reinterpret_cast<const float *>(mat_a), reinterpret_cast<const float *>(mat_b),
                      rows_a, cols_a, cols_b);
        break;

    case hebench::APIBridge::DataType::Float64:
        matMul<double>(reinterpret_cast<double *>(mat_result),
                       reinterpret_cast<const double *>(mat_a), reinterpret_cast<const double *>(mat_b),
                       rows_a, cols_a, cols_b);
        break;

    default:
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Unknown data type."));
        break;
    } // end switch
}

//---------------------
// class DataGenerator
//---------------------

DataGenerator::Ptr DataGenerator::create(std::uint64_t rows_a, std::uint64_t cols_a, std::uint64_t cols_b,
                                         std::uint64_t batch_size_mat_a,
                                         std::uint64_t batch_size_mat_b,
                                         hebench::APIBridge::DataType data_type)
{
    DataGenerator::Ptr retval = DataGenerator::Ptr(new DataGenerator());
    retval->init(rows_a, cols_a, cols_b, batch_size_mat_a, batch_size_mat_b, data_type);
    return retval;
}

void DataGenerator::init(std::uint64_t rows_a, std::uint64_t cols_a, std::uint64_t cols_b,
                         std::uint64_t batch_size_mat_a,
                         std::uint64_t batch_size_mat_b,
                         hebench::APIBridge::DataType data_type)
{
    // Load/generate and initialize the data for matrix multiplication:
    // M2 = M0 * M1

    // number of samples in each input parameter and output
    std::size_t batch_sizes[InputDim0 + 1] = {
        batch_size_mat_a,
        batch_size_mat_b,
        batch_size_mat_a * batch_size_mat_b
    };
    // initialize base data (data packs)
    PartialDataLoader::init(InputDim0, batch_sizes, OutputDim0);

    // store the dimensions of each matrix
    std::array<std::pair<std::uint64_t, std::uint64_t>, InputDim0 + 1> mat_dims; // rows <=> first, cols <=> second
    mat_dims[0] = std::make_pair(rows_a, cols_a);
    mat_dims[1] = std::make_pair(cols_a, cols_b);
    mat_dims[2] = std::make_pair(rows_a, cols_b);

    // compute buffer size for each matrix
    std::uint64_t buffer_sizes[InputDim0 + 1];
    for (std::size_t i = 0; i < InputDim0 + 1; ++i)
    {
        buffer_sizes[i] = mat_dims[i].first * mat_dims[i].second * PartialDataLoader::sizeOf(data_type);
    } // end for

    // allocate memory for each matrix buffer
    allocate(buffer_sizes, // sizes (in bytes) for each input matrix
             InputDim0, // number of input matrices
             buffer_sizes + InputDim0, // sizes (in bytes) for each output matrix
             OutputDim0); // number of output matrices

    // at this point all NativeDataBuffers have been allocated and pointed to the correct locations

    // fill up the matrices data

    // input
    for (std::size_t mat_i = 0; mat_i < InputDim0; ++mat_i)
    {
        for (std::uint64_t i = 0; i < batch_sizes[mat_i]; ++i)
        {
            // generate the data
            DataGeneratorHelper::generateRandomMatrixN(data_type,
                                                       getParameterData(mat_i).p_buffers[i].p,
                                                       mat_dims[mat_i].first, // rows
                                                       mat_dims[mat_i].second, // columns
                                                       0.0, 10.0);
        } // end for
    } // end for

    // output
    //#pragma omp parallel for collapse(2)
    for (std::uint64_t m0_i = 0; m0_i < batch_sizes[0]; ++m0_i)
    {
        for (std::uint64_t m1_i = 0; m1_i < batch_sizes[1]; ++m1_i)
        {
            // find the index for the result buffer based on the input indices
            std::uint64_t ppi[] = { m0_i, m1_i };
            std::uint64_t r_i   = getResultIndex(ppi);

            // generate the data
            DataGeneratorHelper::matMul(data_type,
                                        getResultData(0).p_buffers[r_i].p,
                                        getParameterData(0).p_buffers[m0_i].p,
                                        getParameterData(1).p_buffers[m1_i].p,
                                        mat_dims[0].first, mat_dims[0].second, // dims for m0
                                        mat_dims[1].second); // dims for m1
        } // end for
    } // end for

    // all data has been generated at this point
}

} // namespace MatrixMultiply
} // namespace TestHarness
} // namespace hebench
