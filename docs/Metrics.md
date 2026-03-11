# Metric Definitions

## Concepts
We define a metric as a quantity measured over a given period of time. The time aspect may be crucial because each value measured has an associated time range `[time_start, duration]`. In some cases however, metrics can have instantaneous values as described below.

### Types of metrics
* **Delta metrics:** are changes in a quantity over a given time interval. Examples include the number of bytes transferred over a port. Each delta must have an associated time period to make it meaningful. (100MB was transferred over the network; in how much time?) We can also report these metrics as cumulative values, like 1000GB of total bytes were transferred by XYZ time. Deltas can be calculated by subtracting successive values.
* **Instantaneous metrics:** some metrics can have instantaneous values; this is typical for capacity metrics like the amount of memory used.
* **Ratio metrics:** are useful for measuring the utilization of a unit or component. For example, a cpu core was utilized 50% on average over the last minute.
* **Rate metrics:** are divided by the time interval of measurement. We still need to be aware about the interval over which the rate is averaged. For example, the bytes/sec measured every second vs every minute may have totally different values. The average over a second can have more variation than average over a minute.

### Scopes
A metric is generally associated with a scope that describes the space/dimension it covers. Examples of scope include: server, GPU devices, NIC card etc.

## CPU/System Metrics
| Metric | Description | What does it tell? | Type | Unit | Collection Interval |
| ----------- | ----------- |----------- |----------- |----------- |----------- |
| uptime | System uptime | How long the system has been running. | Instant | s | 60s |
| cpu_util | Fraction of total CPU time spend on user or system mode.| Overall amount of time the CPU was busy. | Ratio | - | 60s |
| cpu_u, cpu_s | Fraction of total CPU time spent in user and system mode respectively| Activity of the system CPU in user/system mode. | Ratio | - | 60s |
| cpu_i| Fraction of total CPU time that the CPU was in idle mode.| Overall inactivity of the system CPU. | Ratio | - | 60s |
| cpu_guest, cpu_guest_nice | Fraction of total CPU time spent running a virtual CPU for guest OS. | Guest/virtualization CPU overhead. | Ratio | - | 60s |
| cpu_u/s/n/w/x/y_ms| Total CPU time in milliseconds spent in various modes: user, system, nice, iowait etc. For more details please see man page for [/proc/stat](https://man7.org/linux/man-pages/man5/proc.5.html) | Activity of the system CPU in various modes. | Delta | ms | 60s |
| rx/tx_bytes.`<devname>` | Total bytes transmitted/received over the specific network device.| Network transfer statistics. | Delta | Bytes | 60s |
| rx/tx_packets.`<devname>` | Total packets transmitted/received over the specific network device.| Network transfer statistics. | Delta | Packets | 60s |
| rx/tx_errors.`<devname>` | Total transmit/receive errors on the specific network device.| Network transfer statistics. | Delta | Errors | 60s |
| rx/tx_drops.`<devname>` | Total transmit/receive packet drops on the specific network device.| Network transfer statistics. | Delta | Packets | 60s |
| mips | Number of million instructions executed per second. | Overall rate of instructions per second the CPU executed. | Rate | Million / sec | 60s |
| mega_cycles_per_second | Number of active CPU clock cycles per second. | Overall rate of the CPU clock cycle. | Rate | MHz | 60s |

## GPU Metrics
| Metric | Description | What does it tell? | Type | Unit | Collection Interval |
| ----------- | ----------- |----------- |----------- |----------- |----------- |
| graphics_engine_active_ratio | Ratio of time the graphics/compute engine is active | A coarse metric showing whether the gpu doing active work | Ratio | - | 10s |
| sm_active_ratio | Ratio of active SMs (streaming multiprocessor) | GPU usage at SM level | Ratio | - | 10s |
| sm_occupancy | Ratio of active warps | GPU usage at warps level | Ratio | - | 10s |
| gpu_frequency_mhz | SM clock of the GPU device | The frequency of the SM | Frequency | Mega Hertz | 10s |
| fp16_active | Ratio of cycles the fp16 pipe is active | fp16 compute pipe active ratio | Ratio | - | 10s |
| fp32_active | Ratio of cycles the fp32 pipe is active | fp32 compute pipe active ratio | Ratio | - | 10s |
| fp64_active | Ratio of cycles the fp64 pipe is active | fp64 compute pipe active ratio | Ratio | - | 10s |
| tensorcore_active | Ratio of cycles the tensor pipe is active | tensor compute pipe active ratio | Ratio | - | 10s |
| hbm_mem_bw_util | Ratio of cycles the high performance memory interface is active | Percentage of GPU memory bandwidth | Ratio | - | 10s |
| pcie_tx_bytes | Rate of transmitted bytes (bandwidth) over Pcie during the sampling interval | Pcie tx bandwidth | Bandwidth | Bytes/sec | 10s |
| pcie_rx_bytes | Rate of received bytes (bandwidth) over Pcie during the sampling interval | Pcie rx bandwidth | Bandwidth | Bytes/sec | 10s |
| nvlink_tx_bytes | Rate of transmitted bytes (bandwidth) on Nvlink during the sampling interval | NVLink tx bandwidth | Bandwidth | Bytes/sec | 10s |
| nvlink_rx_bytes | Rate of received bytes (bandwidth) on Nvlink during the sampling interval | NVLink rx bandwidth | Bandwidth | Bytes/sec | 10s |
| minor_id | Minor id of the device | Linux minor id of the device | ID | - | 10s |
| gpu_device_utilization | GPU Device utilization | The most coarse signal showing the GPU is active | Ratio | - | 10s |
| gpu_memory_utilization | Memory utilization of the GPU device | Ratio of GPU memory being allocated | Ratio | - | 10s |
| gpu_power_draw | Power of the device | How much power the GPU is consuming, also reflects how heavy the GPU is used | Power | Watt | 10s |
| dcgm_error | DCGM hardware error count | Number of DCGM-reported hardware errors for the device | Delta | Count | 10s |

## OTLP Metric Naming

When using the OTLP logger (`--use_otlp`), dynolog maps its internal metric names to [OpenTelemetry Semantic Conventions](https://opentelemetry.io/docs/specs/semconv/). This enables interoperability with any OTLP-compatible backend.

### CPU/System Metric Mapping
| Dynolog Metric | OTEL Name | OTEL Unit | OTEL Type | Attributes | Notes |
| --- | --- | --- | --- | --- | --- |
| cpu_util | system.cpu.utilization | 1 | Gauge | cpu.mode=active | Divided by 100 (% to ratio) |
| cpu_u | system.cpu.utilization | 1 | Gauge | cpu.mode=user | Divided by 100 |
| cpu_s | system.cpu.utilization | 1 | Gauge | cpu.mode=system | Divided by 100 |
| cpu_i | system.cpu.utilization | 1 | Gauge | cpu.mode=idle | Divided by 100 |
| cpu_guest | system.cpu.utilization | 1 | Gauge | cpu.mode=guest | Divided by 100 |
| cpu_guest_nice | system.cpu.utilization | 1 | Gauge | cpu.mode=guest_nice | Divided by 100 |
| cpu_u_node{N} | system.cpu.utilization | 1 | Gauge | cpu.mode=user, cpu.socket={N} | Divided by 100; per-NUMA-socket |
| cpu_s_node{N} | system.cpu.utilization | 1 | Gauge | cpu.mode=system, cpu.socket={N} | Divided by 100; per-NUMA-socket |
| cpu_i_node{N} | system.cpu.utilization | 1 | Gauge | cpu.mode=idle, cpu.socket={N} | Divided by 100; per-NUMA-socket |
| cpu_u_ms | system.cpu.time | s | Gauge | cpu.mode=user | |
| cpu_s_ms | system.cpu.time | s | Gauge | cpu.mode=system | |
| cpu_n_ms | system.cpu.time | s | Gauge | cpu.mode=nice | |
| cpu_w_ms | system.cpu.time | s | Gauge | cpu.mode=iowait | |
| cpu_x_ms | system.cpu.time | s | Gauge | cpu.mode=irq | |
| cpu_y_ms | system.cpu.time | s | Gauge | cpu.mode=softirq | |
| cpu_z_ms | system.cpu.time | s | Gauge | cpu.mode=steal | |
| cpu_guest_ms | system.cpu.time | s | Gauge | cpu.mode=guest | |
| cpu_guest_nice_ms | system.cpu.time | s | Gauge | cpu.mode=guest_nice | |
| uptime | system.uptime | s | Gauge | | |
| mips | dynolog.mips | | Gauge | | Passthrough (value is millions of instructions/s) |
| mega_cycles_per_second | dynolog.mega_cycles_per_second | | Gauge | | Passthrough (value is millions of cycles/s) |

### Network Metric Mapping
| Dynolog Metric | OTEL Name | OTEL Unit | OTEL Type | Attributes |
| --- | --- | --- | --- | --- |
| rx_bytes.{dev} | system.network.io | By | Gauge | network.io.direction=receive, network.interface.name={dev} |
| tx_bytes.{dev} | system.network.io | By | Gauge | network.io.direction=transmit, network.interface.name={dev} |
| rx_packets.{dev} | system.network.packet.count | {packet} | Gauge | network.io.direction=receive, network.interface.name={dev} |
| tx_packets.{dev} | system.network.packet.count | {packet} | Gauge | network.io.direction=transmit, network.interface.name={dev} |
| rx_errors.{dev} | system.network.errors | {error} | Gauge | network.io.direction=receive, network.interface.name={dev} |
| tx_errors.{dev} | system.network.errors | {error} | Gauge | network.io.direction=transmit, network.interface.name={dev} |
| rx_drops.{dev} | system.network.packet.dropped | {packet} | Gauge | network.io.direction=receive, network.interface.name={dev} |
| tx_drops.{dev} | system.network.packet.dropped | {packet} | Gauge | network.io.direction=transmit, network.interface.name={dev} |

### GPU Metric Mapping
GPU metrics use the `hw.gpu.*` namespace following OTEL hardware semantic conventions. The `hw.id` attribute identifies the GPU device.

| Dynolog Metric | OTEL Name | OTEL Unit | Additional Attributes | Notes |
| --- | --- | --- | --- | --- |
| graphics_engine_active_ratio | hw.gpu.utilization | 1 | hw.gpu.task=general | |
| sm_active_ratio | hw.gpu.sm.utilization | 1 | | |
| sm_occupancy | hw.gpu.sm.occupancy | 1 | | |
| gpu_frequency_mhz | hw.gpu.frequency | MHz | | |
| fp16_active | hw.gpu.pipe.utilization | 1 | pipe=fp16 | |
| fp32_active | hw.gpu.pipe.utilization | 1 | pipe=fp32 | |
| fp64_active | hw.gpu.pipe.utilization | 1 | pipe=fp64 | |
| tensorcore_active | hw.gpu.pipe.utilization | 1 | pipe=tensorcore | |
| hbm_mem_bw_util | hw.gpu.memory.bandwidth.utilization | 1 | | |
| pcie_tx_bytes | hw.gpu.io | By/s | direction=transmit | |
| pcie_rx_bytes | hw.gpu.io | By/s | direction=receive | |
| nvlink_tx_bytes | hw.gpu.nvlink.io | By/s | direction=transmit | |
| nvlink_rx_bytes | hw.gpu.nvlink.io | By/s | direction=receive | |
| gpu_device_utilization | hw.gpu.utilization | 1 | hw.gpu.task=device | Divided by 100 (% to ratio) |
| gpu_memory_utilization | hw.gpu.memory.utilization | 1 | | Divided by 100 (% to ratio) |
| gpu_power_draw | hw.power | W | hw.type=gpu | |
| dcgm_error | hw.errors | {error} | hw.type=gpu | |

### ARM Hardware Counter Metric Mapping
ARM hardware counter metrics are mapped to the `system.cpu.*` namespace. These metrics use attributes to distinguish TLB levels, operation types, and other hardware event characteristics.

> **Note:** These metrics are only available on ARM Neoverse V2 hosts with hardware performance counter monitoring enabled.

| Dynolog Metric | OTEL Name | OTEL Unit | Attributes | Description |
| --- | --- | --- | --- | --- |
| l1d_tlb | system.cpu.tlb.operations | {event} | level=l1d, type=access | L1 data TLB accesses |
| l1d_tlb_refill | system.cpu.tlb.operations | {event} | level=l1d, type=miss | L1 data TLB misses |
| l1i_tlb | system.cpu.tlb.operations | {event} | level=l1i, type=access | L1 instruction TLB accesses |
| l1i_tlb_refill | system.cpu.tlb.operations | {event} | level=l1i, type=miss | L1 instruction TLB misses |
| l2d_tlb | system.cpu.tlb.operations | {event} | level=l2, type=access | L2 unified TLB accesses |
| l2d_tlb_refill | system.cpu.tlb.operations | {event} | level=l2, type=miss | L2 unified TLB misses |
| stall_backend_mem | system.cpu.stalls | {cycle} | reason=backend_mem | Backend memory stall cycles |
| ll_cache_miss_rd | system.cpu.cache.misses | {event} | level=ll | Last-level cache read misses |
| br_mis_pred | system.cpu.branch.mispredictions | {event} | — | Branch mispredictions |
| br_retired | system.cpu.branch.instructions | {instruction} | — | Branch instructions retired |
| l1i_cache_refill | system.cpu.cache.operations | {event} | level=l1i, type=miss | L1 instruction cache refills (misses) |
| l1d_cache_refill | system.cpu.cache.operations | {event} | level=l1d, type=miss | L1 data cache refills (misses) |
| l2d_cache_refill | system.cpu.cache.operations | {event} | level=l2, type=miss | L2 data cache refills (misses) |
| l3d_cache_refill | system.cpu.cache.operations | {event} | level=l3, type=miss | L3 data cache refills (misses) |
| FP_HP_SPEC | system.cpu.fp.operations | {operation} | precision=half | Half-precision floating-point operations (speculative) |
| FP_SP_SPEC | system.cpu.fp.operations | {operation} | precision=single | Single-precision floating-point operations (speculative) |
| FP_DP_SPEC | system.cpu.fp.operations | {operation} | precision=double | Double-precision floating-point operations (speculative) |
| dtlb_walk | system.cpu.tlb.walks | {event} | type=data | Data TLB page table walks |
| itlb_walk | system.cpu.tlb.walks | {event} | type=instruction | Instruction TLB page table walks |

For detailed setup instructions, see [docs/logging_to_otlp.md](logging_to_otlp.md).
