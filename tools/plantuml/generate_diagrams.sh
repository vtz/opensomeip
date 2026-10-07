#!/bin/bash

# SOME/IP Stack PlantUML Diagram Generator
# Generates PNG and SVG diagrams from PlantUML source files

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DIAGRAMS_DIR="$PROJECT_ROOT/docs/diagrams"
PLANTUML_JAR="$PROJECT_ROOT/tools/plantuml.jar"

# Check if PlantUML jar exists
if [ ! -f "$PLANTUML_JAR" ]; then
    echo "Error: PlantUML jar not found at $PLANTUML_JAR"
    echo "Please download PlantUML from https://plantuml.com/download"
    echo "and place it at $PLANTUML_JAR"
    exit 1
fi

# Create output directories
mkdir -p "$DIAGRAMS_DIR/png"
mkdir -p "$DIAGRAMS_DIR/svg"

echo "Generating PlantUML diagrams..."

# Generate PNG diagrams
echo "Generating PNG diagrams..."
java -jar "$PLANTUML_JAR" \
    -o "$DIAGRAMS_DIR/png" \
    -tpng \
    "$DIAGRAMS_DIR"/*.puml

# Generate SVG diagrams
echo "Generating SVG diagrams..."
java -jar "$PLANTUML_JAR" \
    -o "$DIAGRAMS_DIR/svg" \
    -tsvg \
    "$DIAGRAMS_DIR"/*.puml

# PlantUML names each image from the @startuml identifier, not the .puml
# filename. Docs link diagrams/svg/<basename>.svg, so the identifier must
# match the source basename (no spaces).
missing=0
for src in "$DIAGRAMS_DIR"/*.puml; do
    base="$(basename "$src" .puml)"
    if [ ! -s "$DIAGRAMS_DIR/svg/${base}.svg" ]; then
        echo "ERROR: missing $DIAGRAMS_DIR/svg/${base}.svg" >&2
        echo "       Set @startuml ${base} in $src" >&2
        missing=1
    fi
done
if [ "$missing" -ne 0 ]; then
    exit 1
fi

echo "Diagram generation complete!"
echo "PNG diagrams: $DIAGRAMS_DIR/png/"
echo "SVG diagrams: $DIAGRAMS_DIR/svg/"
