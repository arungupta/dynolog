/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "dynolog/src/Logger.h"

#ifdef USE_OTLP
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#endif

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef USE_OTLP

DECLARE_string(otlp_endpoint);
DECLARE_string(otlp_protocol);
DECLARE_string(otlp_service_name);
DECLARE_string(otlp_headers);
DECLARE_int32(otlp_timeout_ms);
DECLARE_int32(otlp_export_interval_ms);
DECLARE_bool(otlp_use_tls);

namespace dynolog {

// Describes how a dynolog metric key maps to an OTEL metric.
struct OTLPMetricMapping {
  std::string otel_name; // e.g. "system.cpu.utilization"
  std::string unit; // e.g. "1", "ms", "By"
  // Static attributes to attach (e.g. state=user, direction=receive)
  std::vector<std::pair<std::string, std::string>> attributes;
  // If true, divide the raw value by 100 (percent -> ratio)
  bool divide_by_100 = false;
  // If true, divide the raw value by 1000 (ms -> s)
  bool divide_by_1000 = false;
  // If true, this key should not be exported (metadata only)
  bool skip = false;
};

// Result of parsing a dynolog metric key.
struct ParsedMetricKey {
  OTLPMetricMapping mapping;
  // Dynamic attributes extracted from the key (e.g. device name, socket id)
  std::vector<std::pair<std::string, std::string>> dynamic_attributes;
  bool matched = false; // true if key was recognized
};

// Parse a dynolog metric key into OTEL metric info.
// Exposed for testing.
ParsedMetricKey parseMetricKey(const std::string& key);

// Exposed for testing.
std::string getEnvOr(const char* name, const std::string& fallback);
std::vector<std::pair<std::string, std::string>> parseHeaders(
    const std::string& raw);

// OTLPManager is a process-wide singleton that owns the OTEL MeterProvider and
// instrument caches. A single mutex serializes metric recording across all
// collector threads (kernel, perf, GPU); actual network I/O is handled
// asynchronously by the PeriodicExportingMetricReader on its own thread.
// finalize() does not call forceFlush(); the reader exports on its configured
// interval. forceFlush() is available for explicit use (e.g. shutdown) but is
// not called in the normal collection path.
class OTLPManager {
 public:
  struct LoggingGuard {
    std::shared_ptr<OTLPManager> manager;
    std::lock_guard<std::mutex> lock_guard;
  };

  OTLPManager();

  // Record a gauge value with attributes.
  void logGauge(
      const std::string& name,
      const std::string& unit,
      double val,
      const std::vector<std::pair<std::string, std::string>>& attributes);

  // Force flush all pending metrics to the exporter.
  void forceFlush();

  static LoggingGuard singleton();
  static bool isInitialized();

 private:
  std::lock_guard<std::mutex> lock() {
    return std::lock_guard{mutex_};
  }

  int32_t flush_timeout_ms_ = 30000;
  std::mutex mutex_;

  std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> provider_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter_;

  // Instrument caches keyed by "name:unit"
  std::unordered_map<
      std::string,
      opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Gauge<double>>>
      gauges_;

  // Should match googletest/include/gtest/gtest_prod.h
  friend class OTLPLoggerTest_MetricMappingCPU_Test;
  friend class OTLPLoggerTest_MetricMappingNetwork_Test;
  friend class OTLPLoggerTest_MetricMappingGPU_Test;
  friend class OTLPLoggerTest_OTLPManagerConstructionGRPC_Test;
  friend class OTLPLoggerTest_OTLPManagerConstructionHTTP_Test;
  friend class OTLPLoggerTest_OTLPManagerForceFlush_Test;
  friend class OTLPLoggerTest_OTLPManagerLogGauge_Test;
  friend class OTLPLoggerTest_OTLPManagerExportIntervalValidation_Test;
};

class OTLPLogger : public Logger {
 public:
  // OTel SDK handles timestamps.
  void setTimestamp(Timestamp /*ts*/) override {}

  void logInt(const std::string& key, int64_t val) override {
    pending_metrics_[key] = static_cast<double>(val);
  }

  void logFloat(const std::string& key, float val) override {
    pending_metrics_[key] = static_cast<double>(val);
  }

  void logUint(const std::string& key, uint64_t val) override {
    pending_metrics_[key] = static_cast<double>(val);
  }

  void logStr(const std::string& key, const std::string& val) override;

  void finalize() override;

 private:
  // Accumulated numeric metrics for this collection cycle.
  std::unordered_map<std::string, double> pending_metrics_;
  // String attributes (e.g. Slurm job_id, username) for this cycle.
  std::unordered_map<std::string, std::string> pending_attributes_;

  friend class OTLPLoggerTest_BasicTest_Test;
  friend class OTLPLoggerTest_SlurmAttribution_Test;
  friend class OTLPLoggerTest_FinalizeEmpty_Test;
  friend class OTLPLoggerTest_FinalizeMultiple_Test;
  friend class OTLPLoggerTest_UnknownMetric_Test;
  friend class OTLPLoggerTest_LogStrNonAttribution_Test;
  friend class OTLPLoggerTest_NaNAndInfValues_Test;
  friend class OTLPLoggerTest_LargeUint64Values_Test;
  friend class OTLPLoggerTest_ZeroValues_Test;
  friend class OTLPLoggerTest_NegativeValues_Test;
  friend class OTLPLoggerTest_OverwriteMetricValue_Test;
  friend class OTLPLoggerTest_OverwriteStringAttribute_Test;
  friend class OTLPLoggerTest_EmptyStringLogStr_Test;
  friend class OTLPLoggerTest_MixedMetricTypes_Test;
  friend class OTLPLoggerTest_SetTimestampIsNoOp_Test;
  friend class OTLPLoggerTest_DeviceIdExtraction_Test;
  friend class OTLPLoggerTest_GPUAttributeMerging_Test;
  friend class OTLPLoggerTest_FinalizeClearsState_Test;
  friend class OTLPLoggerTest_CompleteMetricFlowWithoutNetwork_Test;
  friend class OTLPLoggerTest_MultiThreadedIsolation_Test;
};

} // namespace dynolog
#endif // USE_OTLP
