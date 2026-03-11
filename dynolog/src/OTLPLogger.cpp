/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "dynolog/src/OTLPLogger.h"
#include "hbt/src/common/System.h"

#include <fmt/format.h>
#include <glog/logging.h>

#ifdef USE_OTLP
#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/view_registry.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/resource/resource.h>

#include <cmath>
#include <cstdlib>
#include <map>
#include <regex>
#include <unordered_set>

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace otlp_exp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;
namespace metrics_api = opentelemetry::metrics;

DEFINE_string(
    otlp_endpoint,
    "",
    "OTLP endpoint (e.g., http://localhost:4317 for gRPC, "
    "http://localhost:4318/v1/metrics for HTTP)");
DEFINE_string(
    otlp_protocol,
    "grpc",
    "OTLP protocol: 'grpc' or 'http'");
DEFINE_string(
    otlp_service_name,
    "dynolog",
    "Service name for OTLP resource attributes");
DEFINE_string(
    otlp_headers,
    "",
    "OTLP headers (key1=value1,key2=value2)");
DEFINE_int32(otlp_timeout_ms, 30000, "OTLP export timeout in milliseconds");
DEFINE_int32(
    otlp_export_interval_ms,
    60000,
    "OTLP periodic export interval in milliseconds");
DEFINE_bool(otlp_use_tls, false, "Enable TLS for OTLP gRPC");

namespace dynolog {

// ---- Slurm attribution keys that become metric attributes ----
static const std::unordered_set<std::string> kSlurmAttributionKeys = {
    "job_id",
    "username",
    "slurm_account",
    "slurm_partition",
};

// ---- Helper to get env var with fallback ----
std::string getEnvOr(const char* name, const std::string& fallback) {
  const char* val = std::getenv(name);
  return (val && val[0] != '\0') ? std::string(val) : fallback;
}

// ---- Parse OTLP headers from comma-separated key=value string ----
std::vector<std::pair<std::string, std::string>> parseHeaders(
    const std::string& raw) {
  std::vector<std::pair<std::string, std::string>> result;
  if (raw.empty()) {
    return result;
  }
  std::string::size_type start = 0;
  while (start < raw.size()) {
    auto comma = raw.find(',', start);
    auto token =
        raw.substr(start, comma == std::string::npos ? comma : comma - start);
    auto eq = token.find('=');
    if (eq != std::string::npos && eq > 0) {
      result.emplace_back(token.substr(0, eq), token.substr(eq + 1));
    }
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  return result;
}

// ---- Metric key parsing ----

// CPU utilization metrics that need divide-by-100.
static const std::unordered_map<std::string, std::string> kCpuUtilMetrics = {
    {"cpu_util", "active"},
    {"cpu_u", "user"},
    {"cpu_s", "system"},
    {"cpu_i", "idle"},
    {"cpu_guest", "guest"},
    {"cpu_guest_nice", "guest_nice"},
};

// CPU time metrics — dynolog reports per-interval deltas, exported as gauges.
static const std::unordered_map<std::string, std::string> kCpuTimeMetrics = {
    {"cpu_u_ms", "user"},
    {"cpu_s_ms", "system"},
    {"cpu_n_ms", "nice"},
    {"cpu_w_ms", "iowait"},
    {"cpu_x_ms", "irq"},
    {"cpu_y_ms", "softirq"},
    {"cpu_z_ms", "steal"},
    {"cpu_guest_ms", "guest"},
    {"cpu_guest_nice_ms", "guest_nice"},
};

// Network metric base names -> OTEL names.
struct NetworkMetricInfo {
  std::string otel_name;
  std::string unit;
  std::string direction;
};

static const std::unordered_map<std::string, NetworkMetricInfo>
    kNetworkMetrics = {
        {"rx_bytes",
         {"system.network.io", "By", "receive"}},
        {"tx_bytes",
         {"system.network.io", "By", "transmit"}},
        {"rx_packets",
         {"system.network.packet.count", "{packet}", "receive"}},
        {"tx_packets",
         {"system.network.packet.count", "{packet}", "transmit"}},
        {"rx_errors",
         {"system.network.errors", "{error}", "receive"}},
        {"tx_errors",
         {"system.network.errors", "{error}", "transmit"}},
        {"rx_drops",
         {"system.network.packet.dropped", "{packet}", "receive"}},
        {"tx_drops",
         {"system.network.packet.dropped", "{packet}", "transmit"}},
};

// ARM hardware counter metrics mapping.
struct ArmHwCounterMetricInfo {
  std::string otel_name;
  std::string unit;
  std::vector<std::pair<std::string, std::string>> extra_attrs;
};

static const std::unordered_map<std::string, ArmHwCounterMetricInfo>
    kArmHwCounterMetrics = {
        {"l1d_tlb",
         {"system.cpu.tlb.operations",
          "{event}",
          {{"level", "l1d"}, {"type", "access"}}}},
        {"l1d_tlb_refill",
         {"system.cpu.tlb.operations",
          "{event}",
          {{"level", "l1d"}, {"type", "miss"}}}},
        {"l1i_tlb",
         {"system.cpu.tlb.operations",
          "{event}",
          {{"level", "l1i"}, {"type", "access"}}}},
        {"l1i_tlb_refill",
         {"system.cpu.tlb.operations",
          "{event}",
          {{"level", "l1i"}, {"type", "miss"}}}},
        {"l2d_tlb",
         {"system.cpu.tlb.operations",
          "{event}",
          {{"level", "l2"}, {"type", "access"}}}},
        {"l2d_tlb_refill",
         {"system.cpu.tlb.operations",
          "{event}",
          {{"level", "l2"}, {"type", "miss"}}}},
        {"stall_backend_mem",
         {"system.cpu.stalls",
          "{cycle}",
          {{"reason", "backend_mem"}}}},
        {"ll_cache_miss_rd",
         {"system.cpu.cache.misses",
          "{event}",
          {{"level", "ll"}}}},
        {"br_mis_pred",
         {"system.cpu.branch.mispredictions",
          "{event}",
          {}}},
        {"br_retired",
         {"system.cpu.branch.instructions",
          "{instruction}",
          {}}},
        {"l1i_cache_refill",
         {"system.cpu.cache.operations",
          "{event}",
          {{"level", "l1i"}, {"type", "miss"}}}},
        {"l1d_cache_refill",
         {"system.cpu.cache.operations",
          "{event}",
          {{"level", "l1d"}, {"type", "miss"}}}},
        {"l2d_cache_refill",
         {"system.cpu.cache.operations",
          "{event}",
          {{"level", "l2"}, {"type", "miss"}}}},
        {"l3d_cache_refill",
         {"system.cpu.cache.operations",
          "{event}",
          {{"level", "l3"}, {"type", "miss"}}}},
        {"FP_HP_SPEC",
         {"system.cpu.fp.operations",
          "{operation}",
          {{"precision", "half"}}}},
        {"FP_SP_SPEC",
         {"system.cpu.fp.operations",
          "{operation}",
          {{"precision", "single"}}}},
        {"FP_DP_SPEC",
         {"system.cpu.fp.operations",
          "{operation}",
          {{"precision", "double"}}}},
        {"dtlb_walk",
         {"system.cpu.tlb.walks",
          "{event}",
          {{"type", "data"}}}},
        {"itlb_walk",
         {"system.cpu.tlb.walks",
          "{event}",
          {{"type", "instruction"}}}},
};

// GPU metrics mapping.
struct GPUMetricInfo {
  std::string otel_name;
  std::string unit;
  std::vector<std::pair<std::string, std::string>> extra_attrs;
  bool skip = false;
  bool divide_by_100 = false;
};

static const std::unordered_map<std::string, GPUMetricInfo> kGpuMetrics = {
    {"graphics_engine_active_ratio",
     {"hw.gpu.utilization",
      "1",
      {{"hw.gpu.task", "general"}}}},
    {"sm_active_ratio",
     {"hw.gpu.sm.utilization", "1", {}}},
    {"sm_occupancy",
     {"hw.gpu.sm.occupancy", "1", {}}},
    {"gpu_frequency_mhz",
     {"hw.gpu.frequency", "MHz", {}}},
    {"fp16_active",
     {"hw.gpu.pipe.utilization",
      "1",
      {{"pipe", "fp16"}}}},
    {"fp32_active",
     {"hw.gpu.pipe.utilization",
      "1",
      {{"pipe", "fp32"}}}},
    {"fp64_active",
     {"hw.gpu.pipe.utilization",
      "1",
      {{"pipe", "fp64"}}}},
    {"tensorcore_active",
     {"hw.gpu.pipe.utilization",
      "1",
      {{"pipe", "tensorcore"}}}},
    {"hbm_mem_bw_util",
     {"hw.gpu.memory.bandwidth.utilization", "1", {}}},
    {"pcie_tx_bytes",
     {"hw.gpu.io",
      "By/s",
      {{"direction", "transmit"}}}},
    {"pcie_rx_bytes",
     {"hw.gpu.io",
      "By/s",
      {{"direction", "receive"}}}},
    {"nvlink_tx_bytes",
     {"hw.gpu.nvlink.io",
      "By/s",
      {{"direction", "transmit"}}}},
    {"nvlink_rx_bytes",
     {"hw.gpu.nvlink.io",
      "By/s",
      {{"direction", "receive"}}}},
    {"gpu_device_utilization",
     {"hw.gpu.utilization",
      "1",
      {{"hw.gpu.task", "device"}},
      /* skip */ false,
      /* divide_by_100 */ true}},
    {"gpu_memory_utilization",
     {"hw.gpu.memory.utilization",
      "1",
      {},
      /* skip */ false,
      /* divide_by_100 */ true}},
    {"gpu_power_draw",
     {"hw.power",
      "W",
      {{"hw.type", "gpu"}}}},
    {"dcgm_error",
     {"hw.errors",
      "{error}",
      {{"hw.type", "gpu"}}}},
    // Metadata keys: skip export
    {"minor_id",
     {"", "", {}, /* skip */ true}},
    {"device",
     {"", "", {}, /* skip */ true}},
};

ParsedMetricKey parseMetricKey(const std::string& key) {
  ParsedMetricKey result;
  result.matched = false;

  if (key.empty()) {
    return result;
  }

  // 1. Check CPU utilization metrics.
  {
    auto it = kCpuUtilMetrics.find(key);
    if (it != kCpuUtilMetrics.end()) {
      result.matched = true;
      result.mapping.otel_name = "system.cpu.utilization";
      result.mapping.unit = "1";

      result.mapping.attributes = {{"cpu.mode", it->second}};
      result.mapping.divide_by_100 = true;
      return result;
    }
  }

  // 2. Check per-NUMA-socket CPU utilization: cpu_{u,s,i}_node{N}
  {
    static const std::regex kNumaRegex(
        R"(cpu_(u|s|i)_node(\d+))");
    std::smatch match;
    if (std::regex_match(key, match, kNumaRegex)) {
      result.matched = true;
      result.mapping.otel_name = "system.cpu.utilization";
      result.mapping.unit = "1";

      result.mapping.divide_by_100 = true;

      const std::string& mode = match[1].str();
      std::string state;
      if (mode == "u") {
        state = "user";
      } else if (mode == "s") {
        state = "system";
      } else {
        state = "idle";
      }
      result.mapping.attributes = {{"cpu.mode", state}};
      result.dynamic_attributes = {{"cpu.socket", match[2].str()}};
      return result;
    }
  }

  // 3. Check CPU time metrics.
  {
    auto it = kCpuTimeMetrics.find(key);
    if (it != kCpuTimeMetrics.end()) {
      result.matched = true;
      result.mapping.otel_name = "system.cpu.time";
      result.mapping.unit = "s";
      result.mapping.divide_by_1000 = true;

      result.mapping.attributes = {{"cpu.mode", it->second}};
      return result;
    }
  }

  // 4. Check special CPU metrics.
  if (key == "uptime") {
    result.matched = true;
    result.mapping.otel_name = "system.uptime";
    result.mapping.unit = "s";

    return result;
  }
  // 5. Check network metrics (key format: "metric_base.device_name").
  {
    auto dot = key.find('.');
    if (dot != std::string::npos) {
      std::string base = key.substr(0, dot);
      std::string device = key.substr(dot + 1);

      auto it = kNetworkMetrics.find(base);
      if (it != kNetworkMetrics.end()) {
        result.matched = true;
        result.mapping.otel_name = it->second.otel_name;
        result.mapping.unit = it->second.unit;

        result.mapping.attributes = {
            {"network.io.direction",
             it->second.direction}};
        if (!device.empty()) {
          result.dynamic_attributes = {{"network.interface.name", device}};
        }
        return result;
      }
    }
  }

  // 6. Check GPU metrics.
  {
    auto it = kGpuMetrics.find(key);
    if (it != kGpuMetrics.end()) {
      result.matched = true;
      result.mapping.otel_name = it->second.otel_name;
      result.mapping.unit = it->second.unit;

      result.mapping.attributes = it->second.extra_attrs;
      result.mapping.skip = it->second.skip;
      result.mapping.divide_by_100 = it->second.divide_by_100;
      return result;
    }
  }

  // 7. Check ARM hardware counter metrics.
  {
    auto it = kArmHwCounterMetrics.find(key);
    if (it != kArmHwCounterMetrics.end()) {
      result.matched = true;
      result.mapping.otel_name = it->second.otel_name;
      result.mapping.unit = it->second.unit;

      result.mapping.attributes = it->second.extra_attrs;
      return result;
    }
  }

  // 8. Unrecognized key: pass through with dynolog.* prefix.
  result.matched = true;
  result.mapping.otel_name = fmt::format("dynolog.{}", key);
  result.mapping.unit = "";
  return result;
}

// ---- OTLPManager implementation ----

OTLPManager::OTLPManager() {
  try {
    // Resolve configuration with env var overrides.
    std::string endpoint = getEnvOr(
        "OTEL_EXPORTER_OTLP_ENDPOINT",
        FLAGS_otlp_endpoint);
    std::string protocol = getEnvOr(
        "OTEL_EXPORTER_OTLP_PROTOCOL",
        FLAGS_otlp_protocol);
    std::string service_name = getEnvOr(
        "OTEL_SERVICE_NAME",
        FLAGS_otlp_service_name);
    std::string headers_str = getEnvOr(
        "OTEL_EXPORTER_OTLP_HEADERS",
        FLAGS_otlp_headers);

    // Validate export timing parameters.
    int32_t interval_ms = FLAGS_otlp_export_interval_ms;
    if (interval_ms <= 0) {
      LOG(WARNING) << "otlp_export_interval_ms=" << interval_ms
                   << " is invalid (must be > 0); using default 60000ms";
      interval_ms = 60000;
    }
    int32_t timeout_ms = FLAGS_otlp_timeout_ms;
    if (timeout_ms <= 0) {
      LOG(WARNING) << "otlp_timeout_ms=" << timeout_ms
                   << " is invalid (must be > 0); using default 30000ms";
      timeout_ms = 30000;
    }
    if (timeout_ms >= interval_ms) {
      LOG(WARNING) << "otlp_timeout_ms (" << timeout_ms
                   << ") >= otlp_export_interval_ms (" << interval_ms
                   << "); the OTEL SDK may reset both to defaults";
    }

    // Build resource attributes.
    std::string hostname;
    try {
      hostname = facebook::hbt::getHostName();
    } catch (...) {
      hostname = "unknown";
    }

    auto resource_attrs = resource::Resource::Create({
        {"service.name", service_name},
        {"service.version", DYNOLOG_VERSION},
        {"host.name", hostname},
        {"os.type", "linux"},
    });

    // Create exporter based on protocol.
    std::unique_ptr<metric_sdk::PushMetricExporter> exporter;

    if (protocol == "http") {
      otlp_exp::OtlpHttpMetricExporterOptions opts;
      if (!endpoint.empty()) {
        opts.url = endpoint;
      } else {
        opts.url = "http://localhost:4318/v1/metrics";
      }
      auto parsed_headers = parseHeaders(headers_str);
      for (const auto& [k, v] : parsed_headers) {
        opts.http_headers.insert({k, v});
      }
      opts.timeout = std::chrono::milliseconds(timeout_ms);

      LOG(INFO) << "OTLP HTTP exporter endpoint: " << opts.url;
      exporter = otlp_exp::OtlpHttpMetricExporterFactory::Create(opts);
    } else {
      if (protocol != "grpc") {
        LOG(WARNING) << "Unknown otlp_protocol '" << protocol
                     << "'; defaulting to gRPC";
      }
      // Default to gRPC.
      otlp_exp::OtlpGrpcMetricExporterOptions opts;
      if (!endpoint.empty()) {
        opts.endpoint = endpoint;
      } else {
        opts.endpoint = "http://localhost:4317";
      }
      opts.use_ssl_credentials = FLAGS_otlp_use_tls;
      auto parsed_headers = parseHeaders(headers_str);
      for (const auto& [k, v] : parsed_headers) {
        opts.metadata.insert({k, v});
      }
      opts.timeout = std::chrono::milliseconds(timeout_ms);

      LOG(INFO) << "OTLP gRPC exporter endpoint: " << opts.endpoint;
      exporter = otlp_exp::OtlpGrpcMetricExporterFactory::Create(opts);
    }

    if (!exporter) {
      LOG(ERROR) << "Failed to create OTLP exporter for protocol: " << protocol;
      provider_ = nullptr;
      meter_ = nullptr;
      return;
    }

    // Create periodic exporting metric reader.
    metric_sdk::PeriodicExportingMetricReaderOptions reader_opts;
    flush_timeout_ms_ = timeout_ms;
    reader_opts.export_interval_millis =
        std::chrono::milliseconds(interval_ms);
    reader_opts.export_timeout_millis =
        std::chrono::milliseconds(timeout_ms);

    auto reader =
        metric_sdk::PeriodicExportingMetricReaderFactory::Create(
            std::move(exporter), reader_opts);

    // Build MeterProvider with resource attributes.
    // Construct directly to pass resource attributes to the provider.
    auto context = std::make_unique<metric_sdk::MeterContext>(
        std::make_unique<metric_sdk::ViewRegistry>(), resource_attrs);
    context->AddMetricReader(std::move(reader));
    auto provider =
        std::make_shared<metric_sdk::MeterProvider>(std::move(context));

    // Store provider and get a meter.
    provider_ = provider;
    metrics_api::Provider::SetMeterProvider(
        std::shared_ptr<metrics_api::MeterProvider>(provider));
    meter_ = provider_->GetMeter("dynolog", DYNOLOG_VERSION);

    LOG(INFO) << "OTLPManager initialized with service.name=" << service_name
              << " host.name=" << hostname;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to initialize OTLP exporter: " << e.what();
    LOG(ERROR) << "OTLP metrics will not be exported.";
    provider_ = nullptr;
    meter_ = nullptr;
  }
}

void OTLPManager::logGauge(
    const std::string& name,
    const std::string& unit,
    double val,
    const std::vector<std::pair<std::string, std::string>>& attributes) {
  if (!provider_ || !meter_) {
    return;
  }
  if (std::isnan(val) || std::isinf(val)) {
    LOG_EVERY_N(WARNING, 100) << "Skipping NaN/Inf value for metric " << name;
    return;
  }

  std::string cache_key = fmt::format("{}:{}", name, unit);
  auto it = gauges_.find(cache_key);
  if (it == gauges_.end()) {
    auto gauge = meter_->CreateDoubleGauge(name, name, unit);
    if (!gauge) {
      LOG(ERROR) << "Failed to create OTLP gauge instrument: " << name;
      return;
    }
    auto [ins_it, _] = gauges_.emplace(cache_key, std::move(gauge));
    it = ins_it;
  }

  // Build attribute map.
  std::map<std::string, std::string> attr_map;
  for (const auto& [k, v] : attributes) {
    attr_map[k] = v;
  }

  it->second->Record(val, opentelemetry::common::KeyValueIterableView(attr_map),
                     opentelemetry::context::Context{});
}

void OTLPManager::forceFlush() {
  if (provider_) {
    bool ok = provider_->ForceFlush(
        std::chrono::milliseconds(flush_timeout_ms_));
    if (!ok) {
      LOG(WARNING) << "OTLPManager::forceFlush() timed out after "
                   << flush_timeout_ms_ << "ms";
    }
  }
}

static std::shared_ptr<OTLPManager> singleton_() {
  static std::shared_ptr<OTLPManager> manager_ =
      []() -> std::shared_ptr<OTLPManager> {
    try {
      return std::make_shared<OTLPManager>();
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to create OTLPManager: " << e.what()
                 << ". OTLP metrics will not be exported.";
      return nullptr;
    }
  }();
  return manager_;
}

// static
bool OTLPManager::isInitialized() {
  auto s = singleton_();
  return s != nullptr && s->provider_ != nullptr
      && s->meter_ != nullptr;
}

// static
OTLPManager::LoggingGuard OTLPManager::singleton() {
  auto s = singleton_();
  CHECK(s != nullptr)
      << "OTLPManager::singleton() called but initialization failed. "
      << "Call isInitialized() before singleton().";
  return LoggingGuard{.manager = s, .lock_guard = s->lock()};
}

// ---- OTLPLogger implementation ----

void OTLPLogger::logStr(const std::string& key, const std::string& val) {
  if (kSlurmAttributionKeys.count(key)) {
    pending_attributes_[key] = val;
  }
  // Non-attribution string keys are silently ignored
  // (same as PrometheusLogger).
}

void OTLPLogger::finalize() {
  if (pending_metrics_.empty()) {
    pending_attributes_.clear();
    return;
  }

  if (!OTLPManager::isInitialized()) {
    pending_metrics_.clear();
    pending_attributes_.clear();
    return;
  }

  auto logging_guard = OTLPManager::singleton();
  auto mgr = logging_guard.manager;

  // Look up the "device" key value for GPU hw.id attribute.
  std::string gpu_device_id;
  auto device_it = pending_metrics_.find("device");
  if (device_it != pending_metrics_.end()) {
    gpu_device_id = fmt::format("{}", static_cast<int64_t>(device_it->second));
  }

  for (const auto& [key, val] : pending_metrics_) {
    auto parsed = parseMetricKey(key);
    if (!parsed.matched || parsed.mapping.skip) {
      continue;
    }

    double export_val = val;
    if (parsed.mapping.divide_by_100) {
      export_val = val / 100.0;
    }
    if (parsed.mapping.divide_by_1000) {
      export_val = val / 1000.0;
    }

    // Merge all attributes: static from mapping +
    // dynamic from key + Slurm attrs.
    std::vector<std::pair<std::string, std::string>> all_attrs;
    all_attrs.insert(
        all_attrs.end(),
        parsed.mapping.attributes.begin(),
        parsed.mapping.attributes.end());
    all_attrs.insert(
        all_attrs.end(),
        parsed.dynamic_attributes.begin(),
        parsed.dynamic_attributes.end());

    // Add hw.id for GPU metrics if we have a device
    // id and this is a GPU metric.
    bool is_gpu_metric =
        parsed.mapping.otel_name.find("hw.") == 0;
    if (is_gpu_metric && !gpu_device_id.empty()) {
      all_attrs.emplace_back("hw.id", gpu_device_id);
    }

    // Attach Slurm attribution attributes to GPU metrics.
    if (is_gpu_metric) {
      for (const auto& [attr_key, attr_val] : pending_attributes_) {
        all_attrs.emplace_back(attr_key, attr_val);
      }
    }

    mgr->logGauge(
        parsed.mapping.otel_name,
        parsed.mapping.unit,
        export_val,
        all_attrs);
  }

  pending_metrics_.clear();
  pending_attributes_.clear();
}

} // namespace dynolog
#endif // USE_OTLP
