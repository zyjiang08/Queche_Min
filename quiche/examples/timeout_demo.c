// timeout_demo.c
// 演示 quiche_conn_timeout_as_nanos 的使用和返回值

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>

#include <quiche.h>

#define UINT64_MAX 18446744073709551615ULL

// 格式化纳秒为人类可读格式
void format_timeout(uint64_t timeout_ns, char *buf, size_t buf_len) {
    if (timeout_ns == UINT64_MAX) {
        snprintf(buf, buf_len, "UINT64_MAX (无需定时器)");
    } else if (timeout_ns == 0) {
        snprintf(buf, buf_len, "0 (已超时，需立即处理)");
    } else if (timeout_ns < 1000) {
        snprintf(buf, buf_len, "%" PRIu64 " 纳秒", timeout_ns);
    } else if (timeout_ns < 1000000) {
        snprintf(buf, buf_len, "%.3f 微秒 (%" PRIu64 " ns)",
                timeout_ns / 1000.0, timeout_ns);
    } else if (timeout_ns < 1000000000) {
        snprintf(buf, buf_len, "%.3f 毫秒 (%" PRIu64 " ns)",
                timeout_ns / 1000000.0, timeout_ns);
    } else {
        snprintf(buf, buf_len, "%.3f 秒 (%" PRIu64 " ns)",
                timeout_ns / 1000000000.0, timeout_ns);
    }
}

// 分析并打印超时信息
void analyze_timeout(quiche_conn *conn, const char *phase) {
    uint64_t timeout_ns = quiche_conn_timeout_as_nanos(conn);
    uint64_t timeout_ms = quiche_conn_timeout_as_millis(conn);

    char formatted[256];
    format_timeout(timeout_ns, formatted, sizeof(formatted));

    printf("\n========================================\n");
    printf("阶段: %s\n", phase);
    printf("========================================\n");
    printf("纳秒接口返回值: %s\n", formatted);
    printf("毫秒接口返回值: %" PRIu64 " ms\n", timeout_ms);

    printf("\n推荐处理:\n");
    if (timeout_ns == UINT64_MAX) {
        printf("  ✓ 停止或禁用定时器\n");
        printf("  ✓ 连接可能已关闭或无待处理事件\n");
    } else if (timeout_ns == 0) {
        printf("  ⚠ 超时已发生！\n");
        printf("  ✓ 立即调用 quiche_conn_on_timeout(conn)\n");
        printf("  ✓ 不要等待，马上处理\n");
    } else {
        printf("  ✓ 设置定时器为 %.3f 秒\n", timeout_ns / 1e9);
        printf("  ✓ 或 %" PRIu64 " 毫秒\n", timeout_ns / 1000000);
        printf("  ✓ 超时后调用 quiche_conn_on_timeout(conn)\n");
    }

    // 显示单位转换
    if (timeout_ns != UINT64_MAX && timeout_ns > 0) {
        printf("\n单位转换参考:\n");
        printf("  秒:   %.9f\n", timeout_ns / 1e9);
        printf("  毫秒: %" PRIu64 "\n", timeout_ns / 1000000);
        printf("  微秒: %" PRIu64 "\n", timeout_ns / 1000);
        printf("  纳秒: %" PRIu64 "\n", timeout_ns);
    }

    printf("\n");
}

// 演示 libev 风格的集成
void demo_libev_integration(quiche_conn *conn) {
    printf("===========================================\n");
    printf("示例代码: libev 集成\n");
    printf("===========================================\n\n");

    printf("```c\n");
    printf("// 在发送数据后更新定时器\n");
    printf("static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {\n");
    printf("    // ... 发送数据包 ...\n\n");

    uint64_t timeout_ns = quiche_conn_timeout_as_nanos(conn);

    printf("    uint64_t timeout_ns = quiche_conn_timeout_as_nanos(conn_io->conn);\n");
    printf("    // 当前返回值: %" PRIu64 "\n\n", timeout_ns);

    if (timeout_ns != UINT64_MAX) {
        double timeout_sec = timeout_ns / 1e9;
        printf("    if (timeout_ns != UINT64_MAX) {\n");
        printf("        double timeout_sec = timeout_ns / 1e9;\n");
        printf("        // timeout_sec = %.6f\n\n", timeout_sec);
        printf("        conn_io->timer.repeat = timeout_sec;\n");
        printf("        ev_timer_again(loop, &conn_io->timer);\n");
        printf("    } else {\n");
        printf("        // 此分支不执行（当前不是 UINT64_MAX）\n");
    } else {
        printf("    if (timeout_ns != UINT64_MAX) {\n");
        printf("        // 此分支不执行（当前是 UINT64_MAX）\n");
        printf("    } else {\n");
        printf("        ev_timer_stop(loop, &conn_io->timer);\n");
    }
    printf("    }\n");
    printf("}\n");
    printf("```\n\n");
}

// 演示 select 风格的集成
void demo_select_integration(quiche_conn *conn) {
    printf("===========================================\n");
    printf("示例代码: select/poll 集成\n");
    printf("===========================================\n\n");

    uint64_t timeout_ns = quiche_conn_timeout_as_nanos(conn);

    printf("```c\n");
    printf("uint64_t timeout_ns = quiche_conn_timeout_as_nanos(conn);\n");
    printf("// 当前值: %" PRIu64 "\n\n", timeout_ns);

    printf("struct timeval tv;\n");
    printf("struct timeval *tv_ptr = NULL;\n\n");

    if (timeout_ns != UINT64_MAX) {
        uint64_t sec = timeout_ns / 1000000000;
        uint64_t usec = (timeout_ns % 1000000000) / 1000;

        printf("if (timeout_ns != UINT64_MAX) {\n");
        printf("    tv.tv_sec = %" PRIu64 ";   // 秒部分\n", sec);
        printf("    tv.tv_usec = %" PRIu64 ";  // 微秒部分\n", usec);
        printf("    tv_ptr = &tv;\n");
        printf("    // select 将等待 %" PRIu64 ".%06" PRIu64 " 秒\n", sec, usec);
        printf("}\n");
    } else {
        printf("if (timeout_ns != UINT64_MAX) {\n");
        printf("    // 此分支不执行\n");
        printf("} else {\n");
        printf("    tv_ptr = NULL;  // 无限等待\n");
        printf("}\n");
    }

    printf("\nint ret = select(fd + 1, &readfds, NULL, NULL, tv_ptr);\n");
    printf("if (ret == 0) {\n");
    printf("    quiche_conn_on_timeout(conn);  // 处理超时\n");
    printf("}\n");
    printf("```\n\n");
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  quiche_conn_timeout_as_nanos() 返回值演示                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    // 创建配置
    quiche_config *config = quiche_config_new(0xbabababa);
    if (config == NULL) {
        fprintf(stderr, "Failed to create config\n");
        return 1;
    }

    // 设置基本参数
    quiche_config_set_max_idle_timeout(config, 5000);  // 5秒
    quiche_config_set_max_recv_udp_payload_size(config, 1350);
    quiche_config_set_initial_max_data(config, 10000000);
    quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(config, 1000000);
    quiche_config_set_initial_max_streams_bidi(config, 100);

    // 创建客户端连接 ID
    uint8_t scid[16];
    for (int i = 0; i < 16; i++) {
        scid[i] = (uint8_t)i;
    }

    // 创建虚拟地址
    struct sockaddr_in local_addr = {0};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct sockaddr_in peer_addr = {0};
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(443);
    peer_addr.sin_addr.s_addr = htonl(0x7f000001);  // 127.0.0.1

    // 创建连接
    quiche_conn *conn = quiche_connect(
        "localhost",
        scid, sizeof(scid),
        (struct sockaddr *)&local_addr, sizeof(local_addr),
        (struct sockaddr *)&peer_addr, sizeof(peer_addr),
        config
    );

    if (conn == NULL) {
        fprintf(stderr, "Failed to create connection\n");
        quiche_config_free(config);
        return 1;
    }

    // 场景 1: 连接刚创建（握手阶段）
    analyze_timeout(conn, "连接初始化（握手开始）");

    // 场景 2: 演示集成示例
    demo_libev_integration(conn);
    demo_select_integration(conn);

    // 场景 3: 模拟关闭连接
    quiche_conn_close(conn, true, 0, (uint8_t *)"demo", 4);
    analyze_timeout(conn, "连接正在关闭（Draining）");

    // 场景 4: 检查是否完全关闭
    if (quiche_conn_is_closed(conn)) {
        analyze_timeout(conn, "连接已完全关闭");
    }

    // 总结
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  关键要点                                                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("1. 三种返回值:\n");
    printf("   • 0              → 立即调用 on_timeout()\n");
    printf("   • 1 ~ MAX-1      → 设置定时器\n");
    printf("   • UINT64_MAX     → 停止定时器\n\n");

    printf("2. 必须检查 UINT64_MAX:\n");
    printf("   ❌ double t = timeout_as_nanos(conn) / 1e9;\n");
    printf("   ✅ if (timeout != UINT64_MAX) { ... }\n\n");

    printf("3. 超时后必须调用 on_timeout():\n");
    printf("   if (timeout_expired) {\n");
    printf("       quiche_conn_on_timeout(conn);\n");
    printf("   }\n\n");

    printf("4. 单位转换:\n");
    printf("   • 秒   = ns / 1e9\n");
    printf("   • 毫秒 = ns / 1000000\n");
    printf("   • 微秒 = ns / 1000\n\n");

    printf("详细文档: TIMEOUT_AS_NANOS_ANALYSIS.md\n");
    printf("快速参考: TIMEOUT_QUICK_REFERENCE.md\n\n");

    // 清理
    quiche_conn_free(conn);
    quiche_config_free(config);

    return 0;
}
