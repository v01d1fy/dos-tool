#ifndef DOS_TOOL_CONFIG_H
#define DOS_TOOL_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DURATION_SECONDS 300
#define MAX_THREADS          1000
#define DEFAULT_THREADS      100
#define DEFAULT_DURATION     30
#define DEFAULT_PORT         80
#define MAX_UDP_PAYLOAD      65507
#define DEFAULT_UDP_PAYLOAD  1024

#define TOOL_VERSION "1.0.0"
#define TOOL_NAME    "DoS Tool - C/C++/Rust Combined"

#ifdef __cplusplus
}
#endif

#endif /* DOS_TOOL_CONFIG_H */
