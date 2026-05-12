#!/usr/bin/env bash
# RT-safety hook — scans every C++ file edited under the audio path
# for forbidden patterns (alloc, lock, exceptions).
#
# Reads a JSON event on stdin (PostToolUse Edit/Write/MultiEdit).
# Output:
#   exit 0 + empty stderr : OK
#   exit 2 + stderr       : violation — the agent is blocked and sees the message

set -euo pipefail

# Read the event
event_json="$(cat)"

# Extract file_path (Edit/Write) or the first file_path (MultiEdit)
file_path="$(printf '%s' "$event_json" | jq -r '.tool_input.file_path // empty')"

# Skip if no file or non-source file
[[ -z "$file_path" ]] && exit 0
[[ ! -f "$file_path" ]] && exit 0

# Target: DSP code, processor, or tests that exercise the audio path
case "$file_path" in
    */source/dsp/*|*/source/PluginProcessor.cpp|*/source/PluginProcessor.h)
        ;;
    *)
        exit 0
        ;;
esac

# Only scan .cpp/.h/.hpp
case "$file_path" in
    *.cpp|*.h|*.hpp) ;;
    *) exit 0 ;;
esac

# Heuristic: anything under dsp/ or PluginProcessor is audio-path.
# `prepare` / `prepareToPlay` / `reset` are allowed to allocate -> we filter those lines out.

violations=()

scan() {
    local pattern="$1"
    local label="$2"
    # Filters applied to candidate lines (any match here = ignored):
    #   - line comments and doc-comment continuations
    #   - prepare()/reset() bodies (heuristic: line contains 'prepare(' or 'reset(')
    #   - JUCE factory functions called from message thread / host init, never the audio path:
    #     createEditor, createPluginFilter (JUCE_CALLTYPE-decorated factory)
    #   - return statements that allocate UI/Editor objects (UI thread)
    #   - explicit developer-asserted safety via // RT-OK: <reason>
    #     (use sparingly; reason MUST be present after the colon)
    local filter='^\s*[0-9]+:\s*//|^\s*[0-9]+:\s*\*|prepare\s*\(|reset\s*\(|createEditor|createPluginFilter|JUCE_CALLTYPE|new\s+Plugin(Editor|Processor)|RT-OK:'
    if grep -nP "\b${pattern}" "$file_path" | grep -vP "$filter" >/dev/null; then
        local hits
        hits="$(grep -nP "\b${pattern}" "$file_path" | grep -vP "$filter" | head -5)"
        violations+=("[$label]"$'\n'"$hits")
    fi
}

# Allocations interdites
scan 'new\s+[A-Za-z_]'           "alloc: new"
scan 'malloc'                     "alloc: malloc"
scan 'std::make_unique'           "alloc: make_unique"
scan 'std::make_shared'           "alloc: make_shared"
scan 'push_back'                  "alloc: push_back (resize possible)"
scan 'emplace_back'               "alloc: emplace_back (resize possible)"
scan '\.resize\('                 "alloc: vector::resize"

# Locks interdits
scan 'std::mutex'                 "lock: std::mutex"
scan 'std::lock_guard'            "lock: std::lock_guard"
scan 'std::unique_lock'           "lock: std::unique_lock"
scan 'juce::ScopedLock'           "lock: juce::ScopedLock"
scan 'juce::CriticalSection'      "lock: juce::CriticalSection"

# Exceptions interdites dans path audio
scan 'throw\s+'                   "exception: throw"

# std::string operations that allocate
if grep -nP '\bstd::string\b' "$file_path" | grep -vP '^\s*[0-9]+:\s*//|const\s+std::string\s*&' >/dev/null; then
    hits="$(grep -nP '\bstd::string\b' "$file_path" | grep -vP '^\s*[0-9]+:\s*//|const\s+std::string\s*&' | head -3)"
    violations+=("[alloc: std::string non-const&]"$'\n'"$hits")
fi

if [[ ${#violations[@]} -eq 0 ]]; then
    exit 0
fi

# Print to stderr — the agent will see it and must fix.
{
    echo "RT-safety violation in $file_path"
    echo "Forbidden patterns in the audio path (see .claude/rules/rt-safety.md):"
    echo
    for v in "${violations[@]}"; do
        echo "$v"
        echo
    done
    echo "If the allocation is inside prepare()/reset() it is fine — move it there."
    echo "Otherwise, refactor to pre-allocate at prepareToPlay."
} >&2

exit 2
