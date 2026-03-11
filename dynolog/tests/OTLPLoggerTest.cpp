/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "dynolog/src/OTLPLogger.h"
#include <fmt/format.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>
#include <thread>

#ifdef OTLP_INTEGRATION_TEST
#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <fstream>
#include <sstream>
#endif // OTLP_INTEGRATION_TEST

namespace dynolog {

// =============================================================================
// Metric Key Parsing Tests
// =============================================================================

// ---- CPU Utilization Metrics (positive) ----

TEST(OTLPLoggerTest, MetricMappingCPU) {
  // cpu_util -> system.cpu.utilization, cpu.mode=active, divide_by_100
  {
    auto parsed = parseMetricKey("cpu_util");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_TRUE(parsed.mapping.divide_by_100);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "cpu.mode");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "active");
  }

  // cpu_u -> system.cpu.utilization, cpu.mode=user
  {
    auto parsed = parseMetricKey("cpu_u");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "user");
  }

  // cpu_s -> cpu.mode=system
  {
    auto parsed = parseMetricKey("cpu_s");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "system");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
  }

  // cpu_i -> cpu.mode=idle
  {
    auto parsed = parseMetricKey("cpu_i");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "idle");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
  }

  // cpu_guest -> cpu.mode=guest
  {
    auto parsed = parseMetricKey("cpu_guest");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "guest");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
  }

  // cpu_guest_nice -> cpu.mode=guest_nice
  {
    auto parsed = parseMetricKey("cpu_guest_nice");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "guest_nice");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
  }
}

// ---- CPU Time Metrics (per-interval delta, Gauge) ----

TEST(OTLPLoggerTest, MetricMappingCPUTime) {
  // cpu_u_ms -> system.cpu.time, cpu.mode=user, Gauge
  {
    auto parsed = parseMetricKey("cpu_u_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.unit, "s");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "user");
  }

  // cpu_s_ms -> cpu.mode=system
  {
    auto parsed = parseMetricKey("cpu_s_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");

    EXPECT_EQ(parsed.mapping.attributes[0].second, "system");
  }

  // cpu_n_ms -> cpu.mode=nice
  {
    auto parsed = parseMetricKey("cpu_n_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "nice");
  }

  // cpu_w_ms -> cpu.mode=iowait
  {
    auto parsed = parseMetricKey("cpu_w_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "iowait");
  }

  // cpu_x_ms -> cpu.mode=irq
  {
    auto parsed = parseMetricKey("cpu_x_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "irq");
  }

  // cpu_y_ms -> cpu.mode=softirq
  {
    auto parsed = parseMetricKey("cpu_y_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "softirq");
  }

  // cpu_z_ms -> cpu.mode=steal
  {
    auto parsed = parseMetricKey("cpu_z_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "steal");
  }

  // cpu_guest_ms -> cpu.mode=guest
  {
    auto parsed = parseMetricKey("cpu_guest_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "guest");
  }

  // cpu_guest_nice_ms -> cpu.mode=guest_nice
  {
    auto parsed = parseMetricKey("cpu_guest_nice_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "guest_nice");
  }
}

// ---- Special CPU Metrics ----

TEST(OTLPLoggerTest, MetricMappingSpecialCPU) {
  // uptime -> system.uptime, Gauge, unit=s
  {
    auto parsed = parseMetricKey("uptime");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.uptime");
    EXPECT_EQ(parsed.mapping.unit, "s");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }

  // mips -> dynolog.mips (passthrough, no unit conversion)
  {
    auto parsed = parseMetricKey("mips");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "dynolog.mips");
    EXPECT_EQ(parsed.mapping.unit, "");
  }

  // mega_cycles_per_second -> dynolog.mega_cycles_per_second
  {
    auto parsed = parseMetricKey("mega_cycles_per_second");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(
        parsed.mapping.otel_name,
        "dynolog.mega_cycles_per_second");
    EXPECT_EQ(parsed.mapping.unit, "");
  }
}

// ---- Per-NUMA-socket CPU metrics ----

TEST(OTLPLoggerTest, MetricMappingNUMA) {
  // cpu_u_node0 -> system.cpu.utilization, cpu.mode=user, cpu.socket=0
  {
    auto parsed = parseMetricKey("cpu_u_node0");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "cpu.mode");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "user");
    ASSERT_EQ(parsed.dynamic_attributes.size(), 1);
    EXPECT_EQ(parsed.dynamic_attributes[0].first, "cpu.socket");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "0");
  }

  // cpu_s_node3 -> cpu.mode=system, cpu.socket=3
  {
    auto parsed = parseMetricKey("cpu_s_node3");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "system");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "3");
  }

  // cpu_i_node7 -> cpu.mode=idle, cpu.socket=7
  {
    auto parsed = parseMetricKey("cpu_i_node7");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.attributes[0].second, "idle");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "7");
  }

  // Large socket number
  {
    auto parsed = parseMetricKey("cpu_u_node127");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "127");
  }
}

// ---- Network Metrics ----

TEST(OTLPLoggerTest, MetricMappingNetwork) {
  // rx_bytes.eth0 -> system.network.io,
  // network.io.direction=receive, network.interface.name=eth0
  {
    auto parsed = parseMetricKey("rx_bytes.eth0");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.io");
    EXPECT_EQ(parsed.mapping.unit, "By");

    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "network.io.direction");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "receive");
    ASSERT_EQ(parsed.dynamic_attributes.size(), 1);
    EXPECT_EQ(parsed.dynamic_attributes[0].first, "network.interface.name");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "eth0");
  }

  // tx_bytes.ens5 -> system.network.io, network.io.direction=transmit
  {
    auto parsed = parseMetricKey("tx_bytes.ens5");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.io");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "transmit");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "ens5");
  }

  // rx_packets.lo -> system.network.packet.count, network.io.direction=receive
  {
    auto parsed = parseMetricKey("rx_packets.lo");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.packet.count");
    EXPECT_EQ(parsed.mapping.unit, "{packet}");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "receive");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "lo");
  }

  // tx_packets.wlan0
  {
    auto parsed = parseMetricKey("tx_packets.wlan0");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.packet.count");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "transmit");
  }

  // rx_errors.eth1 -> system.network.errors
  {
    auto parsed = parseMetricKey("rx_errors.eth1");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.errors");
    EXPECT_EQ(parsed.mapping.unit, "{error}");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "receive");
  }

  // tx_errors.eth1 -> system.network.errors
  {
    auto parsed = parseMetricKey("tx_errors.eth1");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.errors");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "transmit");
  }

  // rx_drops.bond0 -> system.network.packet.dropped
  {
    auto parsed = parseMetricKey("rx_drops.bond0");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.packet.dropped");
    EXPECT_EQ(parsed.mapping.unit, "{packet}");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "receive");
  }

  // tx_drops.bond0 -> system.network.packet.dropped
  {
    auto parsed = parseMetricKey("tx_drops.bond0");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.packet.dropped");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "transmit");
  }

  // Network metric with dot in device name: rx_bytes.eth0.1
  // Should split on first dot: base="rx_bytes", device="eth0.1"
  {
    auto parsed = parseMetricKey("rx_bytes.eth0.1");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.io");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "eth0.1");
  }
}

// ---- GPU Metrics ----

TEST(OTLPLoggerTest, MetricMappingGPU) {
  // graphics_engine_active_ratio - profiling metric, already 0-1 ratio
  {
    auto parsed = parseMetricKey("graphics_engine_active_ratio");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    // Should have hw.gpu.task=general
    bool found_task = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.gpu.task" && v == "general") {
        found_task = true;
      }
    }
    EXPECT_TRUE(found_task);
  }

  // sm_active_ratio - profiling metric, already 0-1 ratio
  {
    auto parsed = parseMetricKey("sm_active_ratio");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.sm.utilization");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }

  // sm_occupancy
  {
    auto parsed = parseMetricKey("sm_occupancy");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.sm.occupancy");
  }

  // gpu_frequency_mhz
  {
    auto parsed = parseMetricKey("gpu_frequency_mhz");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.frequency");
    EXPECT_EQ(parsed.mapping.unit, "MHz");
  }

  // fp16_active -> pipe=fp16
  {
    auto parsed = parseMetricKey("fp16_active");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.pipe.utilization");
    bool found_pipe = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "pipe" && v == "fp16") {
        found_pipe = true;
      }
    }
    EXPECT_TRUE(found_pipe);
  }

  // fp32_active -> pipe=fp32
  {
    auto parsed = parseMetricKey("fp32_active");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.pipe.utilization");
    bool found_pipe = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "pipe" && v == "fp32") {
        found_pipe = true;
      }
    }
    EXPECT_TRUE(found_pipe);
  }

  // fp64_active -> pipe=fp64
  {
    auto parsed = parseMetricKey("fp64_active");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.pipe.utilization");
    bool found_pipe = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "pipe" && v == "fp64") {
        found_pipe = true;
      }
    }
    EXPECT_TRUE(found_pipe);
  }

  // tensorcore_active -> pipe=tensorcore
  {
    auto parsed = parseMetricKey("tensorcore_active");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.pipe.utilization");
    bool found_pipe = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "pipe" && v == "tensorcore") {
        found_pipe = true;
      }
    }
    EXPECT_TRUE(found_pipe);
  }

  // hbm_mem_bw_util
  {
    auto parsed = parseMetricKey("hbm_mem_bw_util");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.memory.bandwidth.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");
  }

  // pcie_tx_bytes -> direction=transmit
  {
    auto parsed = parseMetricKey("pcie_tx_bytes");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.io");
    EXPECT_EQ(parsed.mapping.unit, "By/s");
    bool found_dir = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "direction" && v == "transmit") {
        found_dir = true;
      }
    }
    EXPECT_TRUE(found_dir);
  }

  // pcie_rx_bytes -> direction=receive
  {
    auto parsed = parseMetricKey("pcie_rx_bytes");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.io");
    bool found_dir = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "direction" && v == "receive") {
        found_dir = true;
      }
    }
    EXPECT_TRUE(found_dir);
  }

  // nvlink_tx_bytes
  {
    auto parsed = parseMetricKey("nvlink_tx_bytes");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.nvlink.io");
    EXPECT_EQ(parsed.mapping.unit, "By/s");
  }

  // nvlink_rx_bytes
  {
    auto parsed = parseMetricKey("nvlink_rx_bytes");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.nvlink.io");
  }

  // gpu_device_utilization -> hw.gpu.task=device, DCGM field 203 returns 0-100%
  {
    auto parsed = parseMetricKey("gpu_device_utilization");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.utilization");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
    bool found_task = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.gpu.task" && v == "device") {
        found_task = true;
      }
    }
    EXPECT_TRUE(found_task);
  }

  // gpu_memory_utilization, DCGM field 204 returns 0-100%
  {
    auto parsed = parseMetricKey("gpu_memory_utilization");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.memory.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
  }

  // gpu_power_draw -> hw.power, W, hw.type=gpu
  {
    auto parsed = parseMetricKey("gpu_power_draw");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.power");
    EXPECT_EQ(parsed.mapping.unit, "W");
    bool found_type = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.type" && v == "gpu") {
        found_type = true;
      }
    }
    EXPECT_TRUE(found_type);
  }

  // dcgm_error -> hw.errors, hw.type=gpu
  {
    auto parsed = parseMetricKey("dcgm_error");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.errors");
    EXPECT_EQ(parsed.mapping.unit, "{error}");
    bool found_type = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.type" && v == "gpu") {
        found_type = true;
      }
    }
    EXPECT_TRUE(found_type);
  }

  // minor_id -> skip (metadata)
  {
    auto parsed = parseMetricKey("minor_id");
    ASSERT_TRUE(parsed.matched);
    EXPECT_TRUE(parsed.mapping.skip);
  }

  // device -> skip (metadata, used as hw.id value)
  {
    auto parsed = parseMetricKey("device");
    ASSERT_TRUE(parsed.matched);
    EXPECT_TRUE(parsed.mapping.skip);
  }
}

// =============================================================================
// ARM Hardware Counter Metric Mapping Tests
// =============================================================================

TEST(OTLPLoggerTest, MetricMappingArmBranchRetired) {
  auto parsed = parseMetricKey("br_retired");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.branch.instructions");
  EXPECT_EQ(parsed.mapping.unit, "{instruction}");
  EXPECT_FALSE(parsed.mapping.divide_by_100);
  EXPECT_FALSE(parsed.mapping.divide_by_1000);
  EXPECT_FALSE(parsed.mapping.skip);
  EXPECT_TRUE(parsed.mapping.attributes.empty());
  EXPECT_TRUE(parsed.dynamic_attributes.empty());
}

TEST(OTLPLoggerTest, MetricMappingArmCacheRefills) {
  // l1i_cache_refill -> level=l1i, type=miss
  {
    auto parsed = parseMetricKey("l1i_cache_refill");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.cache.operations");
    EXPECT_EQ(parsed.mapping.unit, "{event}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 2);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "level");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "l1i");
    EXPECT_EQ(parsed.mapping.attributes[1].first, "type");
    EXPECT_EQ(parsed.mapping.attributes[1].second, "miss");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // l1d_cache_refill -> level=l1d, type=miss
  {
    auto parsed = parseMetricKey("l1d_cache_refill");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.cache.operations");
    EXPECT_EQ(parsed.mapping.unit, "{event}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 2);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "level");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "l1d");
    EXPECT_EQ(parsed.mapping.attributes[1].first, "type");
    EXPECT_EQ(parsed.mapping.attributes[1].second, "miss");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // l2d_cache_refill -> level=l2, type=miss
  {
    auto parsed = parseMetricKey("l2d_cache_refill");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.cache.operations");
    EXPECT_EQ(parsed.mapping.unit, "{event}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 2);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "level");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "l2");
    EXPECT_EQ(parsed.mapping.attributes[1].first, "type");
    EXPECT_EQ(parsed.mapping.attributes[1].second, "miss");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // l3d_cache_refill -> level=l3, type=miss
  {
    auto parsed = parseMetricKey("l3d_cache_refill");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.cache.operations");
    EXPECT_EQ(parsed.mapping.unit, "{event}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 2);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "level");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "l3");
    EXPECT_EQ(parsed.mapping.attributes[1].first, "type");
    EXPECT_EQ(parsed.mapping.attributes[1].second, "miss");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }
}

TEST(OTLPLoggerTest, MetricMappingArmFPOperations) {
  // FP_HP_SPEC -> precision=half
  {
    auto parsed = parseMetricKey("FP_HP_SPEC");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.fp.operations");
    EXPECT_EQ(parsed.mapping.unit, "{operation}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "precision");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "half");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // FP_SP_SPEC -> precision=single
  {
    auto parsed = parseMetricKey("FP_SP_SPEC");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.fp.operations");
    EXPECT_EQ(parsed.mapping.unit, "{operation}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "precision");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "single");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // FP_DP_SPEC -> precision=double
  {
    auto parsed = parseMetricKey("FP_DP_SPEC");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.fp.operations");
    EXPECT_EQ(parsed.mapping.unit, "{operation}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "precision");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "double");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }
}

TEST(OTLPLoggerTest, MetricMappingArmTLBWalks) {
  // dtlb_walk -> type=data
  {
    auto parsed = parseMetricKey("dtlb_walk");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.tlb.walks");
    EXPECT_EQ(parsed.mapping.unit, "{event}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "type");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "data");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // itlb_walk -> type=instruction
  {
    auto parsed = parseMetricKey("itlb_walk");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.tlb.walks");
    EXPECT_EQ(parsed.mapping.unit, "{event}");
    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.divide_by_1000);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "type");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "instruction");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }
}

// =============================================================================
// OTLPLogger Class Tests (no exporter needed)
// =============================================================================

TEST(OTLPLoggerTest, BasicTest) {
  // Check that basic logger class is collecting values
  // from various log functions.
  OTLPLogger logger;

  logger.logInt("uptime", 10000);
  logger.logFloat("pi", 3.1457f);
  logger.logUint("cpu_util", 25);
  logger.logUint("rx_bytes.eth0", 42);

  auto& kvs = logger.pending_metrics_;
  EXPECT_DOUBLE_EQ(kvs["uptime"], 10000.0);
  EXPECT_NEAR(kvs["pi"], 3.1457, 0.001);
  EXPECT_DOUBLE_EQ(kvs["cpu_util"], 25.0);
  EXPECT_DOUBLE_EQ(kvs["rx_bytes.eth0"], 42.0);

  // DO NOT RUN finalize() to avoid sending data to OTLP exporter.
}

TEST(OTLPLoggerTest, SlurmAttribution) {
  // logStr with Slurm attribution keys should store them as pending attrs.
  OTLPLogger logger;

  logger.logStr("job_id", "12345");
  logger.logStr("username", "testuser");
  logger.logStr("slurm_account", "research");
  logger.logStr("slurm_partition", "gpu");

  auto& attrs = logger.pending_attributes_;
  EXPECT_EQ(attrs["job_id"], "12345");
  EXPECT_EQ(attrs["username"], "testuser");
  EXPECT_EQ(attrs["slurm_account"], "research");
  EXPECT_EQ(attrs["slurm_partition"], "gpu");
}

TEST(OTLPLoggerTest, LogStrNonAttribution) {
  // logStr with non-Slurm keys should be silently ignored.
  OTLPLogger logger;

  logger.logStr("some_random_key", "some_value");
  logger.logStr("hostname", "host1");

  auto& attrs = logger.pending_attributes_;
  EXPECT_EQ(attrs.count("some_random_key"), 0);
  EXPECT_EQ(attrs.count("hostname"), 0);
  EXPECT_TRUE(attrs.empty());
}

TEST(OTLPLoggerTest, FinalizeEmpty) {
  // finalize() with no pending metrics should be a no-op.
  OTLPLogger logger;

  // Set some string attrs but no numeric metrics.
  logger.logStr("job_id", "12345");

  // finalize() should not crash and should clear state.
  logger.finalize();

  EXPECT_TRUE(logger.pending_metrics_.empty());
  EXPECT_TRUE(logger.pending_attributes_.empty());
}

TEST(OTLPLoggerTest, FinalizeMultiple) {
  // Multiple finalize() calls should work. Each clears state.
  OTLPLogger logger;

  logger.logInt("uptime", 100);
  EXPECT_EQ(logger.pending_metrics_.size(), 1);
  // finalize() takes the !OTLPManager::isInitialized() early-return path
  // which clears both maps. This is safe in unit tests.
  logger.finalize();
  EXPECT_TRUE(logger.pending_metrics_.empty());

  // Second batch
  logger.logInt("uptime", 200);
  logger.logFloat("cpu_util", 50.0f);
  EXPECT_EQ(logger.pending_metrics_.size(), 2);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["uptime"], 200.0);
  EXPECT_NEAR(logger.pending_metrics_["cpu_util"], 50.0, 0.01);
}

// =============================================================================
// Negative Tests
// =============================================================================

TEST(OTLPLoggerTest, UnknownMetric) {
  // Unknown metric key should be passed through with dynolog.* prefix.
  {
    auto parsed = parseMetricKey("totally_unknown_metric");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "dynolog.totally_unknown_metric");

    EXPECT_FALSE(parsed.mapping.skip);
  }
}

TEST(OTLPLoggerTest, EmptyMetricKey) {
  // Empty key should not match.
  auto parsed = parseMetricKey("");
  EXPECT_FALSE(parsed.matched);
}

TEST(OTLPLoggerTest, EmptyDeviceNetwork) {
  // Network metric with empty device name: "rx_bytes."
  // The dot is present but device is empty.
  auto parsed = parseMetricKey("rx_bytes.");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "system.network.io");
  // device attribute should NOT be present (empty)
  EXPECT_TRUE(parsed.dynamic_attributes.empty());
}

TEST(OTLPLoggerTest, NaNAndInfValues) {
  // NaN and Inf should be handled gracefully by logger accumulation
  // (the filter happens in OTLPManager::logGauge, not in parsing).
  OTLPLogger logger;

  float nan_val = std::numeric_limits<float>::quiet_NaN();
  float inf_val = std::numeric_limits<float>::infinity();

  logger.logFloat("cpu_util", nan_val);
  EXPECT_TRUE(std::isnan(logger.pending_metrics_["cpu_util"]));

  logger.logFloat("uptime", inf_val);
  EXPECT_TRUE(std::isinf(logger.pending_metrics_["uptime"]));
}

TEST(OTLPLoggerTest, LargeUint64Values) {
  // Very large uint64 values may lose precision when cast to double.
  OTLPLogger logger;
  uint64_t large_val = std::numeric_limits<uint64_t>::max();
  logger.logUint("rx_bytes.eth0", large_val);

  // The value stored is a double cast, verify it does not crash.
  double stored = logger.pending_metrics_["rx_bytes.eth0"];
  EXPECT_GT(stored, 0.0);
  // Note: precision loss is expected for values > 2^53.
  // We just verify it does not crash or produce NaN.
  EXPECT_FALSE(std::isnan(stored));
}

TEST(OTLPLoggerTest, ZeroValues) {
  // Zero values should be handled normally.
  OTLPLogger logger;

  logger.logInt("cpu_util", 0);
  logger.logFloat("cpu_u", 0.0f);
  logger.logUint("rx_bytes.eth0", 0);

  EXPECT_DOUBLE_EQ(logger.pending_metrics_["cpu_util"], 0.0);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["cpu_u"], 0.0);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["rx_bytes.eth0"], 0.0);
}

TEST(OTLPLoggerTest, NegativeValues) {
  // Negative values for int metrics should be stored correctly.
  OTLPLogger logger;

  logger.logInt("some_metric", -42);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["some_metric"], -42.0);

  logger.logFloat("another", -3.14f);
  EXPECT_NEAR(logger.pending_metrics_["another"], -3.14, 0.01);
}

TEST(OTLPLoggerTest, PercentToRatioConversion) {
  // Verify that CPU utilization metrics are correctly
  // flagged for /100 conversion.
  auto parsed = parseMetricKey("cpu_util");
  ASSERT_TRUE(parsed.matched);
  EXPECT_TRUE(parsed.mapping.divide_by_100);

  // Simulate the conversion: 75.0 -> 0.75
  double raw = 75.0;
  double converted = raw / 100.0;
  EXPECT_DOUBLE_EQ(converted, 0.75);

  // CPU time metrics should NOT be divided by 100.
  auto time_parsed = parseMetricKey("cpu_u_ms");
  ASSERT_TRUE(time_parsed.matched);
  EXPECT_FALSE(time_parsed.mapping.divide_by_100);

  // uptime should NOT be divided.
  auto uptime_parsed = parseMetricKey("uptime");
  ASSERT_TRUE(uptime_parsed.matched);
  EXPECT_FALSE(uptime_parsed.mapping.divide_by_100);
}

TEST(OTLPLoggerTest, NetworkMetricUnknownBase) {
  // A dotted key where the base is not a known network metric should
  // fall through to the unknown handler.
  auto parsed = parseMetricKey("unknown_base.eth0");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.unknown_base.eth0");
}

TEST(OTLPLoggerTest, SetTimestampIsNoOp) {
  // setTimestamp should be a no-op (OTel SDK handles timestamps).
  OTLPLogger logger;
  auto now = std::chrono::system_clock::now();
  // Should not crash or change any state.
  logger.setTimestamp(now);
  EXPECT_TRUE(logger.pending_metrics_.empty());
  EXPECT_TRUE(logger.pending_attributes_.empty());
}

TEST(OTLPLoggerTest, OverwriteMetricValue) {
  // Logging the same key twice should overwrite.
  OTLPLogger logger;

  logger.logInt("uptime", 100);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["uptime"], 100.0);

  logger.logInt("uptime", 200);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["uptime"], 200.0);
}

TEST(OTLPLoggerTest, OverwriteStringAttribute) {
  // Logging the same Slurm key twice should overwrite.
  OTLPLogger logger;

  logger.logStr("job_id", "111");
  EXPECT_EQ(logger.pending_attributes_["job_id"], "111");

  logger.logStr("job_id", "222");
  EXPECT_EQ(logger.pending_attributes_["job_id"], "222");
}

TEST(OTLPLoggerTest, SpecialCharactersInKey) {
  // Keys with special characters should fall through to dynolog.* prefix.
  {
    auto parsed = parseMetricKey("my-metric_with.dots");
    ASSERT_TRUE(parsed.matched);
    // "my-metric_with" is not a known network base, so falls through.
    EXPECT_EQ(parsed.mapping.otel_name, "dynolog.my-metric_with.dots");
  }

  {
    auto parsed = parseMetricKey("metric:with:colons");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "dynolog.metric:with:colons");
  }
}

TEST(OTLPLoggerTest, UnicodeInKey) {
  // Unicode characters in metric key - should be passed through.
  auto parsed = parseMetricKey("m\xC3\xA9trique");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.m\xC3\xA9trique");
}

TEST(OTLPLoggerTest, WhitespaceOnlyKey) {
  // Whitespace-only key should be passed through to dynolog.*.
  auto parsed = parseMetricKey("   ");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.   ");
}

TEST(OTLPLoggerTest, VeryLongKey) {
  // Very long key should not crash.
  std::string long_key(10000, 'a');
  auto parsed = parseMetricKey(long_key);
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog." + long_key);
}

TEST(OTLPLoggerTest, EmptyStringLogStr) {
  // logStr with empty value should still store it for attribution keys.
  OTLPLogger logger;

  logger.logStr("job_id", "");
  EXPECT_EQ(logger.pending_attributes_["job_id"], "");

  // Non-attribution empty value should be ignored.
  logger.logStr("random", "");
  EXPECT_EQ(logger.pending_attributes_.count("random"), 0);
}

TEST(OTLPLoggerTest, MixedMetricTypes) {
  // Log a mix of int, float, uint for the same key - last write wins.
  OTLPLogger logger;

  logger.logInt("cpu_util", 50);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["cpu_util"], 50.0);

  logger.logFloat("cpu_util", 75.5f);
  EXPECT_NEAR(logger.pending_metrics_["cpu_util"], 75.5, 0.01);

  logger.logUint("cpu_util", 99);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["cpu_util"], 99.0);
}

// =============================================================================
// Finalize Logic Tests (without network/exporter)
// =============================================================================

TEST(OTLPLoggerTest, DeviceIdExtraction) {
  // Verify that logging "device" stores it in pending_metrics_ and that
  // parseMetricKey correctly identifies "device" as a
  // skip-flagged metadata key.
  // This tests the finalize() device-lookup logic indirectly.
  OTLPLogger logger;

  // Log GPU metrics along with the device metadata key.
  logger.logInt("device", 3);
  logger.logFloat("gpu_device_utilization", 85.0f);
  logger.logFloat("gpu_power_draw", 250.0f);

  // Verify device value is stored in pending_metrics_.
  ASSERT_EQ(logger.pending_metrics_.count("device"), 1);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["device"], 3.0);

  // Verify parseMetricKey marks "device" as skip (metadata-only).
  auto device_parsed = parseMetricKey("device");
  ASSERT_TRUE(device_parsed.matched);
  EXPECT_TRUE(device_parsed.mapping.skip);
  EXPECT_TRUE(device_parsed.mapping.otel_name.empty());

  // Verify that the device value can be formatted as finalize() does:
  // fmt::format("{}", static_cast<int64_t>(device_value))
  double device_val = logger.pending_metrics_["device"];
  std::string device_id = fmt::format("{}", static_cast<int64_t>(device_val));
  EXPECT_EQ(device_id, "3");

  // Also verify minor_id is skip-flagged.
  auto minor_parsed = parseMetricKey("minor_id");
  ASSERT_TRUE(minor_parsed.matched);
  EXPECT_TRUE(minor_parsed.mapping.skip);

  // Negative: non-metadata GPU metrics should NOT have skip=true.
  auto gpu_util_parsed = parseMetricKey("gpu_device_utilization");
  ASSERT_TRUE(gpu_util_parsed.matched);
  EXPECT_FALSE(gpu_util_parsed.mapping.skip);

  auto gpu_power_parsed = parseMetricKey("gpu_power_draw");
  ASSERT_TRUE(gpu_power_parsed.matched);
  EXPECT_FALSE(gpu_power_parsed.mapping.skip);

  // Edge case: device value of 0 (valid GPU index).
  logger.logInt("device", 0);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["device"], 0.0);
  std::string device_id_zero = fmt::format(
      "{}",
      static_cast<int64_t>(
          logger.pending_metrics_["device"]));
  EXPECT_EQ(device_id_zero, "0");

  // Edge case: negative device value (invalid but should not crash).
  logger.logInt("device", -1);
  std::string device_id_neg = fmt::format(
      "{}",
      static_cast<int64_t>(
          logger.pending_metrics_["device"]));
  EXPECT_EQ(device_id_neg, "-1");

  // Edge case: large device value.
  logger.logInt("device", 255);
  std::string device_id_large = fmt::format(
      "{}",
      static_cast<int64_t>(
          logger.pending_metrics_["device"]));
  EXPECT_EQ(device_id_large, "255");
}

TEST(OTLPLoggerTest, GPUAttributeMerging) {
  // Verify that GPU metrics + Slurm attribution + device metadata are stored
  // correctly and that parseMetricKey produces the right mapping for each.
  OTLPLogger logger;

  // Log Slurm attribution strings.
  logger.logStr("job_id", "12345");
  logger.logStr("username", "testuser");
  logger.logStr("slurm_account", "research");
  logger.logStr("slurm_partition", "gpu-cluster");

  // Log device metadata.
  logger.logInt("device", 2);

  // Log GPU metrics.
  logger.logFloat("gpu_device_utilization", 85.0f);
  logger.logFloat("gpu_memory_utilization", 60.0f);
  logger.logFloat("gpu_power_draw", 300.0f);
  logger.logFloat("graphics_engine_active_ratio", 0.75f);
  logger.logFloat("sm_active_ratio", 0.5f);
  logger.logFloat("pcie_tx_bytes", 1000000.0f);
  logger.logFloat("fp16_active", 0.3f);

  // Verify Slurm attribution strings are in pending_attributes_.
  ASSERT_EQ(logger.pending_attributes_.size(), 4);
  EXPECT_EQ(logger.pending_attributes_["job_id"], "12345");
  EXPECT_EQ(logger.pending_attributes_["username"], "testuser");
  EXPECT_EQ(logger.pending_attributes_["slurm_account"], "research");
  EXPECT_EQ(logger.pending_attributes_["slurm_partition"], "gpu-cluster");

  // Verify device value is in pending_metrics_.
  ASSERT_EQ(logger.pending_metrics_.count("device"), 1);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["device"], 2.0);

  // Verify gpu_device_utilization mapping.
  {
    auto parsed = parseMetricKey("gpu_device_utilization");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_TRUE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    // Should have hw.gpu.task=device attribute.
    bool found_task = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.gpu.task" && v == "device") {
        found_task = true;
      }
    }
    EXPECT_TRUE(found_task);
  }

  // Verify gpu_memory_utilization mapping.
  {
    auto parsed = parseMetricKey("gpu_memory_utilization");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.memory.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");
    EXPECT_TRUE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
  }

  // Verify that non-percent GPU metrics do NOT have divide_by_100.
  {
    auto parsed = parseMetricKey("gpu_power_draw");
    ASSERT_TRUE(parsed.matched);
    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }
  {
    auto parsed = parseMetricKey("graphics_engine_active_ratio");
    ASSERT_TRUE(parsed.matched);
    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }
  {
    auto parsed = parseMetricKey("sm_active_ratio");
    ASSERT_TRUE(parsed.matched);
    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }
  {
    auto parsed = parseMetricKey("pcie_tx_bytes");
    ASSERT_TRUE(parsed.matched);
    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }
  {
    auto parsed = parseMetricKey("fp16_active");
    ASSERT_TRUE(parsed.matched);
    EXPECT_FALSE(parsed.mapping.divide_by_100);
  }

  // Verify GPU metrics are identified by the "hw." prefix (same logic as
  // finalize uses to decide whether to attach hw.id and Slurm attrs).
  auto check_is_gpu = [](const std::string& key) {
    auto parsed = parseMetricKey(key);
    return parsed.matched &&
        parsed.mapping.otel_name.find("hw.") == 0;
  };
  EXPECT_TRUE(check_is_gpu("gpu_device_utilization"));
  EXPECT_TRUE(check_is_gpu("gpu_memory_utilization"));
  EXPECT_TRUE(check_is_gpu("gpu_power_draw"));
  EXPECT_TRUE(check_is_gpu("graphics_engine_active_ratio"));
  EXPECT_TRUE(check_is_gpu("sm_active_ratio"));
  EXPECT_TRUE(check_is_gpu("pcie_tx_bytes"));
  EXPECT_TRUE(check_is_gpu("fp16_active"));
  EXPECT_TRUE(check_is_gpu("dcgm_error"));

  // Negative: non-GPU metrics should NOT start with "hw.".
  EXPECT_FALSE(check_is_gpu("cpu_util"));
  EXPECT_FALSE(check_is_gpu("uptime"));
  EXPECT_FALSE(check_is_gpu("rx_bytes.eth0"));
  EXPECT_FALSE(check_is_gpu("cpu_u_ms"));
  EXPECT_FALSE(check_is_gpu("totally_unknown"));

  // Edge case: empty Slurm attribution values.
  OTLPLogger logger2;
  logger2.logStr("job_id", "");
  logger2.logStr("username", "");
  EXPECT_EQ(logger2.pending_attributes_["job_id"], "");
  EXPECT_EQ(logger2.pending_attributes_["username"], "");

  // Edge case: no device metadata logged -
  // pending_metrics_ has no "device" key.
  OTLPLogger logger3;
  logger3.logFloat("gpu_device_utilization", 50.0f);
  EXPECT_EQ(logger3.pending_metrics_.count("device"), 0);
}

TEST(OTLPLoggerTest, FinalizeClearsState) {
  // Verify that pending_metrics_ accumulates correctly per batch and that
  // a second batch starts fresh after clearing (simulating finalize behavior).
  OTLPLogger logger;

  // First batch.
  logger.logFloat("cpu_util", 75.0f);
  logger.logInt("uptime", 1000);
  logger.logStr("job_id", "111");
  ASSERT_EQ(logger.pending_metrics_.size(), 2);
  ASSERT_EQ(logger.pending_attributes_.size(), 1);
  EXPECT_NEAR(logger.pending_metrics_["cpu_util"], 75.0, 0.01);
  EXPECT_DOUBLE_EQ(logger.pending_metrics_["uptime"], 1000.0);
  EXPECT_EQ(logger.pending_attributes_["job_id"], "111");

  // finalize() clears both maps (takes the !isInitialized() path).
  logger.finalize();
  EXPECT_TRUE(logger.pending_metrics_.empty());
  EXPECT_TRUE(logger.pending_attributes_.empty());

  // Second batch should start completely fresh.
  logger.logFloat("cpu_util", 80.0f);
  logger.logStr("job_id", "222");
  ASSERT_EQ(logger.pending_metrics_.size(), 1);
  ASSERT_EQ(logger.pending_attributes_.size(), 1);
  EXPECT_NEAR(logger.pending_metrics_["cpu_util"], 80.0, 0.01);
  EXPECT_EQ(logger.pending_attributes_["job_id"], "222");
  // uptime from the first batch should NOT be present.
  EXPECT_EQ(logger.pending_metrics_.count("uptime"), 0);

  // Third batch: empty batch.
  logger.finalize();
  EXPECT_TRUE(logger.pending_metrics_.empty());
  EXPECT_TRUE(logger.pending_attributes_.empty());

  // Verify nothing bleeds into subsequent batches.
  logger.logUint("rx_bytes.eth0", 42);
  EXPECT_EQ(logger.pending_metrics_.size(), 1);
  EXPECT_EQ(logger.pending_metrics_.count("cpu_util"), 0);
  EXPECT_EQ(logger.pending_attributes_.count("job_id"), 0);
}

TEST(OTLPLoggerTest, CompleteMetricFlowWithoutNetwork) {
  // Test a representative set of metrics through parseMetricKey and verify the
  // full mapping including OTEL name, all attributes,
  // divide_by_100, kind, unit. This exercises the
  // complete path that finalize() would take for
  // each metric.

  // --- CPU utilization metric ---
  {
    auto parsed = parseMetricKey("cpu_util");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_TRUE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "cpu.mode");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "active");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());

    // Simulate the divide_by_100 conversion.
    double raw = 75.0;
    double converted = parsed.mapping.divide_by_100 ? raw / 100.0 : raw;
    EXPECT_DOUBLE_EQ(converted, 0.75);
  }

  // --- CPU time metric (Gauge) ---
  {
    auto parsed = parseMetricKey("cpu_u_ms");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    EXPECT_EQ(parsed.mapping.unit, "s");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "cpu.mode");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "user");
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // --- NUMA CPU metric ---
  {
    auto parsed = parseMetricKey("cpu_s_node2");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_TRUE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "cpu.mode");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "system");
    ASSERT_EQ(parsed.dynamic_attributes.size(), 1);
    EXPECT_EQ(parsed.dynamic_attributes[0].first, "cpu.socket");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "2");
  }

  // --- Network metric with device ---
  {
    auto parsed = parseMetricKey("tx_bytes.ens5");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.network.io");
    EXPECT_EQ(parsed.mapping.unit, "By");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "network.io.direction");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "transmit");
    ASSERT_EQ(parsed.dynamic_attributes.size(), 1);
    EXPECT_EQ(parsed.dynamic_attributes[0].first, "network.interface.name");
    EXPECT_EQ(parsed.dynamic_attributes[0].second, "ens5");
  }

  // --- GPU metric with device + divide_by_100 ---
  {
    auto parsed = parseMetricKey("gpu_device_utilization");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_TRUE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);

    // Check static attributes.
    bool found_task = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.gpu.task" && v == "device") {
        found_task = true;
      }
    }
    EXPECT_TRUE(found_task);
    EXPECT_TRUE(parsed.dynamic_attributes.empty());

    // Simulate the full attribute merge finalize() would do:
    // static attrs + dynamic attrs + hw.id (from device) + slurm attrs.
    std::vector<std::pair<std::string, std::string>> all_attrs;
    all_attrs.insert(
        all_attrs.end(),
        parsed.mapping.attributes.begin(),
        parsed.mapping.attributes.end());
    all_attrs.insert(
        all_attrs.end(),
        parsed.dynamic_attributes.begin(),
        parsed.dynamic_attributes.end());

    // Add hw.id from device=2.
    std::string gpu_device_id = fmt::format("{}", 2);
    bool is_gpu_metric = parsed.mapping.otel_name.find("hw.") == 0;
    if (is_gpu_metric && !gpu_device_id.empty()) {
      all_attrs.emplace_back("hw.id", gpu_device_id);
    }

    // Add Slurm attrs.
    std::unordered_map<std::string, std::string> slurm_attrs = {
        {"job_id", "99999"}, {"username", "gpuuser"}};
    if (is_gpu_metric) {
      for (const auto& [attr_key, attr_val] : slurm_attrs) {
        all_attrs.emplace_back(attr_key, attr_val);
      }
    }

    // Verify merged attributes contain expected entries.
    auto find_attr = [&](const std::string& key) -> std::string {
      for (const auto& [k, v] : all_attrs) {
        if (k == key) {
          return v;
        }
      }
      return "";
    };
    EXPECT_EQ(find_attr("hw.gpu.task"), "device");
    EXPECT_EQ(find_attr("hw.id"), "2");
    // Slurm attrs (order not guaranteed, just check presence).
    bool has_job_id = false;
    bool has_username = false;
    for (const auto& [k, v] : all_attrs) {
      if (k == "job_id" && v == "99999") has_job_id = true;
      if (k == "username" && v == "gpuuser") has_username = true;
    }
    EXPECT_TRUE(has_job_id);
    EXPECT_TRUE(has_username);

    // Simulate divide_by_100 conversion.
    double raw = 85.0;
    double converted = parsed.mapping.divide_by_100 ? raw / 100.0 : raw;
    EXPECT_DOUBLE_EQ(converted, 0.85);
  }

  // --- GPU metric without divide_by_100 (power) ---
  {
    auto parsed = parseMetricKey("gpu_power_draw");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.power");
    EXPECT_EQ(parsed.mapping.unit, "W");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    bool found_hw_type = false;
    for (const auto& [k, v] : parsed.mapping.attributes) {
      if (k == "hw.type" && v == "gpu") {
        found_hw_type = true;
      }
    }
    EXPECT_TRUE(found_hw_type);
  }

  // --- Skip-flagged metadata key ---
  {
    auto parsed = parseMetricKey("device");
    ASSERT_TRUE(parsed.matched);
    EXPECT_TRUE(parsed.mapping.skip);
    EXPECT_TRUE(parsed.mapping.otel_name.empty());
    EXPECT_TRUE(parsed.mapping.unit.empty());
  }

  // --- Unknown metric (passthrough) ---
  {
    auto parsed = parseMetricKey("custom_metric_xyz");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "dynolog.custom_metric_xyz");
    EXPECT_EQ(parsed.mapping.unit, "");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    EXPECT_TRUE(parsed.mapping.attributes.empty());
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // --- Special CPU metric (uptime) ---
  {
    auto parsed = parseMetricKey("uptime");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "system.uptime");
    EXPECT_EQ(parsed.mapping.unit, "s");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    EXPECT_TRUE(parsed.mapping.attributes.empty());
    EXPECT_TRUE(parsed.dynamic_attributes.empty());
  }

  // --- Boundary: GPU metric with pipe attribute ---
  {
    auto parsed = parseMetricKey("tensorcore_active");
    ASSERT_TRUE(parsed.matched);
    EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.pipe.utilization");
    EXPECT_EQ(parsed.mapping.unit, "1");

    EXPECT_FALSE(parsed.mapping.divide_by_100);
    EXPECT_FALSE(parsed.mapping.skip);
    ASSERT_EQ(parsed.mapping.attributes.size(), 1);
    EXPECT_EQ(parsed.mapping.attributes[0].first, "pipe");
    EXPECT_EQ(parsed.mapping.attributes[0].second, "tensorcore");
  }
}

// =============================================================================
// Multi-Threaded Isolation Test
// =============================================================================

TEST(OTLPLoggerTest, MultiThreadedIsolation) {
  // Verify that 3 OTLPLogger instances (simulating 3 collector threads)
  // can accumulate metrics concurrently without cross-contamination.
  // Also verify parseMetricKey is safe to call from multiple threads
  // (it uses only const static data).

  constexpr int kIterations = 500;

  OTLPLogger logger1; // simulates kernel collector
  OTLPLogger logger2; // simulates perf collector
  OTLPLogger logger3; // simulates GPU collector

  // Use threads to populate each logger concurrently.
  auto kernel_work = [&]() {
    for (int i = 0; i < kIterations; ++i) {
      logger1.logFloat("cpu_util", static_cast<float>(i));
      logger1.logFloat("cpu_u", static_cast<float>(i * 2));
      logger1.logInt("uptime", i * 1000);
      // Call parseMetricKey from this thread to verify thread-safety.
      auto parsed = parseMetricKey("cpu_util");
      EXPECT_TRUE(parsed.matched);
      EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.utilization");
    }
  };

  auto perf_work = [&]() {
    for (int i = 0; i < kIterations; ++i) {
      logger2.logUint("cpu_u_ms", static_cast<uint64_t>(i * 100));
      logger2.logUint("cpu_s_ms", static_cast<uint64_t>(i * 50));
      logger2.logInt("mips", i * 10);
      // Call parseMetricKey from this thread.
      auto parsed = parseMetricKey("cpu_u_ms");
      EXPECT_TRUE(parsed.matched);
      EXPECT_EQ(parsed.mapping.otel_name, "system.cpu.time");
    }
  };

  auto gpu_work = [&]() {
    for (int i = 0; i < kIterations; ++i) {
      logger3.logFloat(
          "gpu_device_utilization", static_cast<float>(i % 100));
      logger3.logFloat("gpu_power_draw", static_cast<float>(i * 10));
      logger3.logInt("device", i % 8);
      logger3.logStr("job_id", fmt::format("job_{}", i));
      // Call parseMetricKey from this thread.
      auto parsed = parseMetricKey("gpu_device_utilization");
      EXPECT_TRUE(parsed.matched);
      EXPECT_EQ(parsed.mapping.otel_name, "hw.gpu.utilization");
    }
  };

  std::thread t1(kernel_work);
  std::thread t2(perf_work);
  std::thread t3(gpu_work);

  t1.join();
  t2.join();
  t3.join();

  // Verify per-instance state isolation: each logger has ONLY its own metrics.

  // Logger 1 (kernel): should have cpu_util, cpu_u, uptime; NOT cpu_u_ms, etc.
  EXPECT_EQ(logger1.pending_metrics_.count("cpu_util"), 1);
  EXPECT_EQ(logger1.pending_metrics_.count("cpu_u"), 1);
  EXPECT_EQ(logger1.pending_metrics_.count("uptime"), 1);
  EXPECT_EQ(logger1.pending_metrics_.size(), 3);
  // Should NOT have any metrics from logger2 or logger3.
  EXPECT_EQ(logger1.pending_metrics_.count("cpu_u_ms"), 0);
  EXPECT_EQ(logger1.pending_metrics_.count("cpu_s_ms"), 0);
  EXPECT_EQ(logger1.pending_metrics_.count("mips"), 0);
  EXPECT_EQ(logger1.pending_metrics_.count("gpu_device_utilization"), 0);
  EXPECT_EQ(logger1.pending_metrics_.count("gpu_power_draw"), 0);
  EXPECT_EQ(logger1.pending_metrics_.count("device"), 0);
  EXPECT_TRUE(logger1.pending_attributes_.empty());

  // Logger 2 (perf): should have cpu_u_ms, cpu_s_ms, mips only.
  EXPECT_EQ(logger2.pending_metrics_.count("cpu_u_ms"), 1);
  EXPECT_EQ(logger2.pending_metrics_.count("cpu_s_ms"), 1);
  EXPECT_EQ(logger2.pending_metrics_.count("mips"), 1);
  EXPECT_EQ(logger2.pending_metrics_.size(), 3);
  EXPECT_EQ(logger2.pending_metrics_.count("cpu_util"), 0);
  EXPECT_EQ(logger2.pending_metrics_.count("gpu_device_utilization"), 0);
  EXPECT_TRUE(logger2.pending_attributes_.empty());

  // Logger 3 (GPU): should have gpu_device_utilization, gpu_power_draw, device.
  EXPECT_EQ(logger3.pending_metrics_.count("gpu_device_utilization"), 1);
  EXPECT_EQ(logger3.pending_metrics_.count("gpu_power_draw"), 1);
  EXPECT_EQ(logger3.pending_metrics_.count("device"), 1);
  EXPECT_EQ(logger3.pending_metrics_.size(), 3);
  EXPECT_EQ(logger3.pending_metrics_.count("cpu_util"), 0);
  EXPECT_EQ(logger3.pending_metrics_.count("cpu_u_ms"), 0);
  // GPU logger should have Slurm attrs.
  EXPECT_EQ(logger3.pending_attributes_.count("job_id"), 1);
  EXPECT_EQ(logger3.pending_attributes_.size(), 1);

  // Verify last-write-wins semantics for each logger's values.
  // Each loop runs kIterations times, so the last written value is
  // for i = kIterations - 1.
  EXPECT_DOUBLE_EQ(
      logger1.pending_metrics_["cpu_util"],
      static_cast<double>(kIterations - 1));
  EXPECT_DOUBLE_EQ(
      logger1.pending_metrics_["uptime"],
      static_cast<double>((kIterations - 1) * 1000));

  EXPECT_DOUBLE_EQ(
      logger2.pending_metrics_["cpu_u_ms"],
      static_cast<double>((kIterations - 1) * 100));

  EXPECT_DOUBLE_EQ(
      logger3.pending_metrics_["gpu_power_draw"],
      static_cast<double>((kIterations - 1) * 10));
  EXPECT_EQ(
      logger3.pending_attributes_["job_id"],
      fmt::format("job_{}", kIterations - 1));

  // Verify parseMetricKey still returns consistent
  // results after concurrent use.
  // (This would fail if there were any hidden mutable static state.)
  {
    auto p1 = parseMetricKey("cpu_util");
    EXPECT_TRUE(p1.matched);
    EXPECT_EQ(p1.mapping.otel_name, "system.cpu.utilization");
    EXPECT_TRUE(p1.mapping.divide_by_100);

    auto p2 = parseMetricKey("cpu_u_ms");
    EXPECT_TRUE(p2.matched);
    EXPECT_EQ(p2.mapping.otel_name, "system.cpu.time");

    auto p3 = parseMetricKey("gpu_device_utilization");
    EXPECT_TRUE(p3.matched);
    EXPECT_EQ(p3.mapping.otel_name, "hw.gpu.utilization");
    EXPECT_TRUE(p3.mapping.divide_by_100);

    auto p4 = parseMetricKey("rx_bytes.eth0");
    EXPECT_TRUE(p4.matched);
    EXPECT_EQ(p4.mapping.otel_name, "system.network.io");
  }
}

// =============================================================================
// parseHeaders() Tests
// =============================================================================

TEST(OTLPLoggerTest, ParseHeadersEmpty) {
  auto result = parseHeaders("");
  EXPECT_TRUE(result.empty());
}

TEST(OTLPLoggerTest, ParseHeadersSingle) {
  auto result = parseHeaders("key=value");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].first, "key");
  EXPECT_EQ(result[0].second, "value");
}

TEST(OTLPLoggerTest, ParseHeadersMultiple) {
  auto result = parseHeaders("k1=v1,k2=v2,k3=v3");
  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0].first, "k1");
  EXPECT_EQ(result[0].second, "v1");
  EXPECT_EQ(result[1].first, "k2");
  EXPECT_EQ(result[1].second, "v2");
  EXPECT_EQ(result[2].first, "k3");
  EXPECT_EQ(result[2].second, "v3");
}

TEST(OTLPLoggerTest, ParseHeadersMissingEquals) {
  // Token without '=' should be skipped.
  auto result = parseHeaders("noequals");
  EXPECT_TRUE(result.empty());
}

TEST(OTLPLoggerTest, ParseHeadersTrailingComma) {
  // Trailing comma produces an empty token which has no '=' -> skipped.
  auto result = parseHeaders("k=v,");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].first, "k");
  EXPECT_EQ(result[0].second, "v");
}

TEST(OTLPLoggerTest, ParseHeadersEmptyValue) {
  // "key=" -> key is "key", value is "".
  auto result = parseHeaders("key=");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].first, "key");
  EXPECT_EQ(result[0].second, "");
}

TEST(OTLPLoggerTest, ParseHeadersEmptyKey) {
  // "=value" -> empty key should be skipped.
  auto result = parseHeaders("=value");
  EXPECT_TRUE(result.empty());
}

TEST(OTLPLoggerTest, ParseHeadersMixed) {
  // Mix of valid and invalid tokens.
  auto result = parseHeaders("good=val,bad,also=ok");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].first, "good");
  EXPECT_EQ(result[0].second, "val");
  EXPECT_EQ(result[1].first, "also");
  EXPECT_EQ(result[1].second, "ok");
}

TEST(OTLPLoggerTest, ParseHeadersValueContainsEquals) {
  // Value with '=' in it (e.g., base64-encoded token). The parser splits on
  // the FIRST '=' so the rest stays in the value.
  auto result = parseHeaders("Authorization=Bearer abc=def==");
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].first, "Authorization");
  EXPECT_EQ(result[0].second, "Bearer abc=def==");
}

TEST(OTLPLoggerTest, ParseHeadersConsecutiveCommas) {
  // Consecutive commas produce empty tokens (no '=' -> skipped).
  auto result = parseHeaders("k1=v1,,k2=v2");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0].first, "k1");
  EXPECT_EQ(result[0].second, "v1");
  EXPECT_EQ(result[1].first, "k2");
  EXPECT_EQ(result[1].second, "v2");
}

// =============================================================================
// getEnvOr() Tests
// =============================================================================

TEST(OTLPLoggerTest, GetEnvOrUnset) {
  // Unset variable should return fallback.
  ::unsetenv("DYNOLOG_TEST_VAR_UNSET");
  EXPECT_EQ(getEnvOr("DYNOLOG_TEST_VAR_UNSET", "fallback"), "fallback");
}

TEST(OTLPLoggerTest, GetEnvOrSet) {
  // Set variable should return its value.
  ::setenv("DYNOLOG_TEST_VAR_SET", "hello", 1);
  EXPECT_EQ(getEnvOr("DYNOLOG_TEST_VAR_SET", "fallback"), "hello");
  ::unsetenv("DYNOLOG_TEST_VAR_SET");
}

TEST(OTLPLoggerTest, GetEnvOrSetEmpty) {
  // Empty string env var: current code checks
  // val[0] != '\0', so returns fallback.
  ::setenv("DYNOLOG_TEST_VAR_EMPTY", "", 1);
  EXPECT_EQ(getEnvOr("DYNOLOG_TEST_VAR_EMPTY", "fallback"), "fallback");
  ::unsetenv("DYNOLOG_TEST_VAR_EMPTY");
}

// =============================================================================
// OTLPManager Integration Tests
// =============================================================================

TEST(OTLPLoggerTest, OTLPManagerConstructionGRPC) {
  // Construct OTLPManager with gRPC protocol. The otel SDK creates objects
  // lazily so no real network I/O happens during construction.
  FLAGS_otlp_endpoint = "http://localhost:4317";
  FLAGS_otlp_protocol = "grpc";
  FLAGS_otlp_service_name = "dynolog-test";
  FLAGS_otlp_headers = "";
  FLAGS_otlp_timeout_ms = 5000;
  FLAGS_otlp_export_interval_ms = 60000;
  FLAGS_otlp_use_tls = false;

  OTLPManager mgr;
  EXPECT_NE(mgr.provider_, nullptr);
  EXPECT_NE(mgr.meter_, nullptr);
}

TEST(OTLPLoggerTest, OTLPManagerConstructionHTTP) {
  // Construct OTLPManager with HTTP protocol.
  FLAGS_otlp_endpoint = "http://localhost:4318/v1/metrics";
  FLAGS_otlp_protocol = "http";
  FLAGS_otlp_service_name = "dynolog-test-http";
  FLAGS_otlp_headers = "Authorization=Bearer test123";
  FLAGS_otlp_timeout_ms = 5000;
  FLAGS_otlp_export_interval_ms = 60000;
  FLAGS_otlp_use_tls = false;

  OTLPManager mgr;
  EXPECT_NE(mgr.provider_, nullptr);
  EXPECT_NE(mgr.meter_, nullptr);
}

TEST(OTLPLoggerTest, OTLPManagerForceFlush) {
  // Construct and call forceFlush(). Should not crash or throw.
  FLAGS_otlp_endpoint = "http://localhost:4317";
  FLAGS_otlp_protocol = "grpc";
  FLAGS_otlp_service_name = "dynolog-test-flush";
  FLAGS_otlp_headers = "";
  FLAGS_otlp_timeout_ms = 1000;
  FLAGS_otlp_export_interval_ms = 60000;
  FLAGS_otlp_use_tls = false;

  OTLPManager mgr;
  ASSERT_NE(mgr.provider_, nullptr);
  EXPECT_NO_THROW(mgr.forceFlush());
}

TEST(OTLPLoggerTest, OTLPManagerLogGauge) {
  // Construct and call logGauge with sample values.
  // Should not crash even though there's no real collector running.
  FLAGS_otlp_endpoint = "http://localhost:4317";
  FLAGS_otlp_protocol = "grpc";
  FLAGS_otlp_service_name = "dynolog-test-log";
  FLAGS_otlp_headers = "";
  FLAGS_otlp_timeout_ms = 1000;
  FLAGS_otlp_export_interval_ms = 60000;
  FLAGS_otlp_use_tls = false;

  OTLPManager mgr;
  ASSERT_NE(mgr.provider_, nullptr);
  ASSERT_NE(mgr.meter_, nullptr);

  // logGauge with attributes.
  EXPECT_NO_THROW(mgr.logGauge(
      "test.gauge", "1", 42.0, {{"state", "active"}}));

  // logGauge with no attributes.
  EXPECT_NO_THROW(mgr.logGauge("test.gauge2", "ms", 100.0, {}));

  // logGauge with different units and attributes.
  EXPECT_NO_THROW(mgr.logGauge(
      "test.gauge3", "By", 1024.0, {{"direction", "receive"}}));

  // logGauge with zero value.
  EXPECT_NO_THROW(mgr.logGauge("test.gauge4", "{packet}", 0.0, {}));

  // logGauge with NaN - should be silently skipped (no crash).
  EXPECT_NO_THROW(mgr.logGauge(
      "test.nan", "1", std::numeric_limits<double>::quiet_NaN(), {}));

  // logGauge with Inf - should be silently skipped (no crash).
  EXPECT_NO_THROW(mgr.logGauge(
      "test.inf", "1", std::numeric_limits<double>::infinity(), {}));
}

TEST(OTLPLoggerTest, OTLPManagerExportIntervalValidation) {
  // Zero interval should fall back to default (no crash, no busy-spin).
  FLAGS_otlp_endpoint = "http://localhost:4317";
  FLAGS_otlp_protocol = "grpc";
  FLAGS_otlp_service_name = "dynolog-test";
  FLAGS_otlp_headers = "";
  FLAGS_otlp_timeout_ms = 5000;
  FLAGS_otlp_export_interval_ms = 0;
  FLAGS_otlp_use_tls = false;

  OTLPManager mgr;
  EXPECT_NE(mgr.provider_, nullptr);

  // Negative interval should also fall back to default.
  FLAGS_otlp_export_interval_ms = -1;
  OTLPManager mgr2;
  EXPECT_NE(mgr2.provider_, nullptr);

  // Timeout >= interval should log warning but still construct.
  FLAGS_otlp_export_interval_ms = 5000;
  FLAGS_otlp_timeout_ms = 10000;
  OTLPManager mgr3;
  EXPECT_NE(mgr3.provider_, nullptr);
}

// =============================================================================
// Additional NUMA Regex Edge Cases
// =============================================================================

TEST(OTLPLoggerTest, NUMARegexNoDigit) {
  // "cpu_u_nodeABC" should NOT match the NUMA regex (no digits after "node").
  // Falls through to dynolog.* passthrough.
  auto parsed = parseMetricKey("cpu_u_nodeABC");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.cpu_u_nodeABC");
}

TEST(OTLPLoggerTest, NUMARegexNoNodeNumber) {
  // "cpu_u_node" with no digits should NOT match the NUMA regex.
  auto parsed = parseMetricKey("cpu_u_node");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.cpu_u_node");
}

TEST(OTLPLoggerTest, NUMARegexInvalidMode) {
  // "cpu_x_node0" - 'x' is not in (u|s|i), should NOT match the NUMA regex.
  // It also won't match CPU time (cpu_x_ms) since there's no "_ms" suffix.
  // Falls through to dynolog.* passthrough.
  auto parsed = parseMetricKey("cpu_x_node0");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.cpu_x_node0");
}

TEST(OTLPLoggerTest, NUMARegexExtraTrailing) {
  // "cpu_u_node0_extra" has trailing chars after the digits.
  // regex_match requires full-string match, so this should NOT match.
  auto parsed = parseMetricKey("cpu_u_node0_extra");
  ASSERT_TRUE(parsed.matched);
  EXPECT_EQ(parsed.mapping.otel_name, "dynolog.cpu_u_node0_extra");
}

// =============================================================================
// End-to-End Integration Tests (require Docker + OTLP_INTEGRATION_TEST)
// =============================================================================

#ifdef OTLP_INTEGRATION_TEST

namespace {

// Execute a shell command and capture stdout + exit code.
std::pair<std::string, int> execCommand(const std::string& cmd) {
  std::string output;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {"popen failed", -1};
  }
  std::array<char, 4096> buf;
  while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
    output += buf.data();
  }
  int status = pclose(pipe);
  int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  while (!output.empty() &&
         (output.back() == '\n' || output.back() == '\r' ||
          output.back() == ' ')) {
    output.pop_back();
  }
  return {output, exit_code};
}

// Wait for a TCP port to accept connections.
bool waitForPort(int port, int timeout_ms = 30000) {
  auto start = std::chrono::steady_clock::now();
  while (true) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      return false;
    }
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int result = connect(
        sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    close(sock);

    if (result == 0) {
      return true;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    if (elapsed >= timeout_ms) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

} // namespace

class OTLPIntegrationTest : public ::testing::Test {
 protected:
  static constexpr int kGRPCPort = 14317;
  static constexpr int kHTTPPort = 14318;
  static constexpr int kContainerReadyTimeoutMs = 30000;
  static constexpr int kOutputReadyTimeoutMs = 15000;

  void SetUp() override {
    // Verify Docker is available.
    auto [info_out, info_ec] = execCommand("docker info >/dev/null 2>&1");
    if (info_ec != 0) {
      GTEST_SKIP() << "Docker not available, skipping integration test";
    }

    // Create unique temp directory per test.
    current_test_id_ = test_counter_++;
    base_dir_ = fmt::format(
        "/tmp/otlp-integ-{}-{}", getpid(), current_test_id_);
    auto [mkdir_out, mkdir_ec] =
        execCommand("mkdir -p " + base_dir_ + "/config " + base_dir_ + "/data");
    ASSERT_EQ(mkdir_ec, 0) << "Failed to create temp dirs: " << mkdir_out;
    // The collector container runs as non-root (UID 10001); the data
    // directory must be writable by that user.
    execCommand("chmod 777 " + base_dir_ + "/data");

    // Write collector configuration.
    // Use the test ports directly (container runs with --network=host
    // to avoid docker-proxy issues with gRPC/HTTP2).
    std::string config = fmt::format(R"(receivers:
  otlp:
    protocols:
      grpc:
        endpoint: 0.0.0.0:{}
      http:
        endpoint: 0.0.0.0:{}

exporters:
  file:
    path: {}

service:
  pipelines:
    metrics:
      receivers: [otlp]
      exporters: [file]
)", kGRPCPort, kHTTPPort, base_dir_ + "/data/output.json");
    {
      std::ofstream ofs(base_dir_ + "/config/config.yaml");
      ASSERT_TRUE(ofs.is_open()) << "Failed to write collector config";
      ofs << config;
    }
  }

  void TearDown() override {
    stopCollector();
    execCommand("rm -rf " + base_dir_);
  }

  void startCollector() {
    // Force-stop any leftover containers from previous test runs.
    execCommand(
        "docker ps -aq --filter name=otlp-test-"
        " | xargs -r docker rm -f 2>/dev/null");

    container_name_ = fmt::format(
        "otlp-test-{}-{}", getpid(), current_test_id_);

    // Use --network=host so the container binds directly to host ports,
    // avoiding docker-proxy issues with gRPC/HTTP2 connections.
    std::string cmd = fmt::format(
        "docker run -d --name {} "
        "--network=host "
        "-v {}:/etc/otelcol/config.yaml:ro "
        "-v {}:{} "
        "otel/opentelemetry-collector:latest 2>&1",
        container_name_,
        base_dir_ + "/config/config.yaml",
        base_dir_ + "/data",
        base_dir_ + "/data");

    auto [output, ec] = execCommand(cmd);
    ASSERT_EQ(ec, 0)
        << "Failed to start collector container: " << output;

    // Wait for both ports to be ready.
    ASSERT_TRUE(waitForPort(kGRPCPort, kContainerReadyTimeoutMs))
        << "Collector gRPC port not ready within timeout";
    ASSERT_TRUE(waitForPort(kHTTPPort, kContainerReadyTimeoutMs))
        << "Collector HTTP port not ready within timeout";
  }

  void stopCollector() {
    if (!container_name_.empty()) {
      execCommand("docker stop " + container_name_ + " >/dev/null 2>&1");
      execCommand("docker rm -f " + container_name_ + " >/dev/null 2>&1");
      container_name_.clear();
    }
  }

  // Construct an OTLPManager pointing at the local collector.
  std::unique_ptr<OTLPManager> createManager(
      const std::string& protocol,
      const std::string& headers = "") {
    if (protocol == "http") {
      FLAGS_otlp_endpoint =
          fmt::format("http://localhost:{}/v1/metrics", kHTTPPort);
      FLAGS_otlp_protocol = "http";
    } else {
      FLAGS_otlp_endpoint =
          fmt::format("http://localhost:{}", kGRPCPort);
      FLAGS_otlp_protocol = "grpc";
    }
    FLAGS_otlp_service_name = "dynolog-integ-test";
    FLAGS_otlp_headers = headers;
    FLAGS_otlp_timeout_ms = 10000;
    FLAGS_otlp_use_tls = false;
    return std::make_unique<OTLPManager>();
  }

  // Read the file exporter output, retrying until the expected metric appears
  // or timeout is reached.
  nlohmann::json waitForOutput(
      const std::string& expected_metric,
      int timeout_ms = kOutputReadyTimeoutMs) {
    auto start = std::chrono::steady_clock::now();
    nlohmann::json result;
    while (true) {
      result = readOutputFile();
      if (!result.empty()) {
        if (expected_metric.empty()) {
          return result;
        }
        auto metric = findMetric(result, expected_metric);
        if (!metric.is_null()) {
          return result;
        }
      }
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
      if (elapsed >= timeout_ms) {
        return result;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }

  // Parse the collector's output file (handles JSON Lines and single-object).
  nlohmann::json readOutputFile() {
    nlohmann::json result = nlohmann::json::array();
    std::string path = base_dir_ + "/data/output.json";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      return result;
    }
    std::string content(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>());
    if (content.empty()) {
      return result;
    }

    // Try parsing as a single JSON object first.
    try {
      auto parsed = nlohmann::json::parse(content);
      if (parsed.is_array()) {
        return parsed;
      }
      result.push_back(std::move(parsed));
      return result;
    } catch (const nlohmann::json::exception&) {
      // Fall through to JSON Lines parsing.
    }

    // Parse as JSON Lines (one JSON object per line).
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.empty()) {
        continue;
      }
      try {
        result.push_back(nlohmann::json::parse(line));
      } catch (const nlohmann::json::exception&) {
        // Skip malformed lines.
      }
    }
    return result;
  }

  // Search all batches for a metric by name. Returns the first match or null.
  static nlohmann::json findMetric(
      const nlohmann::json& batches,
      const std::string& name) {
    for (const auto& batch : batches) {
      if (!batch.contains("resourceMetrics")) {
        continue;
      }
      for (const auto& rm : batch["resourceMetrics"]) {
        if (!rm.contains("scopeMetrics")) {
          continue;
        }
        for (const auto& sm : rm["scopeMetrics"]) {
          if (!sm.contains("metrics")) {
            continue;
          }
          for (const auto& m : sm["metrics"]) {
            if (m.value("name", "") == name) {
              return m;
            }
          }
        }
      }
    }
    return nullptr;
  }

  // Get resource attributes from the first batch.
  static nlohmann::json getResourceAttributes(
      const nlohmann::json& batches) {
    for (const auto& batch : batches) {
      if (!batch.contains("resourceMetrics")) {
        continue;
      }
      for (const auto& rm : batch["resourceMetrics"]) {
        if (rm.contains("resource") &&
            rm["resource"].contains("attributes")) {
          return rm["resource"]["attributes"];
        }
      }
    }
    return nlohmann::json::array();
  }

  // Check if an attribute array contains a specific key=value string pair.
  static bool hasStringAttribute(
      const nlohmann::json& attrs,
      const std::string& key,
      const std::string& value) {
    for (const auto& attr : attrs) {
      if (attr.value("key", "") == key &&
          attr.contains("value") &&
          attr["value"].contains("stringValue") &&
          attr["value"]["stringValue"] == value) {
        return true;
      }
    }
    return false;
  }

  // Check if an attribute key exists (regardless of value).
  static bool hasAttributeKey(
      const nlohmann::json& attrs,
      const std::string& key) {
    for (const auto& attr : attrs) {
      if (attr.value("key", "") == key) {
        return true;
      }
    }
    return false;
  }

  // Get the double value from a data point (handles both asDouble and asInt).
  static double getDataPointValue(const nlohmann::json& dp) {
    if (dp.contains("asDouble")) {
      return dp["asDouble"].get<double>();
    }
    if (dp.contains("asInt")) {
      // asInt is a string-encoded integer in OTLP JSON.
      auto val = dp["asInt"];
      if (val.is_string()) {
        return std::stod(val.get<std::string>());
      }
      return val.get<double>();
    }
    return std::numeric_limits<double>::quiet_NaN();
  }

  // Get the first gauge data point's double value.
  static double getGaugeValue(const nlohmann::json& metric) {
    if (metric.contains("gauge") &&
        metric["gauge"].contains("dataPoints") &&
        !metric["gauge"]["dataPoints"].empty()) {
      return getDataPointValue(metric["gauge"]["dataPoints"][0]);
    }
    return std::numeric_limits<double>::quiet_NaN();
  }

  // Get attributes from the first data point of a gauge metric.
  static nlohmann::json getGaugeAttributes(const nlohmann::json& metric) {
    if (metric.contains("gauge") &&
        metric["gauge"].contains("dataPoints") &&
        !metric["gauge"]["dataPoints"].empty() &&
        metric["gauge"]["dataPoints"][0].contains("attributes")) {
      return metric["gauge"]["dataPoints"][0]["attributes"];
    }
    return nlohmann::json::array();
  }

  std::string container_name_;
  std::string base_dir_;
  int current_test_id_ = 0;
  static int test_counter_;
};

int OTLPIntegrationTest::test_counter_ = 0;

// ---- Test 1: gRPC basic CPU/uptime metrics ----

TEST_F(OTLPIntegrationTest, IntegrationGRPCBasicMetrics) {
  startCollector();
  auto mgr = createManager("grpc");

  // Log CPU utilization gauge (value already divided by 100 by caller).
  mgr->logGauge(
      "system.cpu.utilization", "1", 0.75, {{"cpu.mode", "active"}});

  // Log uptime gauge.
  mgr->logGauge("system.uptime", "s", 86400.0, {});

  // Log CPU time gauge (per-interval delta, same as Prometheus path).
  mgr->logGauge(
      "system.cpu.time", "s", 12345.0, {{"cpu.mode", "user"}});

  mgr->forceFlush();

  auto output = waitForOutput("system.cpu.utilization");
  ASSERT_FALSE(output.empty()) << "No output from collector";

  // Verify CPU utilization metric.
  auto cpu_metric = findMetric(output, "system.cpu.utilization");
  ASSERT_FALSE(cpu_metric.is_null())
      << "system.cpu.utilization not found in collector output";
  EXPECT_NEAR(getGaugeValue(cpu_metric), 0.75, 0.001);
  auto cpu_attrs = getGaugeAttributes(cpu_metric);
  EXPECT_TRUE(hasStringAttribute(cpu_attrs, "cpu.mode", "active"))
      << "cpu.mode=active attribute not found on CPU metric";

  // Verify uptime metric.
  auto uptime_metric = findMetric(output, "system.uptime");
  ASSERT_FALSE(uptime_metric.is_null())
      << "system.uptime not found in collector output";
  EXPECT_NEAR(getGaugeValue(uptime_metric), 86400.0, 0.1);

  // Verify CPU time gauge.
  auto time_metric = findMetric(output, "system.cpu.time");
  ASSERT_FALSE(time_metric.is_null())
      << "system.cpu.time not found in collector output";
  EXPECT_TRUE(time_metric.contains("gauge"))
      << "system.cpu.time should be a Gauge metric";
  double time_val = getGaugeValue(time_metric);
  EXPECT_FALSE(std::isnan(time_val))
      << "system.cpu.time value not found in data point";
  EXPECT_NEAR(time_val, 12345.0, 0.1)
      << "system.cpu.time value incorrect";
  auto time_attrs = getGaugeAttributes(time_metric);
  EXPECT_TRUE(hasStringAttribute(time_attrs, "cpu.mode", "user"))
      << "cpu.mode=user attribute not found on CPU time metric";
}

// ---- Test 2: gRPC network metrics with direction + device ----

TEST_F(OTLPIntegrationTest, IntegrationGRPCNetworkMetrics) {
  startCollector();
  auto mgr = createManager("grpc");

  // Log network IO with network.io.direction and
  // network.interface.name attributes.
  mgr->logGauge(
      "system.network.io", "By", 1048576.0,
      {{"network.io.direction", "receive"},
       {"network.interface.name", "eth0"}});
  mgr->logGauge(
      "system.network.io", "By", 524288.0,
      {{"network.io.direction", "transmit"},
       {"network.interface.name", "eth0"}});
  mgr->logGauge(
      "system.network.packet.count", "{packet}", 1000.0,
      {{"network.io.direction", "receive"},
       {"network.interface.name", "eth0"}});
  mgr->logGauge(
      "system.network.errors", "{error}", 5.0,
      {{"network.io.direction", "receive"},
       {"network.interface.name", "eth0"}});

  mgr->forceFlush();

  auto output = waitForOutput("system.network.io");
  ASSERT_FALSE(output.empty()) << "No output from collector";

  // Verify network IO metric.
  auto net_metric = findMetric(output, "system.network.io");
  ASSERT_FALSE(net_metric.is_null())
      << "system.network.io not found in collector output";
  EXPECT_TRUE(net_metric.contains("gauge"))
      << "system.network.io should be a Gauge metric";
  ASSERT_TRUE(net_metric["gauge"].contains("dataPoints"));

  // Check that data points have network.io.direction
  // and network.interface.name attributes.
  bool found_receive = false;
  bool found_device = false;
  for (const auto& dp : net_metric["gauge"]["dataPoints"]) {
    if (!dp.contains("attributes")) {
      continue;
    }
    if (hasStringAttribute(
            dp["attributes"],
            "network.io.direction", "receive")) {
      found_receive = true;
    }
    if (hasStringAttribute(
            dp["attributes"],
            "network.interface.name", "eth0")) {
      found_device = true;
    }
  }
  EXPECT_TRUE(found_receive)
      << "network.io.direction=receive attribute not found";
  EXPECT_TRUE(found_device)
      << "network.interface.name=eth0 attribute not found";

  // Verify packets metric.
  auto pkt_metric = findMetric(output, "system.network.packet.count");
  ASSERT_FALSE(pkt_metric.is_null())
      << "system.network.packet.count not found in collector output";

  // Verify errors metric.
  auto err_metric = findMetric(output, "system.network.errors");
  ASSERT_FALSE(err_metric.is_null())
      << "system.network.errors not found in collector output";
}

// ---- Test 3: gRPC GPU metrics with hw.id, Slurm attrs, divide_by_100 ----

TEST_F(OTLPIntegrationTest, IntegrationGRPCGPUMetrics) {
  startCollector();
  auto mgr = createManager("grpc");

  // GPU utilization (divide_by_100 applied by caller: 85.0 -> 0.85).
  mgr->logGauge(
      "hw.gpu.utilization", "1", 0.85,
      {{"hw.gpu.task", "device"}, {"hw.id", "0"},
       {"job_id", "12345"}, {"username", "testuser"}});

  // GPU memory utilization (60.0 -> 0.60).
  mgr->logGauge(
      "hw.gpu.memory.utilization", "1", 0.60,
      {{"hw.id", "0"}, {"job_id", "12345"}, {"username", "testuser"}});

  // GPU power (no divide_by_100).
  mgr->logGauge(
      "hw.power", "W", 300.0,
      {{"hw.type", "gpu"}, {"hw.id", "0"},
       {"job_id", "12345"}, {"username", "testuser"}});

  // GPU pipe utilization.
  mgr->logGauge(
      "hw.gpu.pipe.utilization", "1", 0.30,
      {{"pipe", "fp16"}, {"hw.id", "0"}});

  mgr->forceFlush();

  auto output = waitForOutput("hw.gpu.utilization");
  ASSERT_FALSE(output.empty()) << "No output from collector";

  // Verify GPU utilization with divide_by_100 applied.
  auto gpu_util = findMetric(output, "hw.gpu.utilization");
  ASSERT_FALSE(gpu_util.is_null())
      << "hw.gpu.utilization not found in collector output";
  EXPECT_NEAR(getGaugeValue(gpu_util), 0.85, 0.001);
  auto gpu_attrs = getGaugeAttributes(gpu_util);
  EXPECT_TRUE(hasStringAttribute(gpu_attrs, "hw.gpu.task", "device"))
      << "hw.gpu.task=device not found";
  EXPECT_TRUE(hasStringAttribute(gpu_attrs, "hw.id", "0"))
      << "hw.id=0 not found";
  EXPECT_TRUE(hasStringAttribute(gpu_attrs, "job_id", "12345"))
      << "job_id Slurm attribute not found";
  EXPECT_TRUE(hasStringAttribute(gpu_attrs, "username", "testuser"))
      << "username Slurm attribute not found";

  // Verify GPU memory utilization.
  auto gpu_mem = findMetric(output, "hw.gpu.memory.utilization");
  ASSERT_FALSE(gpu_mem.is_null())
      << "hw.gpu.memory.utilization not found in collector output";
  EXPECT_NEAR(getGaugeValue(gpu_mem), 0.60, 0.001);

  // Verify GPU power.
  auto gpu_power = findMetric(output, "hw.power");
  ASSERT_FALSE(gpu_power.is_null())
      << "hw.power not found in collector output";
  EXPECT_NEAR(getGaugeValue(gpu_power), 300.0, 0.1);
  auto power_attrs = getGaugeAttributes(gpu_power);
  EXPECT_TRUE(hasStringAttribute(power_attrs, "hw.type", "gpu"))
      << "hw.type=gpu not found on power metric";

  // Verify pipe utilization.
  auto gpu_pipe = findMetric(output, "hw.gpu.pipe.utilization");
  ASSERT_FALSE(gpu_pipe.is_null())
      << "hw.gpu.pipe.utilization not found in collector output";
  EXPECT_NEAR(getGaugeValue(gpu_pipe), 0.30, 0.001);
  auto pipe_attrs = getGaugeAttributes(gpu_pipe);
  EXPECT_TRUE(hasStringAttribute(pipe_attrs, "pipe", "fp16"))
      << "pipe=fp16 not found";
}

// ---- Test 4: HTTP basic metrics (same as gRPC but over HTTP transport) ----

TEST_F(OTLPIntegrationTest, IntegrationHTTPBasicMetrics) {
  startCollector();
  auto mgr = createManager("http");

  mgr->logGauge(
      "system.cpu.utilization", "1", 0.50, {{"cpu.mode", "user"}});
  mgr->logGauge("system.uptime", "s", 3600.0, {});
  mgr->logGauge(
      "system.cpu.time", "s", 5000.0, {{"cpu.mode", "system"}});

  mgr->forceFlush();

  auto output = waitForOutput("system.cpu.utilization");
  ASSERT_FALSE(output.empty())
      << "No output from collector via HTTP transport";

  auto cpu_metric = findMetric(output, "system.cpu.utilization");
  ASSERT_FALSE(cpu_metric.is_null())
      << "system.cpu.utilization not found via HTTP";
  EXPECT_NEAR(getGaugeValue(cpu_metric), 0.50, 0.001);
  auto cpu_attrs = getGaugeAttributes(cpu_metric);
  EXPECT_TRUE(hasStringAttribute(cpu_attrs, "cpu.mode", "user"))
      << "cpu.mode=user not found via HTTP";

  auto uptime = findMetric(output, "system.uptime");
  ASSERT_FALSE(uptime.is_null())
      << "system.uptime not found via HTTP";
  EXPECT_NEAR(getGaugeValue(uptime), 3600.0, 0.1);

  auto time_metric = findMetric(output, "system.cpu.time");
  ASSERT_FALSE(time_metric.is_null())
      << "system.cpu.time not found via HTTP";
  EXPECT_TRUE(time_metric.contains("gauge"))
      << "system.cpu.time should be a Gauge metric via HTTP";
  double time_val = getGaugeValue(time_metric);
  EXPECT_FALSE(std::isnan(time_val))
      << "system.cpu.time value not found via HTTP";
  EXPECT_NEAR(time_val, 5000.0, 0.1)
      << "system.cpu.time value incorrect via HTTP";
}

// ---- Test 5: HTTP with custom headers ----

TEST_F(OTLPIntegrationTest, IntegrationHTTPHeaders) {
  startCollector();

  // Set custom headers. The collector does not require auth by default, but
  // we verify the export succeeds with custom headers (proving they don't
  // break the HTTP request and are properly formatted).
  auto mgr = createManager(
      "http", "Authorization=Bearer test-token-xyz,X-Custom=value123");

  mgr->logGauge(
      "system.cpu.utilization", "1", 0.42, {{"cpu.mode", "idle"}});

  mgr->forceFlush();

  auto output = waitForOutput("system.cpu.utilization");
  ASSERT_FALSE(output.empty())
      << "No output from collector — custom headers"
         " may have broken the request";

  auto metric = findMetric(output, "system.cpu.utilization");
  ASSERT_FALSE(metric.is_null())
      << "Metric not found — headers may have been malformed";
  EXPECT_NEAR(getGaugeValue(metric), 0.42, 0.001);
  auto attrs = getGaugeAttributes(metric);
  EXPECT_TRUE(hasStringAttribute(attrs, "cpu.mode", "idle"))
      << "cpu.mode=idle not found with custom headers";
}

// ---- Test 6: Resource attributes (service.name, host.name, os.type) ----

TEST_F(OTLPIntegrationTest, IntegrationResourceAttributes) {
  startCollector();
  auto mgr = createManager("grpc");

  // Log any metric so we have output to inspect.
  mgr->logGauge("system.uptime", "s", 1.0, {});
  mgr->forceFlush();

  auto output = waitForOutput("system.uptime");
  ASSERT_FALSE(output.empty()) << "No output from collector";

  // Verify resource attributes.
  auto resource_attrs = getResourceAttributes(output);
  ASSERT_FALSE(resource_attrs.empty())
      << "No resource attributes found in collector output";

  EXPECT_TRUE(
      hasStringAttribute(resource_attrs, "service.name", "dynolog-integ-test"))
      << "service.name resource attribute missing or wrong value";
  EXPECT_TRUE(hasAttributeKey(resource_attrs, "host.name"))
      << "host.name resource attribute missing";
  EXPECT_TRUE(hasStringAttribute(resource_attrs, "os.type", "linux"))
      << "os.type resource attribute missing or wrong value";
  EXPECT_TRUE(hasAttributeKey(resource_attrs, "service.version"))
      << "service.version resource attribute missing";
}

#endif // OTLP_INTEGRATION_TEST

} // namespace dynolog
