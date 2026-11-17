/*
 * Host UDP benchmark for the OpenSOME/IP library.
 *
 * Method
 * ------
 * Each case warms up for 200 ms, then measures for 1 second.
 * One sample is one completed operation (see table below).
 * The run fails if a case completes fewer than 100 operations.
 * Timing uses std::chrono::steady_clock.
 *
 * Cases
 * -----
 * serialize      – Message::serialize then try_deserialize; payload round-trips.
 *                   Payloads: 0, 64, 1400 bytes.
 * udp_rpc        – RpcClient::call_method_sync against an RpcServer on a
 *                   UdpTransport.  Response payload equals request payload.
 *                   Payloads: 64, 1400 bytes.
 * udp_notify     – EventPublisher::publish_event to one unicast subscriber
 *                   (multicast_threshold 0).  The subscriber has received that
 *                   session.  Payloads: 64, 1400 bytes.
 * udp_rpc_demux  – Same as udp_rpc with the server registered on a ServiceDemux
 *                   that also has a second service id registered and idle.
 *                   Payload: 64 bytes.
 *
 * Latency
 * -------
 * serialize  – serialize + deserialize wall clock.
 * udp_rpc    – synchronous round trip.
 * udp_notify – publish until the subscriber observes that session.
 *
 * Report
 * ------
 * Writes build/perf/host-bench.json and prints a table to stdout.
 */

#include "someip/message.h"
#include "someip/types.h"
#include "common/result.h"
#include "rpc/rpc_client.h"
#include "rpc/rpc_server.h"
#include "rpc/rpc_types.h"
#include "events/event_publisher.h"
#include "events/event_subscriber.h"
#include "events/event_types.h"
#include "transport/udp_transport.h"
#include "transport/endpoint.h"
#include "transport/service_demux.h"
#include "platform/buffer_pool.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#endif

using Clock = std::chrono::steady_clock;

static constexpr auto WARMUP     = std::chrono::milliseconds(200);
static constexpr auto MEASURE    = std::chrono::seconds(1);
static constexpr int  MIN_OPS    = 100;
static constexpr uint16_t BASE_PORT = 40000;

struct Latencies {
    int64_t p50_ns{0};
    int64_t p99_ns{0};
    int64_t max_ns{0};
};

struct BenchResult {
    std::string name;
    int payload_bytes{0};
    int64_t samples{0};
    int64_t duration_ns{0};
    double ops_per_sec{0};
    double bytes_per_sec{0};
    Latencies latency;
};

static Latencies compute_latencies(std::vector<int64_t>& lat_ns) {
    Latencies l;
    if (lat_ns.empty()) return l;
    std::sort(lat_ns.begin(), lat_ns.end());
    auto n = lat_ns.size();
    l.p50_ns = lat_ns[n * 50 / 100];
    l.p99_ns = lat_ns[std::min(n - 1, n * 99 / 100)];
    l.max_ns = lat_ns.back();
    return l;
}

static someip::platform::ByteBuffer make_payload(size_t size) {
    someip::platform::ByteBuffer buf(size);
    for (size_t i = 0; i < size; ++i)
        buf[i] = static_cast<uint8_t>(i & 0xFF);
    return buf;
}

// ---------- serialize benchmark ----------

static BenchResult bench_serialize(int payload_bytes) {
    auto payload = make_payload(static_cast<size_t>(payload_bytes));

    someip::Message msg;
    msg.set_service_id(0x1234);
    msg.set_method_id(0x0001);
    msg.set_message_type(someip::MessageType::REQUEST);
    msg.set_payload(payload);

    // warmup
    auto warmup_end = Clock::now() + WARMUP;
    while (Clock::now() < warmup_end) {
        auto wire = msg.serialize();
        someip::Message dec;
        dec.try_deserialize(wire);
    }

    std::vector<int64_t> lat_ns;
    lat_ns.reserve(500000);
    int64_t samples = 0;

    auto t0 = Clock::now();
    auto deadline = t0 + MEASURE;
    while (Clock::now() < deadline) {
        auto op_start = Clock::now();
        auto wire = msg.serialize();
        someip::Message dec;
        auto r = dec.try_deserialize(wire);
        auto op_end = Clock::now();

        if (r != someip::Result::SUCCESS) continue;
        if (dec.get_payload() != payload) continue;

        lat_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count());
        ++samples;
    }
    auto t1 = Clock::now();
    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    BenchResult res;
    res.name = "serialize";
    res.payload_bytes = payload_bytes;
    res.samples = samples;
    res.duration_ns = dur_ns;
    res.ops_per_sec = dur_ns > 0 ? static_cast<double>(samples) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.bytes_per_sec = dur_ns > 0 ? static_cast<double>(samples) * static_cast<double>(payload_bytes) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.latency = compute_latencies(lat_ns);
    return res;
}

// ---------- udp_rpc benchmark ----------

static BenchResult bench_udp_rpc(int payload_bytes, uint16_t server_port) {
    using namespace someip::rpc;
    using namespace someip::transport;

    const uint16_t service_id = 0x1000;
    const MethodId method_id = 0x0001;

    RpcServer server(service_id, 0x01, Endpoint("127.0.0.1", server_port));
    server.register_method(method_id, [](uint16_t, uint16_t,
            const someip::platform::ByteBuffer& input,
            someip::platform::ByteBuffer& output) -> RpcResult {
        output = input;
        return RpcResult::SUCCESS;
    });
    server.initialize();

    RpcClient client(0x0001, 0x01, Endpoint("0.0.0.0", 0));
    client.set_remote_endpoint(Endpoint("127.0.0.1", server_port));
    client.initialize();

    auto payload = make_payload(static_cast<size_t>(payload_bytes));

    // warmup
    auto warmup_end = Clock::now() + WARMUP;
    while (Clock::now() < warmup_end) {
        client.call_method_sync(service_id, method_id, payload);
    }

    std::vector<int64_t> lat_ns;
    lat_ns.reserve(100000);
    int64_t samples = 0;

    auto t0 = Clock::now();
    auto deadline = t0 + MEASURE;
    while (Clock::now() < deadline) {
        auto op_start = Clock::now();
        auto result = client.call_method_sync(service_id, method_id, payload);
        auto op_end = Clock::now();

        if (result.result != RpcResult::SUCCESS) continue;

        lat_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count());
        ++samples;
    }
    auto t1 = Clock::now();
    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    client.shutdown();
    server.shutdown();

    BenchResult res;
    res.name = "udp_rpc";
    res.payload_bytes = payload_bytes;
    res.samples = samples;
    res.duration_ns = dur_ns;
    res.ops_per_sec = dur_ns > 0 ? static_cast<double>(samples) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.bytes_per_sec = dur_ns > 0 ? static_cast<double>(samples) * static_cast<double>(payload_bytes) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.latency = compute_latencies(lat_ns);
    return res;
}

// ---------- udp_notify benchmark ----------

static BenchResult bench_udp_notify(int payload_bytes, uint16_t pub_port, uint16_t sub_port) {
    using namespace someip::events;
    using namespace someip::transport;

    const uint16_t service_id = 0x2000;
    const uint16_t instance_id = 0x0001;
    const uint16_t event_id = 0x8001;
    const uint16_t eventgroup_id = 0x0001;
    const uint16_t subscriber_client_id = 0x0010;

    UdpTransport pub_transport(Endpoint("127.0.0.1", pub_port));
    EventPublisher publisher(service_id, instance_id, pub_transport);

    EventConfig evt_cfg;
    evt_cfg.event_id = event_id;
    evt_cfg.eventgroup_id = eventgroup_id;
    evt_cfg.reliability = Reliability::UNRELIABLE;
    publisher.register_event(evt_cfg);

    EventgroupConfig eg_cfg;
    eg_cfg.eventgroup_id = eventgroup_id;
    eg_cfg.multicast_threshold = 0;
    publisher.configure_eventgroup(eg_cfg);

    publisher.set_default_client_endpoint("127.0.0.1", sub_port);
    publisher.initialize();
    publisher.handle_subscription(eventgroup_id, subscriber_client_id);

    UdpTransport sub_transport(Endpoint("127.0.0.1", sub_port));
    EventSubscriber subscriber(subscriber_client_id, sub_transport);
    subscriber.set_default_endpoint("127.0.0.1", pub_port);

    std::atomic<int64_t> recv_count{0};
    subscriber.initialize();
    subscriber.subscribe_eventgroup(service_id, instance_id, eventgroup_id,
        [&recv_count](const EventNotification&) {
            recv_count.fetch_add(1, std::memory_order_release);
        });

    auto payload = make_payload(static_cast<size_t>(payload_bytes));

    // warmup
    int64_t send_count = 0;
    auto warmup_end = Clock::now() + WARMUP;
    while (Clock::now() < warmup_end) {
        publisher.publish_event(event_id, payload);
        ++send_count;
        while (recv_count.load(std::memory_order_acquire) < send_count) {}
    }

    std::vector<int64_t> lat_ns;
    lat_ns.reserve(100000);
    int64_t samples = 0;

    auto t0 = Clock::now();
    auto deadline = t0 + MEASURE;
    while (Clock::now() < deadline) {
        auto op_start = Clock::now();
        publisher.publish_event(event_id, payload);
        ++send_count;
        while (recv_count.load(std::memory_order_acquire) < send_count) {}
        auto op_end = Clock::now();

        lat_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count());
        ++samples;
    }
    auto t1 = Clock::now();
    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    subscriber.shutdown();
    publisher.shutdown();

    BenchResult res;
    res.name = "udp_notify";
    res.payload_bytes = payload_bytes;
    res.samples = samples;
    res.duration_ns = dur_ns;
    res.ops_per_sec = dur_ns > 0 ? static_cast<double>(samples) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.bytes_per_sec = dur_ns > 0 ? static_cast<double>(samples) * static_cast<double>(payload_bytes) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.latency = compute_latencies(lat_ns);
    return res;
}

// ---------- udp_rpc_demux benchmark ----------

static BenchResult bench_udp_rpc_demux(int payload_bytes, uint16_t demux_port) {
    using namespace someip::rpc;
    using namespace someip::transport;

    const uint16_t service_id_a = 0x3000;
    const uint16_t service_id_b = 0x3001;
    const MethodId method_id = 0x0001;

    UdpTransport transport(Endpoint("127.0.0.1", demux_port));
    ServiceDemux demux(transport);

    RpcServer server_a(service_id_a, demux);
    server_a.register_method(method_id, [](uint16_t, uint16_t,
            const someip::platform::ByteBuffer& input,
            someip::platform::ByteBuffer& output) -> RpcResult {
        output = input;
        return RpcResult::SUCCESS;
    });
    server_a.initialize();

    RpcServer server_b(service_id_b, demux);
    server_b.register_method(method_id, [](uint16_t, uint16_t,
            const someip::platform::ByteBuffer&,
            someip::platform::ByteBuffer& output) -> RpcResult {
        output.clear();
        return RpcResult::SUCCESS;
    });
    server_b.initialize();

    (void)demux.start();

    RpcClient client(0x0002, 0x01, Endpoint("0.0.0.0", 0));
    client.set_remote_endpoint(Endpoint("127.0.0.1", demux_port));
    client.initialize();

    auto payload = make_payload(static_cast<size_t>(payload_bytes));

    // warmup
    auto warmup_end = Clock::now() + WARMUP;
    while (Clock::now() < warmup_end) {
        client.call_method_sync(service_id_a, method_id, payload);
    }

    std::vector<int64_t> lat_ns;
    lat_ns.reserve(100000);
    int64_t samples = 0;

    auto t0 = Clock::now();
    auto deadline = t0 + MEASURE;
    while (Clock::now() < deadline) {
        auto op_start = Clock::now();
        auto result = client.call_method_sync(service_id_a, method_id, payload);
        auto op_end = Clock::now();

        if (result.result != RpcResult::SUCCESS) continue;

        lat_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count());
        ++samples;
    }
    auto t1 = Clock::now();
    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    client.shutdown();
    (void)demux.stop();
    server_b.shutdown();
    server_a.shutdown();

    BenchResult res;
    res.name = "udp_rpc_demux";
    res.payload_bytes = payload_bytes;
    res.samples = samples;
    res.duration_ns = dur_ns;
    res.ops_per_sec = dur_ns > 0 ? static_cast<double>(samples) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.bytes_per_sec = dur_ns > 0 ? static_cast<double>(samples) * static_cast<double>(payload_bytes) * 1e9 / static_cast<double>(dur_ns) : 0;
    res.latency = compute_latencies(lat_ns);
    return res;
}

// ---------- system info ----------

static std::string get_os_info() {
#ifdef _WIN32
    return "Windows";
#else
    struct utsname u;
    if (uname(&u) == 0)
        return std::string(u.sysname) + " " + u.release + " " + u.machine;
    return "unknown";
#endif
}

static std::string get_cpu_info() {
#ifdef __APPLE__
    char buf[256] = {};
    FILE* f = popen("sysctl -n machdep.cpu.brand_string 2>/dev/null", "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            pclose(f);
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            return s;
        }
        pclose(f);
    }
    return "unknown";
#elif defined(__linux__)
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                fclose(f);
                char* colon = strchr(line, ':');
                if (colon) {
                    std::string s(colon + 2);
                    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                        s.pop_back();
                    return s;
                }
            }
        }
        fclose(f);
    }
    return "unknown";
#else
    return "unknown";
#endif
}

static std::string get_compiler_info() {
#if defined(__clang__)
    return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("gcc ") + std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return std::string("MSVC ") + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

static std::string get_git_sha() {
#ifdef HOST_BENCH_GIT_SHA
    return HOST_BENCH_GIT_SHA;
#else
    return "unknown";
#endif
}

static int64_t get_static_footprint() {
#ifdef HOST_BENCH_STATIC_FOOTPRINT
    return HOST_BENCH_STATIC_FOOTPRINT;
#else
    return 0;
#endif
}

// ---------- JSON + table output ----------

static std::string escape_json(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            default:   out += c; break;
        }
    }
    return out;
}

static void write_json(const std::string& path,
                       const std::vector<BenchResult>& results) {
    auto git_sha = get_git_sha();
    auto compiler = get_compiler_info();
    auto os = get_os_info();
    auto cpu = get_cpu_info();
    auto footprint = get_static_footprint();

    std::ofstream f(path);
    f << "{\n";
    f << R"(  "schema": 1,)" << "\n";
    f << R"(  "git_sha": ")" << escape_json(git_sha) << "\",\n";
    f << R"(  "build_type": "Release",)" << "\n";
    f << R"(  "static_alloc": false,)" << "\n";
    f << R"(  "compiler": ")" << escape_json(compiler) << "\",\n";
    f << R"(  "os": ")" << escape_json(os) << "\",\n";
    f << R"(  "cpu": ")" << escape_json(cpu) << "\",\n";
    f << R"(  "cases": [)" << "\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        f << "    {\n";
        f << R"(      "name": ")" << r.name << "\",\n";
        f << R"(      "payload_bytes": )" << r.payload_bytes << ",\n";
        f << R"(      "samples": )" << r.samples << ",\n";
        f << R"(      "duration_ns": )" << r.duration_ns << ",\n";
        f << R"(      "ops_per_sec": )" << static_cast<int64_t>(r.ops_per_sec) << ",\n";
        f << R"(      "bytes_per_sec": )" << static_cast<int64_t>(r.bytes_per_sec) << ",\n";
        f << R"(      "latency_ns": {)"
          << R"("p50": )" << r.latency.p50_ns
          << R"(, "p99": )" << r.latency.p99_ns
          << R"(, "max": )" << r.latency.max_ns
          << "}\n";
        f << "    }";
        if (i + 1 < results.size()) f << ",";
        f << "\n";
    }

    f << "  ],\n";
    f << R"(  "static_footprint": {)" << "\n";
    f << R"(    "source": "scripts/static_memory_budget.py",)" << "\n";
    f << R"(    "kind": "estimate of reserved static storage, not a measured process size",)" << "\n";
    f << R"(    "total_bytes": )" << footprint << "\n";
    f << "  }\n";
    f << "}\n";
}

static void print_table(const std::vector<BenchResult>& results) {
    std::printf("%-20s %8s %10s %12s %12s %12s %10s %10s %10s\n",
        "case", "payload", "samples", "ops/sec", "bytes/sec", "dur(ms)",
        "p50(us)", "p99(us)", "max(us)");
    std::printf("%-20s %8s %10s %12s %12s %12s %10s %10s %10s\n",
        "--------------------", "--------", "----------",
        "------------", "------------", "------------",
        "----------", "----------", "----------");
    for (const auto& r : results) {
        std::printf("%-20s %8d %10lld %12.0f %12.0f %12.1f %10.1f %10.1f %10.1f\n",
            r.name.c_str(), r.payload_bytes,
            static_cast<long long>(r.samples),
            r.ops_per_sec, r.bytes_per_sec,
            static_cast<double>(r.duration_ns) / 1e6,
            static_cast<double>(r.latency.p50_ns) / 1e3,
            static_cast<double>(r.latency.p99_ns) / 1e3,
            static_cast<double>(r.latency.max_ns) / 1e3);
    }
}

int main() {
    std::vector<BenchResult> results;
    uint16_t port = BASE_PORT;
    int failures = 0;

    auto run_case = [&](const char* label, auto fn) {
        std::fprintf(stderr, "  running %s ...\n", label);
        auto r = fn();
        if (r.samples < MIN_OPS) {
            std::fprintf(stderr, "  FAIL: %s completed only %lld ops (need %d)\n",
                label, static_cast<long long>(r.samples), MIN_OPS);
            ++failures;
        } else {
            std::fprintf(stderr, "  %s: %lld ops\n", label, static_cast<long long>(r.samples));
        }
        results.push_back(std::move(r));
    };

    // serialize: 0, 64, 1400 bytes
    for (int sz : {0, 64, 1400})
        run_case(("serialize/" + std::to_string(sz)).c_str(),
                 [&] { return bench_serialize(sz); });

    // udp_rpc: 64, 1400 bytes
    for (int sz : {64, 1400})
        run_case(("udp_rpc/" + std::to_string(sz)).c_str(),
                 [&] { return bench_udp_rpc(sz, port++); });

    // udp_notify: 64, 1400 bytes
    for (int sz : {64, 1400}) {
        auto p = port; port += 2;
        run_case(("udp_notify/" + std::to_string(sz)).c_str(),
                 [&, p] { return bench_udp_notify(sz, p, p + 1); });
    }

    // udp_rpc_demux: 64 bytes
    run_case("udp_rpc_demux/64",
             [&] { return bench_udp_rpc_demux(64, port++); });

    print_table(results);

    write_json("build/perf/host-bench.json", results);

    if (failures > 0) {
        std::fprintf(stderr, "\n%d case(s) failed minimum-ops threshold\n", failures);
        return 1;
    }

    std::printf("\nWrote build/perf/host-bench.json (%zu cases)\n", results.size());
    return 0;
}
