# SOME/IP Stack -- Zephyr RTOS Port Progress

## Resumo do Projeto

- **Branch**: `feature/zephyr-port`
- **Targets**: native_sim, mr_canhubk3 (S32K344), s32k388_renode (S32K388)
- **Status geral**: Implementacao completa -- aguardando validacao em Docker/Zephyr

## Decisoes Arquiteturais

### 1. Codebase unico vs fork

**Decisao**: Manter um unico codebase. Nao criar fork "opensomeip_lite".

**Justificativa**: As diferencas entre host e Zephyr se concentram em 4 headers de
abstracao de plataforma (`include/platform/`). O codigo de protocolo (~95% da codebase)
e identico em todas as plataformas. Um fork duplicaria toda a manutencao.

### 2. STL containers

**Decisao**: Manter `std::vector`, `std::string`, `std::shared_ptr`.

**Justificativa**: Funcionam no Zephyr com `CONFIG_STD_CPP17` e `CONFIG_NEWLIB_LIBC`.
Heap controlado via `CONFIG_HEAP_MEM_POOL_SIZE`.

**Alternativas descartadas**: iceoryx_hoofs (nao suporta Zephyr), containers proprios
(prematuro).

### 3. std::regex

**Decisao**: Removido. Substituido por parsing manual em `endpoint.cpp`.

**Justificativa**: Unica dependencia STL incompativel com RTOS (>100KB RAM). Usado
apenas para validacao IPv4/IPv6. Parsing manual e mais eficiente em todas as plataformas.

### 4. Abstracao de plataforma

**Decisao**: 4 headers minimos em `include/platform/`.

- `byteorder.h` -- htons/ntohs portavel (someip_htons, etc.)
- `net.h` -- sockets portavel (POSIX vs Zephyr BSD sockets)
- `thread.h` -- threading portavel (std::thread vs k_thread)
- `memory.h` -- pool allocator para Message objects

---

## Fase 0: Setup do Ambiente

### Status: done

### Decisoes tomadas

- Docker baseado em `ghcr.io/zephyrproject-rtos/ci:v0.27.4`
- Renode 1.15.3 pre-instalado no container
- Branch `feature/zephyr-port` criado a partir de `ci/skip-tests-on-docs-only`

### O que foi feito

- Criado `Dockerfile.zephyr` com Zephyr SDK + ARM toolchain + Renode
- Criado `docker-compose.zephyr.yml` com volume mount e NET_ADMIN
- Criado `zephyr/samples/net_test/` -- UDP echo para native_sim
- Criado `zephyr/samples/hello_s32k/` -- hello world para mr_canhubk3
- Criado `scripts/zephyr_build.sh` -- helper de build
- Criado `docs/ZEPHYR_PORT_PROGRESS.md` (este arquivo)

### Estado ao final da fase

- Build host: pass (11/11 testes)
- native_sim: infraestrutura criada (requer Docker para executar)
- mr_canhubk3: infraestrutura criada (requer Docker para executar)

---

## Fase 1: Integracao como Modulo Zephyr

### Status: done

### O que foi feito

- Criado `zephyr/module.yml` -- registro do modulo
- Criado `zephyr/CMakeLists.txt` -- build condicional via Kconfig
- Criado `zephyr/Kconfig` -- 12 opcoes de configuracao
- Criado `zephyr/prj.conf` -- config base (C++17, POSIX, networking)
- Criado `zephyr/boards/native_sim.conf` -- heap 256KB, pool 32
- Criado `zephyr/boards/mr_canhubk3.conf` -- heap 64KB, pool 8, sem TCP/E2E
- Criado `include/platform/byteorder.h` -- someip_htons/ntohs/htonl/ntohl

### Decisoes tomadas

- Kconfig entries para cada modulo (SD, RPC, Events, E2E, TP) permitem desabilitar
  features em targets com pouca RAM
- `byteorder.h` usa macros (`someip_htons`) em vez de inline functions para zero
  overhead em todas as plataformas

---

## Fase 2: Remover std::regex

### Status: done

### O que foi feito

- `src/transport/endpoint.cpp`: `is_valid_ipv4()` reescrito com parsing manual
  (split por '.', validar octetos 0-255, rejeitar leading zeros)
- `src/transport/endpoint.cpp`: `is_valid_ipv6()` reescrito com validacao manual
  (contagem de grupos, validacao de caracteres hex, suporte a '::')
- Removido `#include <regex>` de `endpoint.cpp`

### Erros encontrados e resolucoes

- Nenhum. Todos os 11 testes do host passaram apos a mudanca.

### Estado ao final da fase

- Build host: pass (11/11 testes, incluindo EndpointTest)

---

## Fase 3: Camada de Abstracao de Sockets

### Status: done

### O que foi feito

- Criado `include/platform/net.h` -- inclui headers de rede conforme plataforma
  (__ZEPHYR__, _WIN32, POSIX)
- Definidos helpers: `someip_close_socket()`, `someip_set_nonblocking()`,
  `someip_set_blocking()`
- `include/transport/udp_transport.h`: substituido `#include <netinet/in.h>` por
  `#include "platform/net.h"`
- `src/transport/udp_transport.cpp`: removidos includes POSIX diretos, substituidos
  `close()` por `someip_close_socket()`, `fcntl()` por `someip_set_nonblocking()`
- `src/transport/tcp_transport.cpp`: mesmas substituicoes
- `src/someip/message.cpp`: substituido bloco `#ifdef _WIN32` por
  `#include "platform/byteorder.h"`, todas as chamadas `htonl/ntohl` por `someip_*`
- `src/serialization/serializer.cpp`: idem
- `src/sd/sd_message.cpp`: idem
- `src/sd/sd_server.cpp`: idem
- `src/e2e/e2e_header.cpp`: idem
- `src/e2e/e2e_profiles/standard_profile.cpp`: idem
- `tests/test_sd.cpp`: idem

### Arquivos modificados (11 .cpp + 1 .h)

| Arquivo | Mudanca |
|---------|---------|
| `include/transport/udp_transport.h` | `netinet/in.h` -> `platform/net.h` |
| `src/transport/udp_transport.cpp` | POSIX includes -> `platform/net.h`, close -> someip_close_socket |
| `src/transport/tcp_transport.cpp` | POSIX includes -> `platform/net.h`, close/fcntl -> helpers |
| `src/someip/message.cpp` | `arpa/inet.h` -> `platform/byteorder.h`, htonl -> someip_htonl |
| `src/serialization/serializer.cpp` | idem |
| `src/sd/sd_message.cpp` | idem |
| `src/sd/sd_server.cpp` | idem |
| `src/e2e/e2e_header.cpp` | idem |
| `src/e2e/e2e_profiles/standard_profile.cpp` | idem |
| `tests/test_sd.cpp` | `arpa/inet.h` -> `platform/byteorder.h` |

### Estado ao final da fase

- Build host: pass (11/11 testes)
- Zero includes diretos de `arpa/inet.h`, `netinet/*.h`, `sys/socket.h`, `unistd.h`,
  `fcntl.h` nos fontes (apenas dentro de `platform/*.h` com guards)

---

## Fase 4: Threading Portavel

### Status: done

### O que foi feito

- Criado `include/platform/thread.h`:
  - Host/native_sim: aliases para std::thread, std::mutex, std::condition_variable
  - Embedded Zephyr: wrappers k_thread, k_mutex, k_condvar com API compativel
  - `platform::this_thread::sleep_for()` portavel (k_msleep no embedded)
  - `platform::ScopedLock` compativel com ambos
- Criado `src/platform/zephyr_thread.cpp` (stub, implementacao e inline no header)
- Substituido `std::thread`/`std::mutex`/`std::condition_variable` em **19 arquivos**:
  - 6 headers: `udp_transport.h`, `tcp_transport.h`, `session_manager.h`,
    `e2e_profile_registry.h`, `tp_reassembler.h`, `tp_manager.h`
  - 13 sources: `udp_transport.cpp`, `tcp_transport.cpp`, `session_manager.cpp`,
    `sd_server.cpp`, `sd_client.cpp`, `rpc_client.cpp`, `rpc_server.cpp`,
    `event_publisher.cpp`, `event_subscriber.cpp`, `e2e_profile_registry.cpp`,
    `standard_profile.cpp`, `tp_reassembler.cpp`, `tp_manager.cpp`
- Substituido `std::scoped_lock` -> `platform::ScopedLock` em todos os fontes
- Substituido `std::lock_guard<std::mutex>` -> `platform::ScopedLock` em e2e/sd
- Substituido `std::this_thread::sleep_for` -> `platform::this_thread::sleep_for`
- Substituido `std::future`/`std::promise` em `rpc_client.cpp` por `std::atomic` +
  `platform::Mutex` + polling com `platform::this_thread::sleep_for`
- Removidos todos os `#include <mutex>`, `#include <thread>`, `#include <condition_variable>`,
  `#include <future>` dos fontes (incluidos transitivamente via `platform/thread.h`)

### Decisoes tomadas

- `std::atomic` mantido (funciona em todas as plataformas, nao precisa de abstracao)
- `std::future`/`std::promise` eliminados completamente (dependencia do libstdc++
  que nao esta disponivel em todos os targets embedded). Substituido por polling com
  sleep de 1ms, aceitavel para RPC sync calls com latencia de rede muito maior.
- `platform::ConditionVariable` nao precisa de `wait()` no codebase atual (CVs sao
  usadas apenas para `notify_one()`), simplificando a API embedded.

### Estado ao final da fase

- Build host: pass (11/11 testes passam)
- Zero usos de `std::thread`/`std::mutex`/`std::condition_variable` fora de `platform/thread.h`

---

## Fase 5: Gestao de Memoria para Embedded

### Status: done

### O que foi feito

- Criado `include/platform/memory.h`:
  - Host: `allocate_message()` -> `std::make_shared<Message>()`
  - Embedded: pool via `k_mem_slab` com `CONFIG_SOMEIP_MESSAGE_POOL_SIZE` slots
- Criado `src/platform/zephyr_memory.cpp`:
  - Buffer estatico dimensionado por Kconfig
  - Custom deleter no shared_ptr para devolver ao slab
- Kconfig entries adicionadas em `zephyr/Kconfig`:
  - `SOMEIP_MESSAGE_POOL_SIZE`, `SOMEIP_MAX_PAYLOAD_SIZE`
  - `SOMEIP_MAX_SUBSCRIPTIONS`, `SOMEIP_MAX_PENDING_CALLS`
  - `SOMEIP_THREAD_STACK_SIZE`

---

## Fase 6: Board Definition S32K388 + Renode

### Status: done

### O que foi feito

- Criado `zephyr/boards/s32k388_renode/`:
  - `board.yml` -- metadata
  - `s32k388_renode.dts` -- Cortex-M7, 512KB SRAM, 8MB flash, GMAC, LPUART
  - `s32k388_renode_defconfig` -- configuracao base
  - `s32k388_renode.conf` -- SOME/IP config (heap 128KB, pool 16)
- Criado `zephyr/renode/s32k388_someip.resc` -- script Renode com GMAC + TAP
- Criado `scripts/run_renode_test.sh` -- build + execucao no Renode

---

## Fase 7: Porte dos Modulos de Alto Nivel

### Status: done

### O que foi feito

- `zephyr/CMakeLists.txt` ja inclui SD, RPC, Events condicionalmente via Kconfig
- Todos os fontes SD/RPC/Events ja usam `platform/net.h` transitivamente
  (via `transport/udp_transport.h`)
- Nenhuma modificacao adicional necessaria nos fontes

---

## Fase 8: Testes e Demonstracao

### Status: done

### O que foi feito

- Criado `zephyr/samples/someip_echo/` -- demo SOME/IP (serialize, deserialize, endpoint)
- Criado `zephyr/tests/test_core/` -- testes de Message, Endpoint, SessionManager, Serializer
- Criado `zephyr/tests/test_transport/` -- teste UDP loopback
- Criado `scripts/run_zephyr_tests.sh` -- orquestra build + execucao para cada target

---

## Fase 9: CI e Pull Request

### Status: done

### O que foi feito

- Criado `.github/workflows/zephyr.yml` com 4 jobs:
  1. `host-build` -- cmake build + ctest (ubuntu-latest)
  2. `zephyr-native-sim` -- build + runtime (container zephyrproject-rtos/ci)
  3. `zephyr-s32k344-build` -- cross-compile mr_canhubk3 (container)
  4. `zephyr-s32k388-renode` -- build s32k388_renode (container)

### Estado ao final da fase

- Build host: pass (11/11 testes)
- CI workflow: pronto para validacao no push/PR
