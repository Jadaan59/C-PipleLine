# Analyzer Plugin Pipeline

## Overview

This project is a simple, extensible text-processing pipeline. It dynamically loads one or more plugins at runtime and chains them together. Lines from standard input flow through the chain; each plugin transforms the text and forwards it to the next plugin. The special sentinel string `<END>` triggers a graceful drain and shutdown of the whole pipeline.

Key points:
- Plugins are shared libraries (`.so`) loaded at runtime.
- Each plugin owns a worker thread and a bounded queue.
- Synchronization uses a small monitor abstraction to avoid lost wakeups.
- Shutdown is coordinated with a sentinel and join-based teardown.

## Project Structure

- `main.c` — Loads plugins (via `dlopen`), wires the pipeline, reads stdin, and coordinates shutdown.
- `plugins/` — All plugin code and common runtime:
  - `plugin_common.c`, `plugin_common.h` — Shared plugin runtime: queue/thread lifecycle, attach/forward, logging, sentinel handling.
  - `sync/monitor.c`, `sync/monitor.h` — Minimal monitor (mutex + condition + latched signal).
  - `sync/consumer_producer.c`, `sync/consumer_producer.h` — Bounded producer–consumer queue built on monitors.
  - `logger.c`, `uppercaser.c`, `rotator.c`, `flipper.c`, `expander.c`, `typewriter.c` — Example plugins.
- `build.sh` — Builds the main binary and all plugins into `output/`.
- `output/` — Build artifacts: `analyzer` and `*.so` plugins (created by the build script).

## Runtime Flow and Sync

1. `main.c` loads `output/<plugin>.so` for each name passed on the command line and resolves the standard plugin symbols (`plugin_init`, `plugin_place_work`, `plugin_attach`, `plugin_wait_finished`, `plugin_fini`).
2. Each plugin calls `common_plugin_init(...)`, which:
   - Creates a bounded queue (`consumer_producer_*`).
   - Starts a worker thread that repeatedly `get`s from the queue, runs the plugin `process_func`, and forwards the result to the next stage.
3. Producers (`plugin_place_work`) call `consumer_producer_put`, which blocks when the queue is full using the `not_full` monitor. Consumers block on `consumer_producer_get` using the `not_empty` monitor.
4. When `<END>` reaches `plugin_place_work`, the common layer does not enqueue it. Instead, it calls `consumer_producer_signal_finished`, which sets `finished=1` and signals all monitors. Each worker thread drains remaining items, then forwards a single `<END>` downstream after its queue is empty.
5. `main.c` waits for completion by calling each plugin’s `plugin_wait_finished` (joins the worker thread) and then `plugin_fini` to release resources.

## Build and Run

- Platform: Linux (tested with Ubuntu 24.04). The recommended setup is to use the provided `Dockerfile` together with the `.devcontainer` configuration in VS Code for a consistent GCC and toolchain environment.

Quick start:
```sh
./build.sh
echo -e "hello\n<END>" | ./output/analyzer 20 uppercaser rotator logger
```

Usage:
```text
./output/analyzer <queue_size> <plugin1> ... <pluginN>
```

More ways to run:
- Piped input (batch):
  ```sh
  echo -e "<some Text>\n<some Text>\n...\n<END>" | ./output/analyzer <queue_size> <plugin1> ... <pluginN>
  ```
- Interactive input:
  ```sh
  ./output/analyzer <queue_size> <plugin1> ... <pluginN>
  # type your lines, press Enter after each
  # when done, type:
  <END>
  ```

## Notes

- Plugin `process_func` should return a heap-allocated string. The runtime forwards it to the next stage or frees it if there is no downstream plugin.
- Plugins are looked up in `output/` by name (e.g., `uppercaser` → `output/uppercaser.so`).
