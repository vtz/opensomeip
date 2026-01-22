<!--
  Copyright (c) 2025 Vinicius Tadeu Zein

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
-->

# Documentation

This directory contains all documentation for the SOME/IP stack implementation, including architecture documents, design specifications, and user guides.

## Subdirectories

### `architecture/`
High-level architecture documentation including:
- System architecture overview
- Component interaction diagrams
- Safety architecture and fault containment
- Deployment models

### `design/`
Detailed design documentation including:
- Class diagrams and data structures
- Sequence diagrams for protocol flows
- State machines and algorithms
- Interface specifications

### `diagrams/`
PlantUML source files and generated diagrams:
- Architecture diagrams
- Sequence diagrams
- Class diagrams
- Component diagrams

### `requirements/`
Requirements documentation using Sphinx needs:
- Implementation-specific requirements (E2E plugin, transport, architecture)
- Traceability to Open SOME/IP Specification
- Code and test traceability annotations
- Automated traceability matrix generation

See [requirements/README.md](requirements/README.md) for details.

## Key Documents

- [Architecture Overview](architecture/overview.md)
- [Safety Design](architecture/safety_design.md)
- [Protocol Implementation](design/protocol_design.md)
- [API Reference](design/api_reference.md)
- [Requirements Management](requirements/README.md)
- [Traceability Guide](requirements/TRACEABILITY_GUIDE.md)

## Documentation Standards

All documentation follows these standards:
- Markdown format for readability
- PlantUML for diagrams
- Traceable to requirements
- Version controlled with implementation
