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

echo "Diagram generation complete!"
echo "PNG diagrams: $DIAGRAMS_DIR/png/"
echo "SVG diagrams: $DIAGRAMS_DIR/svg/"
