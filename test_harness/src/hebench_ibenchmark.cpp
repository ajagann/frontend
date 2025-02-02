
// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "modules/logging/include/logging.h"
#include "modules/timer/include/timer.h"

#include "hebench/api_bridge/api.h"
#include "include/hebench_engine.h"
#include "include/hebench_ibenchmark.h"
#include "include/hebench_utilities.h"

namespace hebench {
namespace TestHarness {

IBenchmarkDescription::DescriptionToken::Ptr
IBenchmarkDescription::createToken(const BenchmarkConfig &config,
                                   const hebench::APIBridge::Handle &h_desc,
                                   const hebench::APIBridge::BenchmarkDescriptor &bench_desc,
                                   const std::vector<hebench::APIBridge::WorkloadParam> &w_params) const
{
    return DescriptionToken::Ptr(new DescriptionToken(this, config, h_desc, bench_desc, w_params, m_key_creation));
}

//-----------------------------------
// class PartialBenchmarkDescription
//-----------------------------------

PartialBenchmarkDescription::PartialBenchmarkDescription()
{
}

PartialBenchmarkDescription::~PartialBenchmarkDescription()
{
}

std::unordered_set<std::size_t> PartialBenchmarkDescription::getCipherParamPositions(std::uint32_t cipher_param_mask)
{
    std::unordered_set<std::size_t> retval;
    std::bitset<sizeof(decltype(cipher_param_mask)) * 8> cipher_param_bits(cipher_param_mask);
    for (std::size_t i = 0; i < cipher_param_bits.size(); ++i)
        if (cipher_param_bits.test(i))
            retval.insert(i);
    return retval;
}

std::string PartialBenchmarkDescription::getCategoryName(hebench::APIBridge::Category category)
{
    std::string retval;

    switch (category)
    {
    case hebench::APIBridge::Category::Latency:
        retval = "Latency";
        break;

    case hebench::APIBridge::Category::Offline:
        retval = "Offline";
        break;

    default:
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Unknown category."));
        break;
    } // end switch

    return retval;
}

std::string PartialBenchmarkDescription::getDataTypeName(hebench::APIBridge::DataType data_type)
{
    std::string retval;

    switch (data_type)
    {
    case hebench::APIBridge::DataType::Int32:
        retval = "Int32";
        break;

    case hebench::APIBridge::DataType::Int64:
        retval = "Int64";
        break;

    case hebench::APIBridge::DataType::Float32:
        retval = "Float32";
        break;

    case hebench::APIBridge::DataType::Float64:
        retval = "Float64";
        break;

    default:
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Unknown data type."));
        break;
    } // end switch

    return retval;
}

std::uint64_t PartialBenchmarkDescription::computeSampleSizes(std::uint64_t *sample_sizes,
                                                              std::size_t param_count,
                                                              std::uint64_t default_sample_size,
                                                              const hebench::APIBridge::BenchmarkDescriptor &bench_desc)
{
    std::uint64_t result_batch_size = 1;
    for (std::size_t param_i = 0; param_i < param_count; ++param_i)
    {
        sample_sizes[param_i] = (bench_desc.cat_params.offline.data_count[param_i] == 0 ?
                                     default_sample_size :
                                     bench_desc.cat_params.offline.data_count[param_i]);
        result_batch_size *= sample_sizes[param_i];
    } // end for
    return result_batch_size;
}

IBenchmarkDescription::DescriptionToken::Ptr PartialBenchmarkDescription::matchBenchmarkDescriptor(const Engine &engine,
                                                                                                   const IBenchmarkDescription::BenchmarkConfig &bench_config,
                                                                                                   const hebench::APIBridge::Handle &h_desc,
                                                                                                   const std::vector<hebench::APIBridge::WorkloadParam> &w_params) const
{
    IBenchmarkDescription::DescriptionToken::Ptr retval;

    hebench::APIBridge::BenchmarkDescriptor bench_desc;
    std::uint64_t w_params_count, other;
    engine.validateRetCode(hebench::APIBridge::getWorkloadParamsDetails(engine.handle(), h_desc, &w_params_count, &other));
    if (w_params_count != w_params.size())
    {
        std::stringstream ss;
        ss << "Invalid number of workload arguments. Expected " << w_params_count << ", but " << w_params.size() << " received.";
        throw std::runtime_error(IL_LOG_MSG_CLASS(ss.str()));
    } // end if
    engine.validateRetCode(hebench::APIBridge::describeBenchmark(engine.handle(), h_desc, &bench_desc, nullptr));

    std::string s_workload_name = matchBenchmarkDescriptor(bench_desc, w_params);
    if (!s_workload_name.empty())
    {
        retval                            = createToken(bench_config, h_desc, bench_desc, w_params);
        retval->description.workload_name = s_workload_name;
        describe(engine, retval);
    } // end if

    return retval;
}

void PartialBenchmarkDescription::describe(const Engine &engine,
                                           DescriptionToken::Ptr pre_token) const
{
    Description &description                                       = pre_token->description;
    hebench::APIBridge::Handle h_bench_desc                        = pre_token->getDescriptorHandle(this);
    const hebench::APIBridge::BenchmarkDescriptor &bench_desc      = pre_token->getDescriptor(this);
    const std::vector<hebench::APIBridge::WorkloadParam> &w_params = pre_token->getWorkloadParams(this);

    std::stringstream ss;
    std::filesystem::path ss_path;
    std::string &s_workload_name = description.workload_name;
    std::string s_path_workload_name;
    std::string s_scheme_name   = engine.getSchemeName(bench_desc.scheme);
    std::string s_security_name = engine.getSecurityName(bench_desc.scheme, bench_desc.security);

    // generate path
    ss = std::stringstream();
    if (!s_workload_name.empty())
        ss << s_workload_name << "_";
    else
        s_workload_name = std::to_string(static_cast<int>(bench_desc.workload));
    ss << std::to_string(static_cast<int>(bench_desc.workload));
    s_path_workload_name = hebench::Utilities::convertToDirectoryName(ss.str());
    ss_path              = s_path_workload_name;
    ss                   = std::stringstream();
    ss << "wp";
    for (std::size_t i = 0; i < w_params.size(); ++i)
    {
        ss << "_";
        switch (w_params[i].data_type)
        {
        case hebench::APIBridge::WorkloadParamType::UInt64:
            ss << w_params[i].u_param;
            break;

        case hebench::APIBridge::WorkloadParamType::Float64:
            ss << w_params[i].f_param;
            break;

        default:
            ss << w_params[i].i_param;
            break;
        } // end switch
    } // end for
    ss_path /= hebench::Utilities::convertToDirectoryName(ss.str()); // workload params
    ss_path /= hebench::Utilities::convertToDirectoryName(PartialBenchmarkDescription::getCategoryName(bench_desc.category));
    ss_path /= hebench::Utilities::convertToDirectoryName(PartialBenchmarkDescription::getDataTypeName(bench_desc.data_type));
    std::size_t max_non_zero = HEBENCH_MAX_CATEGORY_PARAMS;
    while (max_non_zero > 0 && bench_desc.cat_params.reserved[max_non_zero - 1] == 0)
        --max_non_zero;
    ss = std::stringstream();
    if (max_non_zero > 0)
    {
        for (std::size_t i = 0; i < max_non_zero; ++i)
            ss << bench_desc.cat_params.reserved[i];
    } // end if
    else
        ss << "default";
    ss_path /= ss.str();
    // cipher/plain parameters
    ss = std::stringstream();
    std::unordered_set<std::size_t> cipher_param_pos =
        PartialBenchmarkDescription::getCipherParamPositions(bench_desc.cipher_param_mask);
    if (cipher_param_pos.empty())
        ss << "all_plain";
    else if (cipher_param_pos.size() >= sizeof(std::uint32_t) * 8)
        ss << "all_cipher";
    else
    {
        std::size_t max_elem = *std::max_element(cipher_param_pos.begin(), cipher_param_pos.end());
        for (std::size_t i = 0; i <= max_elem; ++i)
            ss << (cipher_param_pos.count(i) > 0 ? 'c' : 'p');
    } // end else
    ss_path /= ss.str();
    ss_path /= hebench::Utilities::convertToDirectoryName(s_scheme_name);
    ss_path /= hebench::Utilities::convertToDirectoryName(s_security_name);
    ss_path /= std::to_string(bench_desc.other);

    // generate header
    ss = std::stringstream();
    ss << "Specifications," << std::endl
       << ", Encryption, " << std::endl
       << ", , Scheme, " << s_scheme_name << std::endl
       << ", , Security, " << s_security_name << std::endl
       << ", Extra, " << bench_desc.other << std::endl;
    std::string s_tmp = engine.getExtraDescription(h_bench_desc, w_params);
    if (!s_tmp.empty())
        ss << s_tmp;
    ss << std::endl
       << std::endl
       << ", Category, " << PartialBenchmarkDescription::getCategoryName(bench_desc.category) << std::endl;
    switch (bench_desc.category)
    {
    case hebench::APIBridge::Category::Latency:
        ss << ", , Warmup iterations, " << bench_desc.cat_params.latency.warmup_iterations_count << std::endl
           << ", , Minimum test time requested (ms), " << bench_desc.cat_params.latency.min_test_time_ms << std::endl;
        break;

    case hebench::APIBridge::Category::Offline:
    {
        ss << ", , Parameter, Samples requested" << std::endl;

        bool all_params_zero = true;
        for (std::size_t i = 0; i < HEBENCH_MAX_OP_PARAMS; ++i)
        {
            if (bench_desc.cat_params.offline.data_count[i] != 0)
            {
                all_params_zero = false;
                ss << ", , " << i << ", " << bench_desc.cat_params.offline.data_count[i] << std::endl;
            } // end if
        } // end if
        if (all_params_zero)
            ss << ", , All, 0" << std::endl;
    }
    break;

    default:
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Unsupported benchmark category: " + std::to_string(bench_desc.category) + "."));
        break;
    } // end switch

    ss << std::endl
       << ", Workload, " << s_workload_name << std::endl
       << ", , Data type, " << PartialBenchmarkDescription::getDataTypeName(bench_desc.data_type) << std::endl
       << ", , Encrypted op parameters (index)";
    if (cipher_param_pos.empty())
        ss << ", None" << std::endl;
    else if (cipher_param_pos.size() >= sizeof(std::uint32_t) * 8)
        ss << ", All" << std::endl;
    else
    {
        // output the indices of the encrypted parameters
        std::vector<std::size_t> cipher_params(cipher_param_pos.begin(), cipher_param_pos.end());
        std::sort(cipher_params.begin(), cipher_params.end());
        for (auto param_index : cipher_params)
            ss << ", " << param_index;
        ss << std::endl;
    } // end else

    description.header = ss.str();
    description.path   = ss_path;

    completeDescription(engine, pre_token);
}

//------------------------
// class PartialBenchmark
//------------------------

PartialBenchmark::PartialBenchmark(std::shared_ptr<Engine> p_engine,
                                   const IBenchmarkDescription::DescriptionToken &description_token) :
    m_descriptor(m_benchmark_descriptor),
    m_params(m_workload_params),
    m_benchmark_configuration(m_bench_config),
    m_current_event_id(0),
    m_b_constructed(false),
    m_b_initialized(false)
{
    if (!p_engine)
        throw std::invalid_argument(IL_LOG_MSG_CLASS("Invalid null argument 'p_engine'."));

    m_p_engine = p_engine; // this prevents engine from being destroyed while this object exists
    std::memset(&m_handle, 0, sizeof(m_handle));

    internalInit(description_token);

    m_b_constructed = true;
}

PartialBenchmark::~PartialBenchmark()
{
    // destroy benchmark handle
    hebench::APIBridge::destroyHandle(m_handle);
    // decouple from engine
    m_p_engine.reset(); // this object no longer prevents engine from being destroyed
}

void PartialBenchmark::internalInit(const IBenchmarkDescription::DescriptionToken &description_token)
{
    // cache description info about this benchmark
    m_h_descriptor         = description_token.getDescriptorHandle(description_token.getCaller(m_key_adk));
    m_benchmark_descriptor = description_token.getDescriptor(description_token.getCaller(m_key_adk));
    m_workload_params      = description_token.getWorkloadParams(description_token.getCaller(m_key_adk));
    m_bench_config         = description_token.getBenchmarkConfiguration(description_token.getCaller(m_key_adk));
}

void PartialBenchmark::postInit()
{
    m_current_event_id = getEventIDStart();
    m_b_initialized    = true;
}

void PartialBenchmark::initBackend(hebench::Utilities::TimingReportEx &out_report, const FriendPrivateKey &)
{
    hebench::Common::EventTimer timer;
    hebench::Common::TimingReportEvent::Ptr p_timing_event;

    hebench::APIBridge::WorkloadParams params;
    params.count  = m_workload_params.size();
    params.params = m_workload_params.data();

    std::cout << IOS_MSG_INFO << hebench::Logging::GlobalLogger::log("Initializing backend benchmark...") << std::endl;
    timer.start();
    validateRetCode(hebench::APIBridge::initBenchmark(m_p_engine->handle(),
                                                      m_h_descriptor,
                                                      m_workload_params.empty() ? nullptr : &params,
                                                      &m_handle));
    p_timing_event = timer.stop<DefaultTimeInterval>(getEventIDNext(), 1, nullptr);
    out_report.addEvent<DefaultTimeInterval>(p_timing_event, std::string("Initialization"));
    hebench::Logging::GlobalLogger::log("OK");
    std::cout << IOS_MSG_OK << std::endl;
    std::stringstream ss = std::stringstream();
    ss << "Elapsed wall time: " << p_timing_event->elapsedWallTime<std::milli>() << " ms";
    std::cout << IOS_MSG_INFO << hebench::Logging::GlobalLogger::log(ss.str()) << std::endl;
    ss = std::stringstream();
    ss << "Elapsed CPU time: " << p_timing_event->elapsedCPUTime<std::milli>() << " ms";
    std::cout << IOS_MSG_INFO << hebench::Logging::GlobalLogger::log(ss.str()) << std::endl;
}

void PartialBenchmark::checkInitializationState(const FriendPrivateKey &) const
{
    if (!m_b_initialized)
        throw std::runtime_error(IL_LOG_MSG_CLASS("Initialization incomplete. All initialization stages must be called: init(), initBackend(), postInit()."));
}

void PartialBenchmark::validateRetCode(hebench::APIBridge::ErrorCode err_code, bool last_error) const
{
    m_p_engine->validateRetCode(err_code, last_error);
}

} // namespace TestHarness
} // namespace hebench
