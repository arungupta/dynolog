# Logging to OTLP (OpenTelemetry Protocol)

Dynolog supports exporting metrics via the [OpenTelemetry Protocol (OTLP)](https://opentelemetry.io/docs/specs/otlp/), enabling integration with any OTLP-compatible observability backend such as Grafana (via OTEL Collector), Datadog, New Relic, Honeycomb, Splunk, and others.

## Building with OTLP Support

OTLP support requires the [opentelemetry-cpp](https://github.com/open-telemetry/opentelemetry-cpp) SDK. To build dynolog with OTLP enabled:

```bash
# Using the build script
BUILD_OTLP=1 ./scripts/build.sh

# Or manually with cmake
mkdir -p build && cd build
cmake -DUSE_OTLP=ON -DCMAKE_BUILD_TYPE=Release -G Ninja ..
cmake --build .
```

The build will compile the opentelemetry-cpp SDK from the `third_party/opentelemetry-cpp` submodule. Make sure to initialize submodules first:

```bash
git submodule update --init --recursive
```

## Configuration

### Command Line Flags

| Flag | Default | Description |
| --- | --- | --- |
| `--use_otlp` | `false` | Enable OTLP metrics export |
| `--otlp_endpoint` | `""` | OTLP endpoint URL. For gRPC: `http://localhost:4317`. For HTTP: `http://localhost:4318/v1/metrics` |
| `--otlp_protocol` | `grpc` | Transport protocol: `grpc` or `http` |
| `--otlp_service_name` | `dynolog` | Service name reported in OTLP resource attributes |
| `--otlp_headers` | `""` | Custom headers as `key1=value1,key2=value2` (for authentication tokens, etc.) |
| `--otlp_timeout_ms` | `30000` | Export timeout in milliseconds |
| `--otlp_export_interval_ms` | `60000` | Periodic export interval in milliseconds. Should be greater than `--otlp_timeout_ms`; if not, the OTEL SDK may reset both to defaults. |
| `--otlp_use_tls` | `false` | Enable TLS for gRPC transport |

### Environment Variable Overrides

The following standard OpenTelemetry environment variables take precedence over command line flags:

| Environment Variable | Overrides Flag |
| --- | --- |
| `OTEL_EXPORTER_OTLP_ENDPOINT` | `--otlp_endpoint` |
| `OTEL_EXPORTER_OTLP_PROTOCOL` | `--otlp_protocol` |
| `OTEL_SERVICE_NAME` | `--otlp_service_name` |
| `OTEL_EXPORTER_OTLP_HEADERS` | `--otlp_headers` |

### Resource Attributes

The following resource attributes are automatically set on all exported metrics:

| Attribute | Value |
| --- | --- |
| `service.name` | From `--otlp_service_name` flag (default: `dynolog`) |
| `service.version` | Compile-time version from `version.txt` |
| `host.name` | System hostname |
| `os.type` | `linux` |

## Usage Examples

### Basic: Export to local OTEL Collector (gRPC)

```bash
dynolog --use_otlp --otlp_endpoint=http://localhost:4317
```

### Export to local OTEL Collector (HTTP)

```bash
dynolog --use_otlp --otlp_protocol=http --otlp_endpoint=http://localhost:4318/v1/metrics
```

### Export with authentication header

```bash
dynolog --use_otlp --otlp_endpoint=https://otlp.example.com:4317 \
  --otlp_headers="Authorization=Bearer mytoken" --otlp_use_tls
```

### Using environment variables

```bash
export OTEL_EXPORTER_OTLP_ENDPOINT=http://collector:4317
export OTEL_SERVICE_NAME=dynolog-gpu-node-01
dynolog --use_otlp --enable_gpu_monitor
```

### Combined with other loggers

OTLP can be used alongside other loggers (Prometheus, JSON, etc.):

```bash
dynolog --use_otlp --otlp_endpoint=http://localhost:4317 \
  --use_prometheus --use_JSON
```

### With systemd

```bash
echo "--use_otlp" | sudo tee -a /etc/dynolog.gflags
echo "--otlp_endpoint=http://collector:4317" | sudo tee -a /etc/dynolog.gflags
sudo systemctl restart dynolog
```

## Metric Naming

Dynolog maps its internal metric names to [OpenTelemetry Semantic Conventions](https://opentelemetry.io/docs/specs/semconv/). CPU and system metrics use the `system.*` namespace, while GPU metrics use the `hw.gpu.*` namespace.

For the complete metric mapping table, see [Metrics.md](Metrics.md#otlp-metric-naming).

### Key Conversions

- **CPU utilization percentages** (e.g., `cpu_util=75.0`) are converted to ratios (`system.cpu.utilization=0.75`) per OTEL conventions.
- **Network metrics** (e.g., `rx_bytes.eth0`) are split into a base metric name (`system.network.io`) with `network.io.direction` and `network.interface.name` attributes.
- **GPU metrics** are tagged with `hw.id` for device identification and Slurm attribution labels (`job_id`, `username`, etc.) when available.
- **Unknown metrics** are passed through with a `dynolog.` prefix (e.g., `custom_metric` becomes `dynolog.custom_metric`).

## Architecture

The OTLP logger consists of two components:

1. **OTLPLogger**: Implements the `Logger` interface. Accumulates metric values via `logInt()`/`logFloat()`/`logUint()` calls, then on `finalize()` maps them to OTEL conventions and records them on OTel instruments.

2. **OTLPManager**: Thread-safe singleton that owns the OTel `MeterProvider`, `Meter`, and cached instrument handles. Shared across the three collector threads (kernel, perf, GPU) via `CompositeLogger`.
